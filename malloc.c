/* Basic constants and macros.
 * Since we have to perform a lot of pointer manipulation, it's better to
 * abstract away some of the common commands to both make the code readable and
 * make it less of a headache to write
 */

#define WSIZE 4 /* Word size. Same as that of the header, footer, fwrd, bwrd pointers */
#define DSIZE 8 /* Double size. For convenience sake, instead of doing WSIZE*2 every time */
#define DEFAULT_CHUNKSIZE 4096 /* Default chunksize to increase the heap pointer by when we need more memory from the heap 1 */

#define MAX(x, y) ((x) > (y) ? (x) : (y)) /* Simple Max command */

/* This PACK macro create a single Word-size block containing both information on the size
 * and whether the block is allocated. This is possible since because all blocks are aligned
 * on 8-byte boundary, every block's size is a multiple of 8. In binary, this means the 3 least
 * significant bits will always be 0. We can use this to set the LSB to either 0 or 1 to 
 * store whether a block is allocated or free.
 */
#define PACK(size, alloc) ((size) | (alloc)) //1 is allocated. 0 is free.

/* These 2 macros help us read or write a value at a certain address p.
 * 
 * Note that the pointer is casted to be an unsigned int here, but casted to
 * be a void in all other cases. In other cases where we have to do pointer
 * arithmetic, a void pointer would do the job best (this is not legal in C,
 * though the compilers still understand it. Simply changing it to be char pointers
 * would do the same thing). This is due to how C handle pointer arithmetic.
 *
 * However, for reading/writing, we have to convert it to an unsigned int pointer
 * since it's invalid to dereference a typeless void pointer
 */
#define READ(p) (*(unsigned int*)(p))
#define WRITE(p, val) (*(unsigned int*)(p) = (val))

/* Given a metadata pointer (mp), or the pointer to the header/footer, return the size or the alloc info */
#define GET_SIZE(mp) (READ(mp) & ~0x7) /* 0x7 = 0b111 */
#define GET_ALLOC(mp) (READ(mp) & 0x1)

/* Given a block pointer (bp), or the pointer to the first block right after the header of a chunk,
 * return the next/prev pointers
 */
#define NEXTP(bp) ((void*)(bp))
#define PREVP(bp) ((void*)(bp) + WSIZE)

/* Given bp, calculate the pointer to the header/footer of the chunk */
#define HDRP(bp) ((void*)(bp) - WSIZE)
#define FTRP(bp) ((void*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) //-2*WSIZE to skip past the header of the next chunk too to get to the footer

/* Given bp, compute bp of the contiguous next and prev block */
#define NEXT_BLKP(bp) ((void*)(bp) + GET_SIZE((void*)bp - WSIZE))
#define PREV_BLKP(bp) ((void*)(bp) - GET_SIZE((void*)bp - DSIZE)) //getting size of prev block here to know how much to jump to reach previous block

/* forward declaration of helper functions */
static void* extend_heap(size_t words);
static void* find_fit(size_t asize);
static void* handle_free(void* bp);
static void handle_malloc(void* bp, size_t asize);
static void add_free(void* bp, void** free_list_root);
static void fb_patching(void* bp, void** free_list_root);

/* Explicit free list root is a static pointer */
static void* FREE_LIST_ROOT = NULL;

/* 
 * mm_init - initialize the malloc package.
 * This command is always called first before anything happens.
 * Here, we create a start/end block that is always marked as allocated to prevent having
 * to check whether the block is at the start/end of the heap while coalescing.
 */
int mm_init(void) {
    /* create initial pointer to empty heap */
    void* heap_listp = mem_sbrk(4*WSIZE);
    if (heap_listp == (void*) -1) return -1;

    //inserting start/end blocks. Called prolog/epilog in textbook
    WRITE(heap_listp, 0); //alignment padding
    WRITE(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); //start block header
    WRITE(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); //start block footer
    WRITE(heap_listp + (3*WSIZE), PACK(0, 1)); //end block header (for the next free block)

    return 0;
}


/* 
 * extend_heap - extend the heap by the number of words given.
 */
static void* extend_heap(size_t words) {
    void* bp;
    size_t size;

    //allocate an even number of words to maintain 8 bits alignment
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; //0 is false, so words % 2 == 0 eval to false
    bp = mem_sbrk(size);
    if (bp == (void*) -1) return NULL;

    //initialize free block header/footer
    WRITE(HDRP(bp), PACK(size, 0)); //new free header
    WRITE(FTRP(bp), PACK(size, 0)); //new free footer
    WRITE(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); //new epilogue header

    //coalesce if block previous of extension was free;
    return handle_free(bp);
}

/* 
 * mm_malloc - Allocate a block. Always allocate a block that is a multiple of the alignment (8 bits)
 */
void* mm_malloc(size_t size) {
    if (size == 0) return NULL;

    size_t asize; //adjusted block size

    //adjusting size to include overhead and alignment requirements
    if (size <= DSIZE) {
        asize = 2 * DSIZE; //minimum = header + footer (DSIZE) + size (<= DSIZE)
    } else {
        asize = ((size + DSIZE + (DSIZE - 1)) / DSIZE) * DSIZE;
        //formula from the book
        //i understand the idea but explaining it in words is hard
    }

    void* bp;
    //search the free list for a fit
    bp = find_fit(asize);
    if (bp != NULL) {
        handle_malloc(bp, asize); //if found, handle allocation, then return pointer to block
        return bp;
    };

    //no fit found. get more memory
    size_t extend_size = MAX(asize, DEFAULT_CHUNKSIZE);
    bp = extend_heap(extend_size/WSIZE);
    if (bp == NULL) return NULL; //getting more memory fail
    handle_malloc(bp, asize); //otherwise, handle allocation, then return pointer to block
    return bp;
}

