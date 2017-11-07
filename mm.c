/*
 * This is an implementation of a segregated free list. Immediate 
 * coalescing is also used, so coalescing is done when mm_free is called.
 * For the free blocks, it uses block sizes from 2^5 to 2^15 
 * where blocks in the free list are a power of 2 but less than
 * the next level (e.g. block sizes of 32-63 are in 2^5). They 
 * are hashed in using a simple log hash, and then stored as
 * a linked list. The allocated blocks are an implementation of
 * a header, payload, and footer. The free blocks are using a header
 * footer, and 2 pointers, therefore the minimum block size is 32 bytes.
 * 
 * 
 * The allocator manipualtes the free list when mm_alloc, mm_realloc 
 * and, mm_free is called. When mm_alloc is called it first checks
 * the free list and looks for any block sizes that are greater than
 * its current size that will fit. If none are found, it will then
 * expand the heap to allocate the correct amount of space.
 * When mm_realloc is called it checks to see if the size passed in is 
 * to see if more space needs to be allocated. If not, it just returns 
 * the current pointer and frees any extra space. If it requires extra
 * space it will first coalesce with blocks to make sure that there is
 * no space, then if there is not enough space it will allocate
 * more memory on the heap. When mm_free is called, it will first
 * coalesce with blocks nearby and then add it to the segregated free
 * list. 
 * 
 * The segregated free list pointers are stored in a seg_list_arr. There
 * are 15 of these pointers, so the amount of global memory used is 
 * 120 bytes. 
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
    "gg",
    /* First member's full name */
    "Elton Wong",
    /* First member's email address */
    "eltong.wong@mail.utoronto.ca",
    /* Second member's full name (leave blank if none) */
    "Daiqing Li",
    /* Second member's email address (leave blank if none) */
    "daiqing.li@mail.utoronto.ca"
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

/* Data structure for segregated list
 * An array of pointers to doubly linked lists
 * will be used for each 'hash' value
 */
typedef struct seg_block{
	struct seg_block* next;
	struct seg_block* prev;
//	void* bp;
} seg_block;

//number of entries in seg_list_arr
#define NUM_KEYS 15
seg_block* seg_list_arr[NUM_KEYS];
/**********************************************************
 * log_hash
 * hashing function used to return the log base 2 of the 
 * input. Used to determine which entry of seg_list_arr
 * to store the free block. 
**********************************************************/
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
/**********************************************************
 * add_to_seg_list
 * This function adds a given block bp into the appropropriate
 * spot in the segregated free list determined by log_hash.
**********************************************************/
void add_to_seg_list(void* bp){
	size_t size = GET_SIZE(HDRP(bp));
	int index = log_hash(size);
	//seg list empty
	if(seg_list_arr[index] == NULL){
		seg_list_arr[index] = (seg_block*) bp;
		seg_list_arr[index]->next = NULL;
		seg_list_arr[index]->prev = NULL;
	}else{ 
		//seg list not empty, add to the front
		seg_list_arr[index]->prev = (seg_block*) bp;
		seg_list_arr[index]->prev->next = seg_list_arr[index];
		seg_list_arr[index]->prev->prev = NULL;
		seg_list_arr[index] = (seg_block*) bp;
	}
}
/**********************************************************
 * rm_from_seg_list_sp
 * This function removes a given block sp from the segregated
 * free list. This function is typically used when a block is
 * found in the list that can be used for allocation. There
 * are 4 cases:
 * 1) sp is the only block in the list
 * 2) sp is the head of the list
 * 3) sp is the middle of the list (adjacent blocks are free)
 * 4) sp is in the end of the list
 * All of which require different handling. 
**********************************************************/
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
}

/**********************************************************
 * mm_init
 * Initialize the heap, including "allocation" of the
 * prologue and epilogue. This is where the segregated free
 * list is first initialized, and all entries are NULL. 
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

     //initialize keys to null
     for (int i = 0; i < NUM_KEYS; i++){
    	 seg_list_arr[i] = NULL;
     }
     
     return 0;
 }

/**********************************************************
 * coalesce_seg
 * Covers the 4 cases discussed in the text:
 * - both neighbours are allocated
 * - the next block is available for coalescing
 * - the previous block is available for coalescing
 * - both neighbours are available for coalescing
 * Additionally, if blocks are coalesced, they are also 
 * properly removed and added back to the list to the appropriate
 * size. 
 **********************************************************/
