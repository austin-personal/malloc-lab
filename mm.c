/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer. A block is pure payload. There are no headers or
 * footers. Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

team_t team = {
    "Jungle",
    "austin",
    "austin@jungle.com",
    "",
    ""
};

// Macro Set
#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12) // 4096 바이트(4KB)는 일반적으로 많은 시스템에서 페이지 크기
// List Limit
#define LISTLIMIT 20
#define MAX(x, y) ((x) > (y)? (x):(y))
#define PACK(size, alloc) ((size) | (alloc))
#define GET(p)     (*(unsigned int *)(p))
#define PUT(p,val) (*(unsigned int *)(p) = (val))
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))
#define PRED_FREE(bp) (*(void**)(bp))
#define SUCC_FREE(bp) (*(void**)(bp + WSIZE))

// Global Variable
static void *heap_listp;
// 분리 저장 리스트
static void *segregation_list[LISTLIMIT];

// Functions Prototype
static void *extend_heap(size_t words); // 힙구조에 추가적인 메모리 할당 요청 하는 함수
static void *coalesce(void *bp); //free가 됬을때, 인접 가용 블럭과 합치는 함수
static void *find_fit(size_t asize); // 맞당한 가용블럭을 찾는 함수
static void place(void *bp, size_t asize); // 맞당한 가용 블럭에 할당하는 함수
static void remove_block(void *bp);// 
static void insert_block(void *bp, size_t size);

// Heap init
int mm_init(void)
{
    int list;
    // 총 LISTLIMIT 만큼의 리스트 초기화 하기 (만들기)
    for (list = 0; list < LISTLIMIT; list++) {
        segregation_list[list] = NULL;
    } 
    // 힙구조 4*WSIZE 바이트 만큼 할당하고 포인터 반환하긱 with 예외 처리
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
    {
        return -1;
    }
    // Prologue, epilogue, Unused 할당하기 (힙구조 초기화)
    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
    heap_listp = heap_listp + 2*WSIZE;
    
    // 초기 가용 블록을 생성 (CHUNKSIZE/WSIZE 워드 크기의 가용 블록을 생성)
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) //Chunk size는 바이트 단위. Extendheap은 워드 단위로 받음
    {
        return -1;
    }
    return 0;
}

void *mm_malloc(size_t size)
{
    // 할당할 메모리 블록 (8의 배수로 정렬)
    int asize = ALIGN(size + SIZE_T_SIZE);
    size_t extendsize;
    char *bp;

    // 찾으면 할당 함수 호출
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    // If 가용블럭을 찾지 못 했을경우 추가적인 메모리 요구
    extendsize = MAX(asize, CHUNKSIZE); //더 큰거 할당
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
    {
        return NULL;
    }
    place(bp, asize);
    return bp;
}

void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    coalesce(ptr);
}

