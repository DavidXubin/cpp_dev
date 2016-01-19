#ifndef POOL_ALLOCATOR_BASE_H
#define	POOL_ALLOCATOR_BASE_H

#include <cassert>
#include <stddef.h>
#include <bits/c++config.h>
#include <stdint.h>
#include <ext/concurrence.h>

template<size_t align,  size_t maxBlockBytes>
class pool_allocator_base
{
protected:
    typedef struct _blockNode {
        struct _blockNode* next;
    }blockNode;

    typedef struct _chunkNode {
        struct _chunkNode* next;
        char* block;
    }chunkNode;

    static size_t get_free_list_slot(size_t bytes);
    static blockNode* volatile* get_free_list_by_slot(size_t slot);
    static blockNode* volatile* get_free_list(size_t bytes);

    static void update_index_on_allocation(size_t nbytes, const blockNode* pNode);
    static void update_index_on_deallocation(size_t nbytes);
    static size_t getAailableSlot(size_t nbytes);

    __gnu_cxx::__mutex& get_mutex();

    static char* allocate_chunk(size_t nbytes);
    static char* allocate_new_chunk(size_t nbytes);
    static void deallocate_chunk(char* p, size_t nBytes);

    static blockNode* volatile s_free_list[];
    static size_t s_free_list_size;

    static const size_t s_bits_per_byte = 8;
    static const size_t s_bytes_per_bitmap = 8;
    static uint64_t volatile s_slot_bitmap[];

    static size_t s_chunk_size;
    static const size_t s_max_sized_blocks_per_chunk = 10;

    static chunkNode* volatile s_chunk_head;

    static char* s_cur_chunk_pos;
    static char* s_cur_chunk_end;

    static size_t s_use_recycled_memory_threshold;
    static size_t s_total_recycled_memory_size;
};

template<size_t align,  size_t maxBlockBytes>
size_t pool_allocator_base<align, maxBlockBytes>::s_free_list_size = maxBlockBytes / align;

template<size_t align,  size_t maxBlockBytes>
typename pool_allocator_base<align, maxBlockBytes>::blockNode* volatile pool_allocator_base<align, maxBlockBytes>::s_free_list[maxBlockBytes / align] = {0};

template<size_t align,  size_t maxBlockBytes>
uint64_t volatile pool_allocator_base<align, maxBlockBytes>::s_slot_bitmap[maxBlockBytes / align / (pool_allocator_base<align, maxBlockBytes>::s_bytes_per_bitmap * pool_allocator_base<align, maxBlockBytes>::s_bits_per_byte)] = {0};

template<size_t align,  size_t maxBlockBytes>
typename pool_allocator_base<align, maxBlockBytes>::chunkNode* volatile pool_allocator_base<align, maxBlockBytes>::s_chunk_head = 0;

template<size_t align,  size_t maxBlockBytes>
size_t pool_allocator_base<align, maxBlockBytes>::s_chunk_size = maxBlockBytes * pool_allocator_base<align, maxBlockBytes>::s_max_sized_blocks_per_chunk;

template<size_t align,  size_t maxBlockBytes>
char* pool_allocator_base<align, maxBlockBytes>::s_cur_chunk_pos = 0;

template<size_t align,  size_t maxBlockBytes>
char* pool_allocator_base<align, maxBlockBytes>::s_cur_chunk_end = 0;

template<size_t align,  size_t maxBlockBytes>
size_t pool_allocator_base<align, maxBlockBytes>::s_total_recycled_memory_size = 0;

template<size_t align,  size_t maxBlockBytes>
size_t pool_allocator_base<align, maxBlockBytes>::s_use_recycled_memory_threshold = s_chunk_size / 10;


template<size_t align,  size_t maxBlockBytes>
size_t
pool_allocator_base<align, maxBlockBytes>::get_free_list_slot(size_t nbytes){
    return ((nbytes + align - 1) / align - 1);
}

template<size_t align,  size_t maxBlockBytes>
struct pool_allocator_base<align, maxBlockBytes>::blockNode* volatile*
pool_allocator_base<align, maxBlockBytes>::get_free_list_by_slot(size_t slot){
    return s_free_list + slot;
}

