/*
 * This implementation replicates the implicit list implementation
 * provided in the textbook
 * "Computer Systems - A Programmer's Perspective"
 * Blocks are never coalesced or reused.
 * Realloc is implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "t",
    /* First member's full name */
    "elton wong",
    /* First member's email address */
    "eltong.wong@mail.utoronto.ca",
    /* Second member's full name (leave blank if none) */
    "e",
    /* Second member's email address (leave blank if none) */
    "e"
};

/*************************************************************************
 * Basic Constants and Macros
 * You are not required to use these macros but may find them helpful.
*************************************************************************/
#define WSIZE       sizeof(void *)            /* word size (bytes) */
#define DSIZE       (2 * WSIZE)            /* doubleword size (bytes) */
#define CHUNKSIZE   (1<<7)      /* initial heap size (bytes) */

#define MAX(x,y) ((x) > (y)?(x) :(y))
#define MIN(x,y) ((x) < (y)?(x) :(y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)          (*(uintptr_t *)(p))
#define PUT(p,val)      (*(uintptr_t *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     (GET(p) & ~(DSIZE - 1))
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)        ((char *)(bp) - WSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

int mm_check(void);

void* heap_listp = NULL;

// data structure heap pointer
//void* heap_list_segp = NULL;

void *mm_malloc_ori(size_t size);
void mm_free_ori(void* bp);

/* Data structure for segregated list
 * An array of pointers to doubly linked lists
 * will be used for each 'hash' value
 */
typedef struct seg_block{
	struct seg_block* next;
	struct seg_block* prev;
//	void* bp;
} seg_block;
/*
// build a new seg block using data structure heap mem
seg_block* build_block(void* bp, seg_block* next, seg_block* prev){
	seg_block* new_block = mm_malloc_ori(sizeof (seg_block));
	new_block->next = next;
	new_block->prev = prev;
	new_block->bp = bp;
	return new_block;
}
*/
//for now starting with only 10 values in key-value mapping
#define NUM_KEYS 15
seg_block* seg_list_arr[NUM_KEYS];

//returns log base 2 of size
int log_hash(size_t size){

	int index = 0;
	size_t val = 1;
	
	//keep shifting val to the left until it's bigger than size
	
	while(val < size) {
		val <<= 1;
		index++;
	}
		
	//min shifts is 5, because min seg block size is 32
	index = MAX(index, 5);
	index = MIN(index, NUM_KEYS-1);
	return index;
	
}

void add_to_seg_list(void* bp){
	size_t size = GET_SIZE(HDRP(bp));
	int index = log_hash(size);
	//seg_block* block = build_block(bp, seg_list_arr[index], NULL);
	//printf("adding to seg list at index: %d address %x\n",index,seg_list_arr[index]);
	//seg list empty
	if(seg_list_arr[index] == NULL){
		seg_list_arr[index] = (seg_block*) bp;
		seg_list_arr[index]->next = NULL;
		seg_list_arr[index]->prev = NULL;
	}else{ //seg list not empty, add to the front
		seg_list_arr[index]->prev = (seg_block*) bp;
		seg_list_arr[index]->prev->next = seg_list_arr[index];
		seg_list_arr[index]->prev->prev = NULL;
		seg_list_arr[index] = (seg_block*) bp;
	}
}

// remove block from seg_list given block pointer sp
void rm_from_seg_list_sp(int index, seg_block* sp){
	//case where there's just one block
	if(sp->prev == NULL && sp->next == NULL) {
		seg_list_arr[index] = NULL;
	}
    // sp is head
    else if (sp->prev == NULL && sp->next != NULL) {
        seg_list_arr[index] = sp->next;
        seg_list_arr[index]->prev = NULL;
    }
    // sp in the middle
    else if (sp->prev != NULL && sp->next != NULL) {
        sp->next->prev = sp->prev;
        sp->prev->next = sp->next;

    }
    // sp is tail
    else {
        sp->prev->next = NULL;

    }
    //null out sp
    sp->next = NULL;
    sp->prev = NULL;
    // free sp
    //mm_free_ori(sp);
}


// remove block from seg_list given index and bp

void rm_from_seg_list_bp(int index, void* bp) {
    // find bp block
    seg_block* cur_block = seg_list_arr[index];
    while (cur_block) {
        if (cur_block == bp) {
            break;
        }
        cur_block = cur_block->next;
    }

    // check if we find it
    if (cur_block) {
        rm_from_seg_list_bp(index, cur_block);
    } else {
        //printf("do not find a block at index %d\n", index);
    }
}

/**********************************************************
 * mm_init
 * Initialize the heap, including "allocation" of the
 * prologue and epilogue
 **********************************************************/
 int mm_init(void)
 {
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
            return -1;
     PUT(heap_listp, 0);                         // alignment padding
     PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));   // prologue header
     PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));   // prologue footer
     PUT(heap_listp + (3 * WSIZE), PACK(0, 1));    // epilogue header
     heap_listp += DSIZE;
