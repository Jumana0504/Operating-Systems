#include <unistd.h>
#include <cmath>
#include <string.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdint.h>

#define MAX_ORDER 10
#define MAX_BLOCK_SIZE (128 * 1024) // 128 KB
#define PAGE_SIZE 4096 // 4KB
#define ALIGNMENT (32 * 128 * 1024) // 4MB


typedef struct MallocMetadata {
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
} MallocMetadata;

class FreeList {
    MallocMetadata* free_lists[MAX_ORDER + 1];
    size_t total_blocks;
    size_t total_allocated_bytes;
    bool initialized;
    FreeList() : free_lists{NULL}, total_blocks(0), total_allocated_bytes(0), initialized(false) {}
public:
    static FreeList& getInstance() // make FreeList
    {
        static FreeList instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }
    MallocMetadata* getFreeList(int i);
    size_t getToatalBlocks();
    void increaseToatalBlocks();
    void decreaseToatalBlocks();
    size_t getToatalAllocatedBytes();
    void addToatalAllocatedBytes(size_t add);
    bool isInitialized();
    void* initializeFreeLists();
    void* allocateBlock(int order);
    void insertBlock(MallocMetadata* block, int order);
    void removeBlock(MallocMetadata* block, int order);
};

MallocMetadata* FreeList::getFreeList(int i)
{
    return this->free_lists[i];
}


size_t FreeList::getToatalBlocks()
{
    return this->total_blocks;
}

void FreeList::increaseToatalBlocks()
{
    this->total_blocks++;
}

void FreeList::decreaseToatalBlocks()
{
    this->total_blocks--;
}

size_t FreeList::getToatalAllocatedBytes()
{
    return this->total_allocated_bytes;
}

void FreeList::addToatalAllocatedBytes(size_t add)
{
    this->total_allocated_bytes += add;
}

bool FreeList::isInitialized()
{
    return this->initialized;
}