void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
    {
        return NULL;
    }
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    // Case 1
    if (prev_alloc && next_alloc){
        insert_block(bp, size);      
        return bp;
    }
    // Case 2
    else if (prev_alloc && !next_alloc)
    {
        remove_block(NEXT_BLKP(bp));

        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    // Case 3
    else if (!prev_alloc && next_alloc)
    {
        remove_block(PREV_BLKP(bp));

        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    // Case 4
    else if (!prev_alloc && !next_alloc)
    {
        remove_block(PREV_BLKP(bp));
        remove_block(NEXT_BLKP(bp));

        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    insert_block(bp, size);
    return bp;
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    remove_block(bp);
    if ((csize - asize) >= (2 * DSIZE))
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        coalesce(bp);
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

static void *find_fit(size_t asize) {
    void *bp;
    int list = 0;
    size_t searchsize = asize;

    // segregation list를 순회하며 적절한 크기의 블록을 찾음
    while (list < LISTLIMIT) {
        // 마지막 리스트이거나, 현재 리스트의 크기가 요청 크기보다 크거나 같고 비어있지 않은 경우
        if ((list == LISTLIMIT-1) || ((searchsize <= 1) && (segregation_list[list] != NULL))) {
            bp = segregation_list[list];

            // 현재 리스트 내에서 적절한 크기의 블록을 찾음
            while ((bp != NULL) && (asize > GET_SIZE(HDRP(bp)))) {
                bp = SUCC_FREE(bp);
            }
            // 적절한 블록을 찾았다면 반환
            if (bp != NULL) {
                return bp;
            }
        }
        searchsize >>= 1;
        list++;
    }

    return NULL;
}
// 가용 블록 리스트에서 특정 블록을 제거하는 역할
static void remove_block(void *bp){
    int list = 0;
    size_t size = GET_SIZE(HDRP(bp));

    // 블록이 속한 리스트 인덱스 찾기
    while ((list < LISTLIMIT - 1) && (size > 1)) {
        size >>= 1;
        list++;
    }

    // 후속 블록이 있는 경우
    if (SUCC_FREE(bp) != NULL){
        // 선행 블록도 있는 경우 (중간 블록 제거)
        if (PRED_FREE(bp) != NULL){
            PRED_FREE(SUCC_FREE(bp)) = PRED_FREE(bp);
            SUCC_FREE(PRED_FREE(bp)) = SUCC_FREE(bp);
        }
        // 선행 블록이 없는 경우 (첫 번째 블록 제거)
        else{
            PRED_FREE(SUCC_FREE(bp)) = NULL;
            segregation_list[list] = SUCC_FREE(bp);
        }
    }
    // 후속 블록이 없는 경우
    else{
        // 선행 블록이 있는 경우 (마지막 블록 제거)
        if (PRED_FREE(bp) != NULL){
            SUCC_FREE(PRED_FREE(bp)) = NULL;
        }
        // 선행 블록도 없는 경우 (유일한 블록 제거)
        else{
            segregation_list[list] = NULL;
        }
    }
    return;
}

static void insert_block(void *bp, size_t size){
    // 삽입할 블록의 크기에 따라 적절한 segregation list를 찾기.
    // 선택된 리스트 내에서 크기 순으로 적절한 삽입 위치를 찾기.
    // 4가지 경우: 
        // a. 리스트 중간에 삽입 
        // b. 리스트 맨 앞에 삽입
        // c. 리스트 맨 뒤에 삽입
        // d. 빈 리스트에 삽입
    int list = 0;
    void *search_ptr;
    void *insert_ptr = NULL;
    size_t org_size = size;

    // 블록 크기에 맞는 리스트 찾기
    while ((list < LISTLIMIT - 1) && (size > 1)){
        size >>= 1;
        list++;
    }

    // 리스트 내에서 적절한 삽입 위치 찾기
    search_ptr = segregation_list[list];
    while ((search_ptr != NULL) && (org_size > GET_SIZE(HDRP(search_ptr)))){
        insert_ptr = search_ptr;
        search_ptr = SUCC_FREE(search_ptr);
    }

    // 삽입 위치에 따라 포인터 조정
    if (search_ptr != NULL){
        if (insert_ptr != NULL){
            // 리스트 중간에 삽입
            SUCC_FREE(bp) = search_ptr;
            PRED_FREE(bp) = insert_ptr;
            PRED_FREE(search_ptr) = bp;
            SUCC_FREE(insert_ptr) = bp;
        }else{
            // 리스트 맨 앞에 삽입
            SUCC_FREE(bp) = search_ptr;
            PRED_FREE(bp) = NULL;
            PRED_FREE(search_ptr) = bp;
            segregation_list[list] = bp;
        }
    }else{
        if (insert_ptr != NULL){
            // 리스트 맨 뒤에 삽입
            SUCC_FREE(bp) = NULL;
            PRED_FREE(bp) = insert_ptr;
            SUCC_FREE(insert_ptr) = bp;
        }else{
            // 빈 리스트에 삽입
            SUCC_FREE(bp) = NULL;
            PRED_FREE(bp) = NULL;
            segregation_list[list] = bp;
        }
    }
    return;
}