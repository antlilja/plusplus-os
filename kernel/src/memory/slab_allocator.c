#include "memory/slab_allocator.h"

#include "kassert.h"
#include "util.h"
#include "memory.h"
#include "memory/paging.h"

#include <stddef.h>
#include <stdalign.h>

#define CACHE_START_ORDER 4
#define CACHE_END_ORDER 11

#define OFF_SLAB_THRESHOLD (PAGE_SIZE / 8)
#define MIN_OBJ_THRESHOLD 4

#define NUM_CACHES (CACHE_END_ORDER - CACHE_START_ORDER)

#define FREE_ARRAY(cache, slab) ((uint32_t*)((slab)->mem - (cache)->free_arr_size))

typedef struct {
    void* next;         // Pointer to the next Slab in chain
    void* prev;         // Pointer to the previous Slab in chain
    void* mem;          // Pointer to first object in the slab
    uint32_t allocated; // Number of allocated objects in slab
    uint32_t next_free; // Index of the next free object
} Slab;

_Static_assert(
    sizeof(Slab) < OFF_SLAB_THRESHOLD,
    "Size of slab is larger than OFF_SLAB_THRESHOLD. This would cause infinite recursive calls to "
    "kalloc when an object larger than OFF_SLAB_THRESHOLD is allocated");

typedef struct {
    Slab* part;
    Slab* full;
    uint32_t pages;         // Number of pages each slab constists of
    uint32_t size;          // Size of objects in this cache
    uint32_t count;         // Number of objects each slab can hold
    uint32_t free_arr_size; // Size of the free array (in bytes, with alignment)
    uint32_t mem_size;      // Size of the memory used for objects
} Cache;

// Linked list entry for keeping track of pages allocated for single allocations
typedef struct {
    void* next;
    void* ptr;
    uint64_t pages;
} PageAllocation;

struct {
    Cache size_caches[NUM_CACHES];

    // List of non slab allocations
    PageAllocation* page_allocation_list;
} g_slab_allocator = {0};

uint8_t get_min_cache_order(uint64_t size) {
    uint8_t order = CACHE_START_ORDER;
    while ((1ULL << order) < size && order <= CACHE_END_ORDER) ++order;
    return order;
}

__attribute__((always_inline)) Cache* get_cache(uint8_t order) {
    return &g_slab_allocator.size_caches[order - CACHE_START_ORDER];
}

void initialize_slab_allocator() {
    // Initialize size caches
    for (uint8_t order = CACHE_START_ORDER; order <= CACHE_END_ORDER; ++order) {
        Cache* cache = get_cache(order);
        cache->part = 0;
        cache->full = 0;
        cache->pages = 1;
        cache->size = (1 << order);

        while (((cache->pages * PAGE_SIZE) / cache->size) < MIN_OBJ_THRESHOLD) ++cache->pages;

        uint64_t size = cache->pages * PAGE_SIZE;
        if (cache->size < OFF_SLAB_THRESHOLD) size -= sizeof(Slab);

        cache->count = size / cache->size;
        while (cache->count != 0) {
            cache->mem_size = cache->count * cache->size;

            const uint64_t alignment = MIN(alignof(max_align_t), cache->size);

            cache->free_arr_size = round_up_to_multiple(cache->count * sizeof(uint32_t), alignment);

            if (size - cache->mem_size >= cache->free_arr_size) break;

            --cache->count;
        }

        KERNEL_ASSERT(cache->count != 0, "Number of objects stored can't be zero")
    }
}

