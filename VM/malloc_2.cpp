#include <unistd.h>
#include <cmath>
#include <string.h>


typedef struct MallocMetadata {
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
} MallocMetadata;

//needs to be sorted, head is the lowest address
class List {
    MallocMetadata* list_head;
    List() : list_head(NULL) {}
public:
    static List &getInstance() // make List
    {
        static List instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }
    MallocMetadata* getListHead();
    void* find_block(size_t size);
    void insertBlock(void* block_ptr);
    void freeBlock(void* block_ptr);
    void* findElementPtr(void* block_ptr);
};

MallocMetadata* List::getListHead()
{
    return this->list_head;
}

void* List::find_block(size_t size)
{
    MallocMetadata *node = list_head;
    while (node != NULL) 
    {
        if (node->size >= size && node->is_free)
        {
            node->is_free = false;
            return node;
        }
        node = node->next;
    }
    return NULL;
}

void List::insertBlock(void* block_ptr)
{
    MallocMetadata* meta_data_block_ptr = (MallocMetadata*)block_ptr;
    meta_data_block_ptr->is_free = false;
    meta_data_block_ptr->next = NULL;
    meta_data_block_ptr->prev = NULL;
    if (list_head == NULL)
    {
        list_head = meta_data_block_ptr;
    }
    else
    {
        MallocMetadata* current = list_head;
        while (current->next != NULL)
        {
            current = current->next;
        }
        current->next = meta_data_block_ptr;
        meta_data_block_ptr->prev = current;
    }
}

void List::freeBlock(void* block_ptr)
{
    MallocMetadata* meta_data_block_ptr = (MallocMetadata*)((char*)block_ptr - sizeof(MallocMetadata));
    if (meta_data_block_ptr != NULL)
    {
        meta_data_block_ptr->is_free = true;
    }
}

void* List::findElementPtr(void* block_ptr)
{
    MallocMetadata *current = list_head;
    while (current != NULL) 
    {
        if ((char*)current + sizeof(MallocMetadata) == block_ptr)
        {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

////////////////////////////////////5-10,Functions//////////////////////////////////

size_t _num_free_blocks()
{
    MallocMetadata *node = List::getInstance().getListHead();
    int counter = 0;
    while (node != NULL) 
    {
        if (node->is_free)
        {
            counter++;
        }
        node = node->next;
    }
    return counter;
}

size_t _num_free_bytes()
{
    MallocMetadata *node = List::getInstance().getListHead();
    int bytes_sum = 0;
    while (node != NULL) 
    {
        if (node->is_free)
        {
            bytes_sum += node->size;
        }
        node = node->next;
    }
    return bytes_sum;
}

size_t _num_allocated_blocks()
{
    MallocMetadata *node = List::getInstance().getListHead();
    int counter = 0;
    while (node != NULL) 
    {
        counter++;
        node = node->next;
    }
    return counter;
}

size_t _num_allocated_bytes()
{
    MallocMetadata *node = List::getInstance().getListHead();
    int bytes_sum = 0;
    while (node != NULL) 
    {
        bytes_sum += node->size;
        node = node->next;
    }
    return bytes_sum;
}
size_t _num_meta_data_bytes()
{
    MallocMetadata *node = List::getInstance().getListHead();
    int md_bytes_num = 0;
    while (node != NULL) 
    {
        md_bytes_num ++;
        node = node->next;
    }
    return md_bytes_num * sizeof(MallocMetadata);
}

size_t _size_meta_data()
{
    return sizeof(MallocMetadata);
}

////////////////////////////////////1-4,Functions//////////////////////////////////

void* smalloc(size_t size) 
{
    if ((size == 0) || (size > pow(10, 8)))
    {
        return NULL;
    }
    MallocMetadata *free_block = (MallocMetadata*)List::getInstance().find_block(size);
    if (free_block == NULL) // there is no free block found in list , allocate with sbrk and create metadata
    {
        void *current_break = sbrk(size + sizeof(MallocMetadata));
        if (current_break == (void *) -1)
        {
            return NULL;
        }
        //create metadata
        MallocMetadata* add_block = (MallocMetadata*)current_break;
        add_block->size = size;
        List::getInstance().insertBlock(add_block);
        return (char*)current_break + sizeof(MallocMetadata);
    } 
    else 
    {
        return (char*)free_block + sizeof(MallocMetadata);
    }
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
    List::getInstance().freeBlock(p);
}

void* srealloc(void* oldp, size_t size)
{
    if ((size == 0) || (size > pow(10, 8)))
    {
        return NULL;
    }
    MallocMetadata* get_old_block_meta_data_ptr = (MallocMetadata*)List::getInstance().findElementPtr(oldp);
    if (get_old_block_meta_data_ptr == NULL || oldp == NULL)
    {
        return smalloc(size);
    }
    if (get_old_block_meta_data_ptr->size < size)
    {
        void* reallocated_block = smalloc(size);
        if (reallocated_block == NULL)
        {
            return NULL;
        }
        memmove(reallocated_block,oldp,get_old_block_meta_data_ptr->size);
        sfree(oldp);
        return reallocated_block;
    }
    else 
    {
        return oldp;
    }
}

