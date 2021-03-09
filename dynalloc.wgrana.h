//
// Created by Will Grana on 11/4/20.
//

#ifndef DYNAMICALLOCATION_DYNALLOC_WGRANA_H
#define DYNAMICALLOCATION_DYNALLOC_WGRANA_H

typedef struct BlockListNodeStruct {
    int size;
    int start;
    struct BlockListNodeStruct *next;
} BlockListNode;

typedef struct {
    int capacity;
    BlockListNode *allocations;
    BlockListNode *holes;
} AllocInfo;

typedef enum AllocTypeEnum {
    ALLOC_BEST_FIT,
    ALLOC_WORST_FIT,
    ALLOC_FIRST_FIT
} AllocType;

int allocateBlock(int size, AllocType allocType, AllocInfo *allocInfo);

int deallocateBlock(int location, AllocInfo *allocInfo);

int addHole(int start, int size, AllocInfo *allocInfo);

int deleteHole(int start, AllocInfo *allocInfo);

int addAllocation(int start, int size, AllocInfo *allocInfo);

void updateHole(int requestSize, BlockListNode *holeNode, AllocInfo *info);

int validateAllocation(AllocInfo *info);

#endif //DYNAMICALLOCATION_DYNALLOC_WGRANA_H