/*
     // allocate mem for data structure
     if ((heap_list_segp = mem_sbrk(4*WSIZE)) == (void *)-1)
            return -1;
       PUT(heap_list_segp, 0);                         // alignment padding
       PUT(heap_list_segp + (1 * WSIZE), PACK(DSIZE, 1));   // prologue header
       PUT(heap_list_segp + (2 * WSIZE), PACK(DSIZE, 1));   // prologue footer
       PUT(heap_list_segp + (3 * WSIZE), PACK(0, 1));    // epilogue header
       heap_list_segp += DSIZE;
*/
     //initialize keys to null
     for (int i = 0; i < NUM_KEYS; i++){
    	 seg_list_arr[i] = NULL;
     }
     
     //int i = log_hash(100000000000);
     //printf("log_hash: %d\n",i);
     
     return 0;
 }

/**********************************************************
 * coalesce
 * Covers the 4 cases discussed in the text:
 * - both neighbours are allocated
 * - the next block is available for coalescing
 * - the previous block is available for coalescing
 * - both neighbours are available for coalescing
 **********************************************************/
void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    //printf("size: %ld\n", size);
    if (prev_alloc && next_alloc) {       /* Case 1 */
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        //size_t oldsize = size;
    	size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        return (bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        return (PREV_BLKP(bp));
    }

    else {            /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)))  +
            GET_SIZE(FTRP(NEXT_BLKP(bp)))  ;
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        return (PREV_BLKP(bp));
    }
}
// coaleasce func used by seg
void *coalesce_seg(void *bp)
{
    //printf("coalesce called\n");
    //mm_check();
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    //printf("size: %ld\n", size);
    if (prev_alloc && next_alloc) {       /* Case 1 */
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        size_t new_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));
        int index = log_hash(new_size);
        rm_from_seg_list_sp(index, (seg_block*) NEXT_BLKP(bp));
        
    	size += new_size;
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        //configure seg list to move the coalesced blocks together
        //can either divide up the blocks into further blocks to try to get good fit
        //or just leave it and potentially get higher internal fragmentation
        // remove next block from seg list

        return (bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        size_t new_size = GET_SIZE(HDRP(PREV_BLKP(bp)));
        int index = log_hash(new_size);
        rm_from_seg_list_sp(index, (seg_block*) PREV_BLKP(bp));
    	size += new_size;
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        // remove prev block from seg list

        return (PREV_BLKP(bp));
    }

    else {            /* Case 4 */
        size_t prev_size = GET_SIZE(HDRP(PREV_BLKP(bp)));
        size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));
        int index = log_hash(prev_size);
        rm_from_seg_list_sp(index, (seg_block*) PREV_BLKP(bp));
        
        index = log_hash(next_size);
        rm_from_seg_list_sp(index, (seg_block*) NEXT_BLKP(bp));
        size += prev_size + next_size;
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));

        return (PREV_BLKP(bp));
    }
}


/**********************************************************
 * extend_heap
 * Extend the heap by "words" words, maintaining alignment
 * requirements of course. Free the former epilogue block
 * and reallocate its new header
 **********************************************************/
void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignments */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ( (bp = mem_sbrk(size)) == (void *)-1 )
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));                // free block header
    PUT(FTRP(bp), PACK(size, 0));                // free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));        // new epilogue header

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

// extend_heap used by segregated list
void *extend_heap_seg(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignments */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ( (bp = mem_sbrk(size)) == (void *)-1 )
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));                // free block header
    PUT(FTRP(bp), PACK(size, 0));                // free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));        // new epilogue header

    /* Coalesce if the previous block was free */
    return coalesce_seg(bp);
    //return bp;
}
/**********************************************************
 * find_fit
 * Traverse the heap searching for a block to fit asize
 * Return NULL if no free blocks can handle that size
 * Assumed that asize is aligned
 **********************************************************/
void * find_fit(size_t asize)
{
	//change this to just use log_hash to find if there is a free block
	//if there isn't enough space, alloc on heap more space
    void *bp;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
        {
            return bp;
        }
    }
    
    return NULL;
}

// find_fit for segregated list