void* FreeList::initializeFreeLists() 
{
    void* base = sbrk(0);
    if (base == (void*)-1) 
    {
        return NULL;
    }
    uintptr_t base_addr = (uintptr_t)base;
    uintptr_t aligned_brk_addr = (base_addr+ ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    size_t offset = aligned_brk_addr - base_addr;
    if (sbrk(offset + ALIGNMENT) == (void*)-1) 
    {
        return NULL;
    }
    MallocMetadata* block = (MallocMetadata*)aligned_brk_addr;
    free_lists[MAX_ORDER] = block;
    for (int i = 0; i < 32; i++) 
    {
        block->size = MAX_BLOCK_SIZE;
        block->is_free = true;
        if (i < 31) 
        {
            block->next = (MallocMetadata*)((char*)block + MAX_BLOCK_SIZE);
        }
        if (i > 0) 
        {
            block->prev = (MallocMetadata*)((char*)block - MAX_BLOCK_SIZE);
        }
        block = block->next;
    }
    for (int i = 0; i < MAX_ORDER; i++)
    {
        free_lists[i] = NULL;
    }
    total_blocks = 32;
    total_allocated_bytes = 32 * (MAX_BLOCK_SIZE-sizeof(MallocMetadata));
    initialized = true;
    return base;
}

void* FreeList::allocateBlock(int order)
{
    for (int i = order; i <= MAX_ORDER; i++) 
    {
        MallocMetadata* block = free_lists[i];
        if (block) 
        {
            removeBlock(block,i);
            // Split
            while (i > order) 
            {
                total_blocks++;
                MallocMetadata* buddy = (MallocMetadata*)((size_t)(block) ^ (block->size/2));
                buddy->size = block->size / 2;
                buddy->is_free = true;
                buddy->next = NULL;
                buddy->prev = NULL;
                insertBlock(buddy,i-1);
                block->size /= 2;
                total_allocated_bytes -= sizeof(MallocMetadata);
                i--;
            }
            block->is_free = false;
            return (char*)(block) + sizeof(MallocMetadata);
        }
    }
    return NULL;
}

void FreeList::insertBlock(MallocMetadata* block, int order) 
{
    if (free_lists[order] == NULL) 
    {
        free_lists[order] = block;
        block->prev = NULL;
        block->next = NULL;
    } 
    else 
    {
        MallocMetadata* current = free_lists[order];
        while (current && (char*)current < (char*)block) 
        {
            current = current->next;
        }
        if (current) 
        {
            block->next = current;
            block->prev = current->prev;
            if (current->prev) 
            {
                current->prev->next = block;
            } 
            else 
            {
                free_lists[order] = block;
            }
            current->prev = block;
        } 
        else 
        {
            MallocMetadata* tail = free_lists[order];
            while (tail->next)
            {
                tail = tail->next;
            }
            tail->next = block;
            block->prev = tail;
            block->next = NULL;
        }
    }
}

void FreeList::removeBlock(MallocMetadata* block, int order)
{
    if (block->prev) 
    {
        block->prev->next = block->next;
    } 
    else 
    {
        free_lists[order] = block->next;
    }
    if (block->next) 
    {
        block->next->prev = block->prev;
    }
}

////////////////////////////////////5-10,Functions//////////////////////////////////

size_t _num_free_blocks()
{
    size_t count = 0;
    for (int i = 0; i <= MAX_ORDER; i++) 
    {
        MallocMetadata* block = FreeList::getInstance().getFreeList(i);
        while (block) 
        {
            count++;
            block = block->next;
        }
    }
    return count;
}

size_t _num_free_bytes() 
{
    size_t count = 0;
    for (int i = 0; i <= MAX_ORDER; i++) 
    {
        MallocMetadata* block = FreeList::getInstance().getFreeList(i);
        while (block) 
        {
            count += (block->size - sizeof(MallocMetadata));
            block = block->next;
        }
    }
    return count;
}

size_t _num_allocated_blocks() 
{
    return FreeList::getInstance().getToatalBlocks();
}

size_t _num_allocated_bytes() 
{
    return FreeList::getInstance().getToatalAllocatedBytes();
}

size_t _num_meta_data_bytes() 
{
    return FreeList::getInstance().getToatalBlocks() * sizeof(MallocMetadata);
}

size_t _size_meta_data() 
{
    return sizeof(MallocMetadata);
}

////////////////////////////////////1-4,Functions//////////////////////////////////

void* smalloc(size_t size) 
{
    if (!FreeList::getInstance().isInitialized()) 
    {
        if (!FreeList::getInstance().initializeFreeLists())
        {
            return NULL;
        }
    }
    if ((size == 0) || (size > pow(10, 8))) 
    {
        return NULL;
    }
    int order = ceil(log2(size + sizeof(MallocMetadata))) - 7;
    if (order < 0)
    {
        order = 0;
    }
    if (size + sizeof(MallocMetadata) > MAX_BLOCK_SIZE)
    {
        size_t mmap_size = ((size + sizeof(MallocMetadata) + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
        void* ptr = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == (void *) -1) 
        {
            return NULL;
        }
        MallocMetadata* block = (MallocMetadata*)(ptr);
        block->size = size + sizeof(MallocMetadata);
        block->is_free = false;
        FreeList::getInstance().increaseToatalBlocks();
        FreeList::getInstance().addToatalAllocatedBytes(size);
        return (char*)(block) + sizeof(MallocMetadata);
    }
    return FreeList::getInstance().allocateBlock(order);
}

void* scalloc(size_t num, size_t size)
{
    void* allocated = smalloc (num * size);
    if (allocated == NULL)
    {
        return NULL;
    }
    return memset(allocated, 0, num * size);
}

void sfree(void* p)
{
    if (p == NULL)
    {
        return;
    }
    MallocMetadata* block = (MallocMetadata*)((char*)(p) - sizeof(MallocMetadata));
    if (block->size > MAX_BLOCK_SIZE)
    {
        size_t mmap_size = ((block->size + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
        FreeList::getInstance().addToatalAllocatedBytes(-(block->size - sizeof(MallocMetadata)));
        FreeList::getInstance().decreaseToatalBlocks();
        block->is_free = true;
        munmap(block,mmap_size);
        return;
    }
    if (!block->is_free) 
    {
        block->is_free = true;
        int order = ceil(log2(block->size)) - 7;
        if (order < 0)
        {
            order = 0;
        }
        // Merge
        MallocMetadata* buddy = (MallocMetadata*)((size_t)(block) ^ block->size);
        while (buddy != NULL && buddy->is_free && buddy->size == block->size && order < MAX_ORDER) 
        {
            FreeList::getInstance().decreaseToatalBlocks();
            FreeList::getInstance().addToatalAllocatedBytes(sizeof(MallocMetadata));
            FreeList::getInstance().removeBlock(buddy, order);
            buddy->is_free = true;
            buddy->size *= 2;
            block->size *= 2;
            order++;
            if ((void*)buddy < (void*)block)
            {
                block = buddy;
            }
            buddy = (MallocMetadata*)((size_t)(block) ^ block->size);
        }
        FreeList::getInstance().insertBlock(block, order);
    }
}

void* srealloc(void* oldp, size_t size)
{
    if ((size == 0) || (size > pow(10, 8)))
    {
        return NULL;
    }
    MallocMetadata* oldp_mmd = (MallocMetadata*)((char*)(oldp) - sizeof(MallocMetadata));
    if (oldp_mmd == NULL || oldp == NULL)
    {
        return smalloc(size);
    }
    if (oldp_mmd->size > MAX_BLOCK_SIZE)
    {
        if (size + sizeof(MallocMetadata) == oldp_mmd->size)
        {
            return oldp;
        }
        void* reallocated_mmap_block = smalloc(size);
        if (reallocated_mmap_block == NULL)
        {
            return NULL;
        }
        memmove(reallocated_mmap_block,oldp,oldp_mmd->size - sizeof(MallocMetadata));
        sfree(oldp);
        return reallocated_mmap_block;
    }
    if (oldp_mmd->size < size + sizeof(MallocMetadata))
    {
        int order = ceil(log2(oldp_mmd->size)) - 7;
        if (order < 0)
        {
            order = 0;
        }
        // Merge
        MallocMetadata* buddy = (MallocMetadata*)((size_t)(oldp_mmd) ^ oldp_mmd->size);
        while (buddy != NULL && buddy->is_free && buddy->size == oldp_mmd->size && order < MAX_ORDER) 
        {
            FreeList::getInstance().decreaseToatalBlocks();
            FreeList::getInstance().addToatalAllocatedBytes(sizeof(MallocMetadata));
            FreeList::getInstance().removeBlock(buddy, order);
            buddy->is_free = true;
            buddy->size *= 2;
            oldp_mmd->size *= 2;
            order++;
            if (buddy < oldp_mmd)
            {
                oldp_mmd->is_free = true;
                oldp_mmd = buddy;
                oldp_mmd->is_free = false;
            }
            buddy = (MallocMetadata*)((size_t)(oldp_mmd) ^ oldp_mmd->size);
            if (oldp_mmd->size >= size + sizeof(MallocMetadata))
            {
                return (char*)oldp_mmd + sizeof(MallocMetadata);
            }
        }
        void* reallocated_block = smalloc(size);
        if (reallocated_block == NULL)
        {
            return NULL;
        }
        memmove(reallocated_block,oldp,oldp_mmd->size - sizeof(MallocMetadata));
        sfree(oldp);
        return reallocated_block;
    }
    else 
    {
        return oldp;
    }
}

