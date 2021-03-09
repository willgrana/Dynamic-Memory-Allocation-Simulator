#include "dynalloc.wgrana.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define CAPACITY 2000
#define MEAN_REQUEST_SIZE 25
#define MIN_CYCLES 1
#define MAX_CYCLES 1000
#define CYCLE_INCREMENT 1

// error code
#define NO_FIT_FOUND -1

void runTests(AllocType type, char *filenameMeanHole, char *filenameElementsInUse);
void runTest(AllocType type, int numCycles, FILE *fpMean, FILE *fpElements);
int initializeAllocations(AllocInfo *info);
int getRandomSize();
void debugInfo(AllocInfo *info);

double getElementsInUseRatio(AllocInfo *info);
double getMeanHoleSize(AllocInfo *info);

AllocInfo *newAllocInfo(int capacity);
void freeAllocInfo(AllocInfo *info);

int main()
{
    // Initialize rand()
    srand(time(NULL));

    // Run tests on each allocation type
    runTests(ALLOC_BEST_FIT, "best_fit_mean_hole_size.csv", "best_fit_elements_in_use.csv");
    runTests(ALLOC_WORST_FIT, "worst_fit_mean_hole_size.csv", "worst_fit_elements_in_use.csv");
    runTests(ALLOC_FIRST_FIT, "first_fit_mean_hole_size.csv", "first_fit_elements_in_use.csv");
}

void runTests(AllocType type, char *filenameMeanHole, char *filenameElementsInUse)
{
    FILE *fpMean = fopen(filenameMeanHole, "w");
    FILE *fpElements = fopen(filenameElementsInUse, "w");
    // Run tests for the set cycles
    for (int i=MIN_CYCLES; i<=MAX_CYCLES; i+=CYCLE_INCREMENT)
        runTest(type, i, fpMean, fpElements);
    fclose(fpMean);
    fclose(fpElements);
}

void runTest(AllocType type, int numCycles, FILE *fpMean, FILE *fpElements)
{
    AllocInfo *allocInfo = newAllocInfo(CAPACITY);    

    int numAllocations = initializeAllocations(allocInfo);
    // debugInfo(allocInfo);

    double meanHoleSizes = 0;
    double elementsInUsage = 0;

    int successCount = 0;
    int failureCount = 0;
    for (int i=0; i<numCycles; i++)
    {
        if (numAllocations > 0)
        {
            // Deallocate one of the blocks at random
            int deallocateIndex = rand() % numAllocations;
            BlockListNode *node = allocInfo->allocations;
            for (int j=0; j<deallocateIndex; j++)
            {
                if (node == NULL)
                {
                    printf("Error in deallocation - not that many items (j:%d, i:%d, s:%d)!\n", j, deallocateIndex, numAllocations);
                    break;
                }
                node = node->next;
            }
            if (node == NULL && allocInfo->allocations != NULL)
                node = allocInfo->allocations;
            if (node == NULL)
                continue;
            if (deallocateBlock(node->start, allocInfo) == 0)
                numAllocations--;
        }
        // Allocate a block of random size
        if (allocateBlock(getRandomSize(), type, allocInfo) == NO_FIT_FOUND)
            failureCount++;
        else
        {
            successCount++;
            numAllocations++;
        }
        // Validate the allocation structures
        validateAllocation(allocInfo);
        // debugInfo(allocInfo);

        elementsInUsage += getElementsInUseRatio(allocInfo);
        meanHoleSizes += getMeanHoleSize(allocInfo);
    }

    fprintf(fpElements, "%d,%f\n", numCycles, elementsInUsage / numCycles);
    fprintf(fpMean, "%d,%f\n", numCycles, meanHoleSizes / numCycles);

    freeAllocInfo(allocInfo);
}

int initializeAllocations(AllocInfo *info)
{
    int count = 0;
    int failCount = 0;
    // CAPACITY / MEAN_REQUEST_SIZE allocations
    for (int i=0; i<CAPACITY/MEAN_REQUEST_SIZE; i++)
    {
        if (allocateBlock(getRandomSize(), ALLOC_FIRST_FIT, info) != NO_FIT_FOUND)
            count++;
        else
            failCount++;
    }

    return count;
}

int getRandomSize()
{
    // Random size in [1, 2*(Mean request size)]
    return rand() % (MEAN_REQUEST_SIZE * 2) + 1;
}