void *coalesce_seg(void *bp)
{
    //mm_check();
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
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
       
        return (bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        size_t new_size = GET_SIZE(HDRP(PREV_BLKP(bp)));
        int index = log_hash(new_size);
        rm_from_seg_list_sp(index, (seg_block*) PREV_BLKP(bp));
    	size += new_size;
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));

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

// calculate block size after coalescing
size_t get_coalesce_size(void* bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    //printf("size: %ld\n", size);
    if (prev_alloc && next_alloc) {       /* Case 1 */
        return size;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        size_t new_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));
    	size += new_size;
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        size_t new_size = GET_SIZE(HDRP(PREV_BLKP(bp)));
    	size += new_size;
    }

    else {            /* Case 4 */
        size_t prev_size = GET_SIZE(HDRP(PREV_BLKP(bp)));
        size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));
        size += prev_size + next_size;
    }
    return size;
}

/**********************************************************
 * extend_heap_seg
 * Extend the heap by "words" words, maintaining alignment
 * requirements of course. Free the former epilogue block
 * and reallocate its new header
 **********************************************************/
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
 * find_fit_seg
 * Traverse the free list starting from key values then
 * moving onto linked list searching for a block to fit asize
 * Return NULL if no free blocks can handle that size
 * Assumed that asize is aligned
 **********************************************************/