template<size_t align,  size_t maxBlockBytes>
typename pool_allocator_base<align, maxBlockBytes>::blockNode* volatile*
pool_allocator_base<align, maxBlockBytes>::get_free_list(size_t nbytes){
    size_t i = (nbytes + align - 1) / align - 1;
    return s_free_list + i;
}

template<size_t align,  size_t maxBlockBytes>
__gnu_cxx::__mutex& pool_allocator_base<align, maxBlockBytes>::get_mutex() {
    static __gnu_cxx::__mutex palloc_mutex;
    return palloc_mutex;
}

template<size_t align,  size_t maxBlockBytes>
void
pool_allocator_base<align, maxBlockBytes>::update_index_on_allocation(size_t nbytes, const blockNode *pNode) {

    if (pNode == 0) {
        size_t slotIdx = (nbytes + align - 1) / align - 1;
        size_t step = sizeof(uint64_t) * s_bits_per_byte;

        size_t idx = slotIdx / step;
        size_t offset = slotIdx % step;

        uint8_t *p = (uint8_t*)&s_slot_bitmap[idx];
        uint8_t tmp = *(p + offset / s_bytes_per_bitmap);
        tmp &= ~((uint8_t)0x01 << (s_bits_per_byte - offset % s_bytes_per_bitmap - 1));
        *(p + offset / s_bytes_per_bitmap) = tmp;
    }
}

template<size_t align,  size_t maxBlockBytes>
void
pool_allocator_base<align, maxBlockBytes>::update_index_on_deallocation(size_t nbytes) {

    size_t slotIdx = (nbytes + align - 1) / align - 1;
    size_t step = sizeof(uint64_t) * s_bits_per_byte;

    size_t idx = slotIdx / step;
    size_t offset = slotIdx % step;

    uint8_t *p = (uint8_t*)&s_slot_bitmap[idx];
    uint8_t tmp = *(p + offset / s_bytes_per_bitmap);
    tmp |= ((uint8_t)0x01 << (s_bits_per_byte - offset % s_bytes_per_bitmap - 1));
    *(p + offset / s_bytes_per_bitmap) = tmp;
}

template<size_t align,  size_t maxBlockBytes>
size_t
pool_allocator_base<align, maxBlockBytes>::getAailableSlot(size_t nbytes) {

    size_t slotIdx = (nbytes + align - 1) / align - 1;
    size_t step = sizeof(uint64_t) * s_bits_per_byte;

    size_t idx = slotIdx / step;
    size_t offset = slotIdx % step;
    size_t maxIdx = s_free_list_size / step - 1;

    int i;
    for ( i = maxIdx; i > idx; --i ) {
        if (s_slot_bitmap[i] != 0) {
            break;
        }
    }

    if ( i > idx ) {
        uint8_t *p = (uint8_t*)&s_slot_bitmap[i];

        int j = s_bytes_per_bitmap - 1;
        for ( ; j >= 0; --j ) {
            if (p[j] != 0) {
                break;
            }
        }

        int k = 0;
        for (; k < s_bits_per_byte; ++k ) {
            if ( p[j] & (0x01 << k) ) {
                break;
            }
        }
        return (i * step + j * s_bytes_per_bitmap + s_bits_per_byte - k - 1) ;
    }

    size_t innerIdx = offset / s_bytes_per_bitmap;
    size_t left_over = 0;

    if(innerIdx < s_bytes_per_bitmap - 1) {
        left_over = s_bytes_per_bitmap - innerIdx - 1;
        uint8_t *p = (uint8_t*)&s_slot_bitmap[idx] + innerIdx + 1;

        int i = left_over - 1;
        for ( ; i >= 0; --i ) {
            if (p[i] != 0) {
                break;
            }
        }

        if ( i >= 0 ) {
            size_t j = 0;
            for (; j < s_bits_per_byte; ++j ) {
                if ( p[i] & (0x01 << j) ) {
                    break;
                }
            }
            return (idx * step + (innerIdx + 1 + i) * s_bytes_per_bitmap + s_bits_per_byte - j - 1) ;
        }
    }

    size_t innerOffset = offset % s_bytes_per_bitmap;
    left_over = s_bits_per_byte - innerOffset - 1;
    for ( size_t k = 0; k < left_over; ++k ) {
        if ( *( (uint8_t*)&s_slot_bitmap[idx] + innerIdx ) & (0x01 << k) ) {
            return (idx * step + innerIdx * s_bytes_per_bitmap + s_bits_per_byte - k - 1);
        }
    }

    return -1;
}



