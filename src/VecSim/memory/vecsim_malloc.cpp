#include "vecsim_malloc.h"
#include <stdlib.h>
#include <memory>
#include <string.h>

std::shared_ptr<VecSimAllocator> VecSimAllocator::newVecsimAllocator() {
    std::shared_ptr<VecSimAllocator> allocator(new VecSimAllocator());
    return allocator;
}

size_t VecSimAllocator::allocation_header_size = sizeof(size_t);

VecSimMemoryFunctions VecSimAllocator::memFunctions = {.allocFunction = malloc,
                                                       .callocFunction = calloc,
                                                       .reallocFunction = realloc,
                                                       .freeFunction = free};

void VecSimAllocator::setMemoryFunctions(VecSimMemoryFunctions memFunctions) {
    VecSimAllocator::memFunctions = memFunctions;
}

void *VecSimAllocator::allocate(size_t size) {
    size_t *ptr = (size_t *)vecsim_malloc(size + allocation_header_size);

    if (ptr) {
        *this->allocated.get() += size + allocation_header_size;
        *ptr = size;
        return ptr + 1;
    }
    return NULL;
}

void VecSimAllocator::deallocate(void *p, size_t size) { free_allocation(p); }

void *VecSimAllocator::reallocate(void *p, size_t size) {
    if (!p) {
        return this->allocate(size);
    }
    size_t oldSize = getPointerAllocationSize(p);
    if (oldSize >= size) {
        return p;
    }
    void *new_ptr = this->allocate(size);
    if (new_ptr) {
        memcpy(new_ptr, p, oldSize);
        free_allocation(p);
        return new_ptr;
    }
    return NULL;
}

void VecSimAllocator::free_allocation(void *p) {
    if (!p)
        return;
    size_t *ptr = ((size_t *)p) - 1;
    *this->allocated.get() -= (ptr[0] + allocation_header_size);
    vecsim_free(ptr);
}

void *VecSimAllocator::callocate(size_t size) {
    size_t *ptr = (size_t *)vecsim_calloc(1, size + allocation_header_size);

    if (ptr) {
        *this->allocated.get() += size + allocation_header_size;
        *ptr = size;
        return ptr + 1;
    }
    return NULL;
}

void *VecSimAllocator::operator new(size_t size) { return vecsim_malloc(size); }

void *VecSimAllocator::operator new[](size_t size) { return vecsim_malloc(size); }
void VecSimAllocator::operator delete(void *p, size_t size) { vecsim_free(p); }
void VecSimAllocator::operator delete[](void *p, size_t size) { vecsim_free(p); }

int64_t VecSimAllocator::getAllocationSize() { return *this->allocated.get(); }