int allocateBlock(int size, AllocType allocType, AllocInfo *allocInfo)
{
    // printf("Allocating block of size %d...", size);
    // Need to declare variables outside of switch statement
    BlockListNode *node;
    BlockListNode *bestFit;
    BlockListNode *worstFit;
    switch (allocType)
    {
        case ALLOC_BEST_FIT:
            node = allocInfo->holes;
            bestFit = node;
            // Search all holes until the best possible match is found
            while (node != NULL)
            {
                // If the current 'best fit' is invalid or if the current node is better
                if (node->size >= size && (bestFit->size < size || node->size < bestFit->size))
                {
                    bestFit = node;
                    // End loop early if we can
                    if (bestFit->size == size)
                        break;
                }
                node = node->next;
            }
            if (bestFit != NULL && bestFit->size >= size)
            {
                int start = bestFit->start;
                updateHole(size, bestFit, allocInfo);
                // printf("Allocated at %d\n", start);
                return addAllocation(start, size, allocInfo);
            }
            break;
        case ALLOC_WORST_FIT:
            node = allocInfo->holes;
            worstFit = node;
            // Search all holes until worst possible match is found
            while (node != NULL)
            {
                // Check if current node is 'worse'
                if (node->size >= size && node->size > worstFit->size)
                    worstFit = node;
                node = node->next;
            }
            if (worstFit != NULL && worstFit->size >= size)
            {
                int start = worstFit->start;
                updateHole(size, worstFit, allocInfo);
                // printf("Allocated at %d\n", start);
                return addAllocation(start, size, allocInfo);
            }
            break;
        case ALLOC_FIRST_FIT:
            node = allocInfo->holes;
            // Search holes until the first possible match is found
            while (node != NULL)
            {
                if (node->size >= size)
                {
                    int start = node->start;
                    updateHole(size, node, allocInfo);
                    // printf("Allocated at %d\n", start);
                    return addAllocation(start, size, allocInfo);
                }
                node = node->next;
            }
            break;
    }
    // printf("Could not allocate\n");
    return NO_FIT_FOUND;
}

int deallocateBlock(int location, AllocInfo *allocInfo)
{
    // printf("Deallocating block at %d...", location);
    if (allocInfo->allocations == NULL)
    {
        // printf("No block found\n");
        return NO_FIT_FOUND;
    }

    BlockListNode *node = allocInfo->allocations;
    BlockListNode *prevReference = NULL;
    while (node != NULL)
    {
        if (node->start == location)
            break;
        prevReference = node;
        node = node->next;
    }
    if (node == NULL || node->start != location)
    {
        // printf("No block found\n");
        return NO_FIT_FOUND;
    }

    if (prevReference == NULL)
        allocInfo->allocations = node->next;
    else
        prevReference->next = node->next;

    // Collapse contiguous holes
    int start = location;
    int size = node->size;
    BlockListNode *hole = allocInfo->holes;
    while (hole != NULL)
    {
        // Get this early in case we free up node
        BlockListNode *next = hole->next;
        // Check for previous contiguous hole
        if (hole->start + hole->size == node->start)
        {
            start = hole->start;
            size += hole->size;
            deleteHole(start, allocInfo);
        }
        // Check for next contiguous hole
        else if (node->start + node->size == hole->start)
        {
            size += hole->size;
            deleteHole(hole->start, allocInfo);
        }
        hole = next;
    }

    addHole(start, size, allocInfo);
    free(node);
    // printf("Block deallocated.\n");
    return 0;
}

/**
 * Adds a new hole to the linked list of holes in the given info struct.
 */
int addHole(int start, int size, AllocInfo *info)
{
    // Need to make a new node for the holes list
    BlockListNode *newBlock = malloc(sizeof(BlockListNode));
    newBlock->start = start;
    newBlock->size = size;

    // Need to insert the new node into the linked list
    BlockListNode *node = info->holes;
    if (node == NULL)
    {
        // New block is the first element in the list
        info->holes = newBlock;
        newBlock->next = NULL;
        return start;
    }
    if (node->start > start)
    {
        // New block should be first element in list
        info->holes = newBlock;
        newBlock->next = node;
        return start;
    }
    // Find the block that is ahead of the block to be inserted
    while (node->next != NULL)
    {
        // Don't error check for size of space available - should be done outside this method
        if (node->next->start > start)
        {
            BlockListNode *nextNode = node->next;
            node->next = newBlock;
            newBlock->next = nextNode;
            return start;
        }
        node = node->next;
    }
    if (node->next == NULL)
    {
        // New element should be last element in list
        node->next = newBlock;
        newBlock->next = NULL;
        return start;
    }
    printf("Error in addHole\n");
    return NO_FIT_FOUND;
}

int deleteHole(int start, AllocInfo *info)
{
    BlockListNode *node = info->holes;
    if (node == NULL)
        return NO_FIT_FOUND;
    if (node->start == start)
    {
        info->holes = node->next;
        free(node);
        return 0;
    }
    while (node != NULL && node->next != NULL)
    {
        if (node->next->start == start)
        {
            BlockListNode *item = node->next;
            node->next = item->next;
            free(item);
            return 0;
        }
        if (node->next->start > start)
            return NO_FIT_FOUND;
        node = node->next;
    }
    return NO_FIT_FOUND;
}