template<size_t align,  size_t maxBlockBytes>
char*
pool_allocator_base<align, maxBlockBytes>::allocate_new_chunk(size_t bytes_to_get) {
    char *startFree = NULL, *endFree = NULL;

    try {
        size_t left_over = s_cur_chunk_end - s_cur_chunk_pos;
        if (left_over > 0) {
            blockNode* volatile* to_free_list = get_free_list(left_over);
            ((blockNode*)(void*)s_cur_chunk_pos)->next = *to_free_list;
            *to_free_list = (blockNode*)(void*)s_cur_chunk_pos;
            update_index_on_deallocation(left_over);
            s_total_recycled_memory_size += left_over;
        }

        chunkNode* p = static_cast<chunkNode*>(::operator new(sizeof(chunkNode) + s_chunk_size));
        p->next = s_chunk_head;
        p->block = reinterpret_cast<char*>(p) + sizeof(chunkNode);
        s_chunk_head = p;

        s_cur_chunk_pos = p->block + bytes_to_get;
        s_cur_chunk_end = p->block + s_chunk_size;

        startFree = p->block;
        endFree = startFree + bytes_to_get;
    }
    catch(const std::bad_alloc&) {
        // What we have wasn't enough.  Rethrow.
        startFree = endFree = 0;   // We have no chunk.
        __throw_exception_again;
    }

    return startFree;
}


template<size_t align,  size_t maxBlockBytes>
char*
pool_allocator_base<align, maxBlockBytes>::allocate_chunk(size_t nBytes) {

    char *startFree = NULL, *endFree = NULL;
    size_t bytes_to_get = align * (get_free_list_slot(nBytes) + 1);

    if(s_cur_chunk_end - s_cur_chunk_pos >= bytes_to_get) {
        startFree = s_cur_chunk_pos;
        s_cur_chunk_pos += bytes_to_get;
    }
    else if (s_total_recycled_memory_size < s_use_recycled_memory_threshold) {
        startFree = allocate_new_chunk(bytes_to_get);
    }
    else {
        size_t slot = getAailableSlot(nBytes);

        if (slot != -1) {
            blockNode* volatile* free_list = get_free_list_by_slot(slot);
            blockNode* p = *free_list;

            *free_list = p->next;
            startFree = (char*)(void*)p;
            endFree = startFree + bytes_to_get;

            update_index_on_allocation((slot  + 1 ) * align, *free_list);
            s_total_recycled_memory_size -= bytes_to_get;

            size_t left_over = (slot  + 1) * align - bytes_to_get;

            if (left_over > 0) {
                blockNode* volatile* to_free_list = get_free_list(left_over);
                ((blockNode*)(void*)endFree)->next = *to_free_list;
                *to_free_list = (blockNode*)(void*)endFree;
                update_index_on_deallocation(left_over);
            }
        }
        else{
            startFree = allocate_new_chunk(bytes_to_get);
        }
    }
    return startFree;
}

template<size_t align,  size_t maxBlockBytes>
void
pool_allocator_base<align, maxBlockBytes>::deallocate_chunk(char* p, size_t nBytes){

    blockNode* curNode = (blockNode*)(void*)p;
    size_t bytes_to_deallocate = align * (get_free_list_slot(nBytes) + 1);
    blockNode* volatile* cur_free_list = get_free_list(nBytes);
    curNode->next = *cur_free_list;
    *cur_free_list = curNode;
    update_index_on_deallocation(bytes_to_deallocate);

    s_total_recycled_memory_size += bytes_to_deallocate;
}

#endif

