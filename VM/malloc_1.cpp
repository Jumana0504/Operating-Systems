#include <unistd.h>
#include <cmath>

void* smalloc(size_t size)
{
    if (size == 0 || size > pow(10,8))
    {
        return NULL;
    }
    void* first_allocated_byte = sbrk(size);
    return (first_allocated_byte == (void*)-1) ? NULL : first_allocated_byte;
}