void * find_fit_seg(size_t asize)
{
    //printf("in find_fit_seg\n");
	int index = log_hash(asize);
    void* bp;
    seg_block* sp = NULL;
    while(index < NUM_KEYS){
    	if(seg_list_arr[index] != NULL){
    		sp = seg_list_arr[index];
            while (sp != NULL) {
                bp = (void*) sp;
                if (asize <= GET_SIZE(HDRP(bp))) {
                    break;
                }
                sp = sp->next;
            }
            //found block fit but can't break up block into more pieces
            if (sp != NULL && (asize + 2*DSIZE) > GET_SIZE(HDRP(bp)) ) {
                //rm from seg list
    		    bp = (void*) sp;
    		    rm_from_seg_list_sp(index, sp);
    		    return bp;
            }else{
            	if(sp!=NULL){
					//split up blocks into 2
            		//printf("special split:\n");
					rm_from_seg_list_sp(index, sp);
					int extra_size = GET_SIZE(HDRP(bp)) - asize;
					
					void* malloc_ptr = bp + extra_size;
					
					PUT(HDRP(malloc_ptr), PACK(asize,0));
					PUT(FTRP(malloc_ptr), PACK(asize,0));
					
					PUT(HDRP(bp), PACK(extra_size,0));
					PUT(FTRP(bp), PACK(extra_size,0));
					
					add_to_seg_list(bp);
					
					return malloc_ptr;
            	}
            }
    	}
    	index++;
    }
    
    return NULL;
}

/**********************************************************
 * place
 * Mark the block as allocated
 **********************************************************/
void place(void* bp, size_t asize)
{
  /* Get the current block size */
  size_t bsize = GET_SIZE(HDRP(bp));

  PUT(HDRP(bp), PACK(bsize, 1));
  PUT(FTRP(bp), PACK(bsize, 1));
}


/**********************************************************
 * mm_free
 * Free the block and coalesce with neighbouring blocks
 **********************************************************/
void mm_free(void *bp)
{
    //fix the blocks in the coalesce function
	//printf("free called\n");
	if(bp == NULL){
      return;
    }
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    bp = coalesce_seg(bp);
    
    //mm_check();
    
    add_to_seg_list(bp);
    
    //mm_check();
}

// original method
void mm_free_ori(void *bp)
{
    //fix the blocks in the coalesce function
	if(bp == NULL){
      return;
    }
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    coalesce(bp);
}

/**********************************************************
 * mm_malloc
 * Allocate a block of size bytes.
 * The type of search is determined by find_fit
 * The decision of splitting the block, or not is determined
 *   in place(..)
 * If no block satisfies the request, the heap is extended
 **********************************************************/
void *mm_malloc(size_t size)
{
    size_t asize; /* adjusted block size */
    size_t extendsize; /* amount to extend heap if no fit */
    char * bp;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);
    //printf("The asize is: %d\n",asize);
    /* Search the free list for a fit */
    if ((bp = find_fit_seg(asize)) != NULL) {
        place(bp, asize);
        //mm_check();
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap_seg(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    //mm_check();
    return bp;

}

// orignal method
void *mm_malloc_ori(size_t size)
{
    size_t asize; /* adjusted block size */
    size_t extendsize; /* amount to extend heap if no fit */
    char * bp;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;

}

/**********************************************************
 * mm_realloc
 * Implemented simply in terms of mm_malloc and mm_free
 *********************************************************/
void *mm_realloc(void *ptr, size_t size)
{
    //printf("realloc: %d",size);
	/* If size == 0 then this is just free, and we return NULL. */
    if(size == 0){
      mm_free(ptr);
      return NULL;
    }
    /* If oldptr is NULL, then this is just malloc. */
    if (ptr == NULL)
      return (mm_malloc(size));

    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;

    /* Copy the old data. */
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

/**********************************************************
 * mm_check
 * Check the consistency of the memory heap
 * Return nonzero if the heap is consistant.
 *********************************************************/
int mm_check(void){
	
	//print out heap
	void* heap_start = heap_listp;
	
	printf("START OF HEAP \n");
	while(GET_SIZE(HDRP(heap_start)) != 0){
		printf("Address: 0x:%x tSize: %d Allocated: %d\n",heap_start, GET_SIZE(HDRP(heap_start)), GET_ALLOC(HDRP(heap_start)));
		heap_start = NEXT_BLKP(heap_start);
	}
	printf("END OF HEAP \n");
	
	printf("START OF SEG LIST\n");
	//print out free list
	for (int i = 0; i < NUM_KEYS; i++){
		seg_block* traverse = seg_list_arr[i];
		printf("hash value: %d\n",i);
		while(traverse != NULL){
			
			printf("Address: 0x:%x tSize: %d Allocated: %d\n",traverse, GET_SIZE(HDRP(traverse)), GET_ALLOC(HDRP(traverse)));
			
			traverse = traverse->next;
		}
		
	}
	
	printf("END OF SEG LIST\n");
  return 1;
}