void* kalloc(uint64_t size) {
    const uint8_t order = get_min_cache_order(size);

    // No cache large enough exists, so we allocate the closest number of pages instead.
    if (order > CACHE_END_ORDER) {
        const uint64_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
        void* ptr = alloc_pages(pages, PAGING_WRITABLE);
        if (ptr == 0) return 0;

        // Store in a list so that the pages can be deallocated just through the pointer
        PageAllocation* allocation = kalloc(sizeof(PageAllocation));
        allocation->ptr = ptr;
        allocation->pages = pages;
        allocation->next = g_slab_allocator.page_allocation_list;
        g_slab_allocator.page_allocation_list = allocation;

        return ptr;
    }

    Slab* slab;
    Cache* cache = get_cache(order);
    if (cache->part != 0) {
        slab = cache->part;
    }
    else {
        // Allocate slab and owned memory
        {
            void* mem = (void*)alloc_pages(cache->pages, PAGING_WRITABLE);
            if (mem == 0) return 0;

            // Allocate Slab on memory if size is below threshold
            if (cache->size < OFF_SLAB_THRESHOLD) {
                slab = (Slab*)mem;
                slab->mem = mem + sizeof(Slab);
            }
            else {
                slab = kalloc(sizeof(Slab));
                if (slab == 0) {
                    free_pages(mem, cache->pages);
                    return 0;
                }
                slab->mem = mem;
            }
        }

        // Initialize free array
        {
            uint32_t* free = (uint32_t*)slab->mem;
            for (uint32_t i = 0; i < cache->count; ++i) {
                free[i] = i + 1;
            }
        }

        // Point slab->mem at start of object memory
        slab->mem += cache->free_arr_size;

        slab->next_free = 0;
        slab->allocated = 0;

        // Add to part slab list
        slab->next = cache->part;
        slab->prev = 0;
        if (cache->part != 0) cache->part->prev = slab;
        cache->part = slab;
    }

    void* obj_ptr = slab->mem + (slab->next_free * cache->size);
    slab->next_free = FREE_ARRAY(cache, slab)[slab->next_free];
    ++slab->allocated;

    // Is slab full?
    if (slab->allocated == cache->count) {
        // Remove from partial list
        // NOTE (Anton Lilja):
        // This one is different from the function remove_slab_from_list because we know that the
        // slab is the head of the partial list
        {
            cache->part = slab->next;

            if (cache->part != 0) cache->part->prev = 0;
        }

        // Add to full list
        slab->next = cache->full;
        if (cache->full != 0) cache->full->prev = (void*)slab;
        cache->full = slab;
    }

    KERNEL_ASSERT(((uint64_t)obj_ptr % MIN(alignof(max_align_t), cache->size)) == 0,
                  "kalloc pointer has incorrect alignment")
    return obj_ptr;
}

void remove_slab_from_list(Slab** head, Slab* slab) {
    Slab* next = (Slab*)slab->next;
    if (next != 0) next->prev = slab->prev;

    Slab* prev = (Slab*)slab->prev;
    if (prev != 0) prev->next = slab->next;

    if (slab == *head) *head = (Slab*)slab->next;
}

bool free_from_slab_list(void* ptr, Cache* cache, Slab* slab) {
    while (slab != 0) {
        if (range_contains((uint64_t)ptr, (uint64_t)slab->mem, cache->mem_size)) {
            {
                const uint64_t obj_offset = (ptr - slab->mem);
                KERNEL_ASSERT((obj_offset % cache->size) == 0, "Pointer not aligned")

                const uint32_t obj_index = obj_offset / cache->size;
                FREE_ARRAY(cache, slab)[obj_index] = slab->next_free;
                slab->next_free = obj_index;
            }

            if (slab->allocated == cache->count) {
                // Remove from full list
                remove_slab_from_list(&cache->full, slab);

                // Add to partial list
                slab->next = cache->part;
                slab->prev = 0;
                if (cache->part != 0) cache->part->prev = slab;
                cache->part = slab;
            }

            --slab->allocated;

            // Free slab if empty
            if (slab->allocated == 0) {
                // Remove from partial list
                remove_slab_from_list(&cache->part, slab);

                if (cache->size < OFF_SLAB_THRESHOLD) {
                    free_pages(slab, cache->pages);
                }
                else {
                    // Calculate pointer to the start of the page allocation and free those pages
                    slab->mem -= cache->free_arr_size;
                    free_pages(slab->mem, cache->pages);

                    // Free slab entry
                    kfree(slab);
                }
            }

            return true;
        }
        slab = (Slab*)slab->next;
    }

    return false;
}

void kfree(void* ptr) {
    for (uint8_t order = CACHE_START_ORDER; order <= CACHE_END_ORDER; ++order) {
        Cache* cache = get_cache(order);

        // Search partial slabs
        if (free_from_slab_list(ptr, cache, cache->part)) return;

        // Search full slabs
        if (free_from_slab_list(ptr, cache, cache->full)) return;
    }

    // Deallocate page allocation
    PageAllocation* allocation = g_slab_allocator.page_allocation_list;
    PageAllocation* last_allocation = 0;
    while (allocation != 0) {
        if (allocation->ptr == ptr) {
            free_pages(allocation->ptr, allocation->pages);

            if (last_allocation == 0) {
                g_slab_allocator.page_allocation_list = allocation->next;
            }
            else {
                last_allocation->next = allocation->next;
            }

            kfree(allocation);
            return;
        }

        last_allocation = allocation;
        allocation = allocation->next;
    }

    KERNEL_ASSERT(false, "Failed to free memory")
}