/*
 * find_fit - given size of block we are allocating, find in the free list to see whether
 * there exists a free block large enough
 */
static void* find_fit(size_t asize) {
    void* curr_free = FREE_LIST_ROOT;
    //simple linked list traversal, checking whether the size is large enough
    while (curr_free != NULL) {
        size_t cf_size = GET_SIZE(HDRP(curr_free));
        if (cf_size < asize) {
            curr_free = READ(NEXTP(curr_free));
        }
        else {
            break;
        }
    }
    return curr_free;
}

/*
 * handle_malloc - handle the allocation of a block with size asize at address bp
 */
static void handle_malloc(void* bp, size_t asize) {
    size_t cf_size = GET_SIZE(HDRP(bp)); //get size of current free block
    size_t rem_size = cf_size - asize; //get remaining size after the block is allocated

    fb_patching(bp, &FREE_LIST_ROOT); //see explanation of what this does in the comment for the function.

    if (rem_size < DSIZE * 2) { //if the remainder is not enough to construct a free block, just return the whole block             
        //mark off malloc block with header/footer information
        WRITE(HDRP(bp), PACK(cf_size, 1));
        WRITE(FTRP(bp), PACK(cf_size, 1)); //ftrp calculation based on size from header
    } else { //otherwise, create a new free block from the remainder
        //mark off malloc block
        WRITE(HDRP(bp), PACK(asize, 1));
        WRITE(FTRP(bp), PACK(asize, 1));

        //construct new free block
        void* new_free = NEXT_BLKP(bp);
        WRITE(HDRP(new_free), PACK(rem_size, 0));
        WRITE(FTRP(new_free), PACK(rem_size, 0));

        //add new free block to beginning
        add_free(new_free, &FREE_LIST_ROOT);
    }
}

/*
 * mm_free - Freeing a block.
 * input is a pointer to a previously malloc'd block
 */
void mm_free(void *bp) {
    size_t size = GET_SIZE(HDRP(bp)); //get size of block to be freed

    WRITE(HDRP(bp), PACK(size, 0)); //set alloc bit to 0
    WRITE(FTRP(bp), PACK(size, 0));
    handle_free(bp); //handle coalescing, basically
}
/*
 * handle_free - handle coalescing and correct linking of free blocks
 */
static void* handle_free(void* bp) {
    void* prev_block = PREV_BLKP(bp); //get the immediate previous and next blocks
    void* next_block = NEXT_BLKP(bp);
    size_t prev_alloc = GET_ALLOC(FTRP(prev_block)); //get whether the prev/next block are free
    size_t next_alloc = GET_ALLOC(HDRP(next_block));
    size_t size = GET_SIZE(HDRP(bp)); //get size of current block

    if (prev_alloc && next_alloc) { //case 1: both prev/next blocks are alloc'd
        //do nothing
    }

    else if (prev_alloc && !next_alloc) { //case 2: prev alloc'd next free
        fb_patching(next_block, &FREE_LIST_ROOT);

        //coalescing next block and new block, updating new block's size
        size += GET_SIZE(HDRP(next_block));
        WRITE(HDRP(bp), PACK(size, 0));
        WRITE(FTRP(next_block), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc) { //case 3: prev free next alloc'd
        fb_patching(prev_block, &FREE_LIST_ROOT);

        //coalescing prev block and new block, updating new block's size
        size += GET_SIZE(HDRP(prev_block));
        WRITE(HDRP(prev_block), PACK(size, 0));
        WRITE(FTRP(bp), PACK(size, 0));
        bp = prev_block;
    }

    else { //case 4: both free
        //cut off both prev/next blocks
        fb_patching(prev_block, &FREE_LIST_ROOT);
        fb_patching(next_block, &FREE_LIST_ROOT);

        //coalescing
        size = size + GET_SIZE(HDRP(prev_block)) + GET_SIZE(HDRP(next_block));
        WRITE(HDRP(prev_block), PACK(size, 0));
        WRITE(FTRP(next_block), PACK(size, 0));
        bp = prev_block;
    }

    //add free block to beginning of list
    add_free(bp, &FREE_LIST_ROOT);

    return bp;
}

/*
 * add_free - add bp to beginning of free list
 */
static void add_free(void* bp, void** free_list_root) {
    WRITE(PREVP(bp), NULL); //set previous pointer to NULL (since we're adding it at root)
    //if free_list_root is not null, we need to make bp's next point to the old_root
    //otherwise (empty list), just set next to null
    if (*free_list_root) {
        void* root_next = READ(NEXTP(*free_list_root));
        if (root_next) WRITE(PREVP(root_next), bp);
        WRITE(NEXTP(bp), root_next);
    } else {
        WRITE(NEXTP(bp), NULL);
    }
    *free_list_root = bp;
}

/*
 * fb_patching - Patch the previous/next free blocks of bp together
 * Also update free_list_root to point at next free block from bp if bp is already root
 * Essentially, it's an utility for removing bp from the free list
 */
static void fb_patching(void* bp, void** free_list_root) {
    //getting the relevant blocks for pointer reallocating
    void* bp_prev = READ(PREVP(bp));
    void* bp_next = READ(NEXTP(bp));

    //rearranging prev next for prev block prev/next
    if (bp_prev) {
        WRITE(NEXTP(bp_prev), bp_next);
    }
    else {
        *free_list_root = bp_next;
    }
    if (bp_next) WRITE(PREVP(bp_next), bp_prev);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void* mm_realloc(void *ptr, size_t size) {
    //TBD
}