void * find_fit_seg(size_t asize)
{
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
            	//the block can fit and also be broken up into 2 pieces
            	//for better memory usage
            	if(sp!=NULL){
                    bp = (void*) sp;
					//split up blocks into 2, the first part of the block
            		//is returned to the user, the second part is added
            		//to the free list. 
					rm_from_seg_list_sp(index, sp);
					size_t extra_size = GET_SIZE(HDRP(bp)) - asize;
					
					void* split_ptr = bp + asize;
					
					PUT(HDRP(bp), PACK(asize,0));
					PUT(FTRP(bp), PACK(asize,0));
					
					PUT(HDRP(split_ptr), PACK(extra_size,0));
					PUT(FTRP(split_ptr), PACK(extra_size,0));
					
					add_to_seg_list(split_ptr);
					
					return bp;
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

/**********************************************************
 * mm_malloc
 * Allocate a block of size bytes.
 * The type of search is determined by find_fit_seg
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

/**********************************************************
 * mm_realloc
 * size = 0 means that it's a free
 * if ptr is NULL, it means that it's a malloc
 * if ptr is a valid ptr, and size is > 0 there are 2 cases
 * Case 1: size > old_size and we need to expand the allocated mem
 * Case 2: size < old_size and we need to shrink the allocated mem
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
    size_t asize;
    size_t oldSize;

    oldSize = GET_SIZE(HDRP(oldptr));
    copySize = oldSize;
    if (size < oldSize)
        copySize = size;

    /* Adjust block size to include overhead and alignment reqs. */
	if (size <= DSIZE)
		asize = 2 * DSIZE;
	else
		asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    if (asize > oldSize) {
    	//Case 1: expand
       
        PUT(HDRP(oldptr),PACK(oldSize,0));
        PUT(FTRP(oldptr),PACK(oldSize,0));
    	size_t new_block_size = get_coalesce_size(oldptr);
    	
    	if (new_block_size >= asize) {
    	    //ptr may be a new location now, oldptr is still old location
    	    newptr = coalesce_seg(oldptr);

            //just copy over oldptr size without hp and fp, no need to expand heap
    		memmove(newptr, oldptr, oldSize - DSIZE);
    		
    		size_t extra_size = new_block_size - asize;
    		/*
    		PUT(HDRP(newptr),PACK(new_block_size,1));
    		PUT(FTRP(newptr),PACK(new_block_size,1));
    		return newptr;
    		*/
    		// further split, still need to test it
    		/*if (extra_size >= 2 * DSIZE) {
			    //add new header and footer and return
				PUT(HDRP(newptr),PACK(asize,1));
				PUT(FTRP(newptr),PACK(asize,1));
				
				void* split_ptr = newptr + asize;
				PUT(HDRP(split_ptr),PACK(extra_size,0));
				PUT(FTRP(split_ptr),PACK(extra_size,0));
				printf("1new block size: %d, asize: %d, old size: %d\n", new_block_size, asize, oldSize);
				add_to_seg_list(split_ptr);
                //mm_check();
    		}*/
            
        	//add new header and footer and return
        	PUT(HDRP(newptr),PACK(new_block_size,1));
        	PUT(FTRP(newptr),PACK(new_block_size,1));
            //mm_check();

            return newptr;
    		
    	}
        
    	//mm_check();
		//do the original realloc
		newptr = mm_malloc(asize);
		if (newptr == NULL)
		  return NULL;
		memmove(newptr, oldptr, copySize);
        mm_free(oldptr);
		//mm_check();
        return newptr;

    } else {
    	//Case 2: shrink 
    	size_t extra_size = oldSize - asize;
    	
    	if (extra_size >= 2 * DSIZE) {
    		//enforce at least 4 words are free
    		//adjust the header and ptr of new block
    		PUT(HDRP(oldptr),PACK(asize,1));
    		PUT(FTRP(oldptr),PACK(asize,1));
    		
    		//cut off free block and add to seg list
    		
    		newptr = oldptr + asize;
    		PUT(HDRP(newptr),PACK(extra_size,0));
    		PUT(FTRP(newptr),PACK(extra_size,0));
    		
    		add_to_seg_list(newptr);
    		
    		return oldptr;
    		
    	} else {
    		return oldptr;
    	}
    }
    
    return NULL;
}

/**********************************************************
 * mm_check
 * Check the consistency of the memory heap
 * Heap checker does the following: 
 * 
 * 1) check if every block in free list is marked as free
 * 
 * 2) check if any contiguous blocks escaped coalesce
 * 
 * 3) check if every free block is actually free
 * 
 * 4) check if pointers in free block point to valid free blocks
 * 
 * 5) check if any allocated blocks overlap
 * 
 * 6) check if pointers in a heap block point to a valid address
 * 
 * Return nonzero if the heap is consistant.
 *********************************************************/
int mm_check(void){
	
	//print out heap
	void* heap_start = heap_listp;
	
	printf("START OF HEAP \n");
	while(GET_SIZE(HDRP(heap_start)) != 0){
		int curr_alloc = GET_ALLOC(HDRP(heap_start));
		
		printf("Address: 0x:%x tSize: %d Allocated: %d\n",heap_start, GET_SIZE(HDRP(heap_start)), curr_alloc);
		
		heap_start = NEXT_BLKP(heap_start);
		
		//check for any blocks that escape coalescing
		if(curr_alloc == 0 && GET_ALLOC(HDRP(heap_start)) == 0){
			printf("block escaped coalescing, but this could be fine if this was called before coalesce\n");
		}
		
		//check for overlap between any blocks
		if(FTRP(heap_start) > HDRP(NEXT_BLKP(heap_start)) ){
			printf("THERE IS BLOCK OVERLAP at: %x\n",heap_start);
			return 0;
		}
	}
	printf("END OF HEAP \n");
	
	printf("START OF SEG LIST\n");
	//print out free list
	//and check to see if each block is free. 
	for (int i = 0; i < NUM_KEYS; i++){
		seg_block* traverse = seg_list_arr[i];
		printf("hash value: %d\n",i);
		while(traverse != NULL){
			
			int free_bit = GET_ALLOC(HDRP(traverse));
			
			printf("Address: 0x:%x tSize: %d Allocated: %d\n",traverse, GET_SIZE(HDRP(traverse)), free_bit);
			
			if(free_bit!=0){
				printf("free bit isn't 0. This could be fine depending on where mm_check() is called.\n");
			}
			
			traverse = traverse->next;
		}
		
	}
	
	
	printf("END OF SEG LIST\n");
  return 1;
}