int addAllocation(int start, int size, AllocInfo *info)
{
    // Need to make a new node for the allocation list
    BlockListNode *newBlock = malloc(sizeof(BlockListNode));
    newBlock->start = start;
    newBlock->size = size;

    // Need to insert the new node into the linked list
    BlockListNode *node = info->allocations;
    if (node == NULL)
    {
        // New block is the first element in the list
        info->allocations = newBlock;
        newBlock->next = NULL;
        return start;
    }
    if (node->start > start)
    {
        // New block should be first element in list
        info->allocations = newBlock;
        newBlock->next = node;
        return start;
    }
    // Find the block that is ahead of the block to be inserted
    while (node->next != NULL)
    {
        // Don't error check for size of space available - should be done outside this method
        if (node->next->start > start)
        {
            BlockListNode *nextNode = node->next;
            node->next = newBlock;
            newBlock->next = nextNode;
            return start;
        }
        node = node->next;
    }
    if (node->next == NULL)
    {
        // New element should be last element in list
        node->next = newBlock;
        newBlock->next = NULL;
        return start;
    }
    printf("Error in addAllocation\n");
    return NO_FIT_FOUND;
}

void updateHole(int requestSize, BlockListNode *holeNode, AllocInfo *info)
{
    if (holeNode->size == requestSize)
        deleteHole(holeNode->start, info);
    else
    {
        holeNode->start = holeNode->start + requestSize;
        holeNode->size = holeNode->size - requestSize;
    }
}

void debugInfo(AllocInfo *info)
{
    BlockListNode *hole = info->holes;
    printf("Holes:");
    while (hole != NULL)
    {
        printf(" [%d,%d]", hole->start, hole->start+hole->size-1);
        hole = hole->next;
    }

    BlockListNode *allocation = info->allocations;
    printf("\nAllocations:");
    while (allocation != NULL)
    {
        printf(" [%d,%d]", allocation->start, allocation->start+allocation->size-1);
        allocation = allocation->next;
    }

    printf("\n");
}


int validateAllocation(AllocInfo *allocInfo) {
    int allocSum, holeSum, hole, done, idx, match;
    BlockListNode *currNode, *holeNode, *allocNode;

    allocSum = 0;
    holeSum = 0;

    currNode = allocInfo->allocations;
    while (currNode != NULL) {
        allocSum = allocSum + currNode->size;
        currNode = currNode->next;
    }

    currNode = allocInfo->holes;
    while (currNode != NULL) {
        holeSum = holeSum + currNode->size;
        currNode = currNode->next;
    }

    if (allocSum + holeSum != allocInfo->capacity) {
        printf("ERROR: capacity = %d, allocSum = %d, holeSum = %d\n",
               allocInfo->capacity, allocSum, holeSum);
        return 1;
    }

    idx = 0;
    holeNode = allocInfo->holes;
    allocNode = allocInfo->allocations;
    while (idx < allocInfo->capacity) {
        match = 0;
        if (allocNode != NULL) {
            if (idx == allocNode->start) {
//debug: printf("idx = %d: match alloc (%d, %d)\n", idx, allocNode->start, allocNode->size);
                idx = idx + allocNode->size;
                allocNode = allocNode->next;
                match = 1;
            }
        }

        if (match == 0) {
            if (holeNode != NULL) {
                if (idx == holeNode->start) {
                    // check for consecutive holes
                    if (holeNode->next != NULL) {
                        if (holeNode->start + holeNode->size == holeNode->next->start) {
                            printf("ERROR: consecutive holes (%d, %d) and (%d, %d)\n",
                                   holeNode->start, holeNode->size,
                                   holeNode->next->start, holeNode->next->size);
                            return 1;
                        }
                    }
//debug: printf("idx = %d: match hole (%d, %d)\n", idx, holeNode->start, holeNode->size);
                    idx = idx + holeNode->size;
                    holeNode = holeNode->next;
                    match = 1;
                }
            } // didn't match on allocation node
        }

        if ( ! match ) {
            printf("ERROR: index = %d, but can't match next hole or allocation\n", idx);
            return 1;
        }
    }

    if (idx != allocInfo->capacity) {
        printf("ERROR: index = %d, but capacity = %d\n", idx, allocInfo->capacity);
        return 1;
    }

    return 0;
}

double getElementsInUseRatio(AllocInfo *info)
{
    double totalAllocSize = 0;
    BlockListNode *node = info->allocations;
    while (node != NULL)
    {
        totalAllocSize += node->size;
        node = node->next;
    }
    return totalAllocSize / info->capacity;
}

double getMeanHoleSize(AllocInfo *info)
{
    double totalHoleSize = 0;
    int numHoles = 0;
    BlockListNode *node = info->holes;
    while (node != NULL)
    {
        totalHoleSize += node->size;
        numHoles++;
        node = node->next;
    }
    if (numHoles > 0)
        return totalHoleSize / numHoles;
    return 0;
}

AllocInfo *newAllocInfo(int capacity)
{
    AllocInfo *allocInfo = malloc(sizeof(AllocInfo));
    allocInfo->capacity = CAPACITY;
    allocInfo->allocations = NULL;
    allocInfo->holes = NULL;
    addHole(0, CAPACITY, allocInfo);

    return allocInfo;
}

void freeAllocInfo(AllocInfo *info)
{
    BlockListNode *node = info->holes;
    while (node != NULL)
    {
        BlockListNode *temp = node;
        node = node->next;
        free(temp);
    }

    node = info->allocations;
    while (node != NULL)
    {
        BlockListNode *temp = node;
        node = node->next;
        free(temp);
    }

    free(info);
}
