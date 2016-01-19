/*
 * File:   pool_allocator.h
 * Author: xud8
 *
 * Created on November 25, 2015, 1:45 PM
 */

#ifndef POOL_ALLOCATOR_H
#define	POOL_ALLOCATOR_H


#include "triton_pool_base_allocator.h"


template<typename T, size_t align = 1 << 12,  size_t maxBlockBytes = 1 << 22>
class triton_pool_allocator: public pool_allocator_base<align, maxBlockBytes>
{
public:
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
    typedef const T& const_reference;
    typedef T value_type;

    template<typename T1>
    struct rebind {typedef triton_pool_allocator<T1, align, maxBlockBytes> other;};

    triton_pool_allocator() throw() {}
    triton_pool_allocator(triton_pool_allocator const&) throw() {}
    template<typename T1>
    triton_pool_allocator(triton_pool_allocator<T1, align, maxBlockBytes> const&) throw() {}
    ~triton_pool_allocator() throw() {}

    pointer address(reference r) const {return &r;}
    const_pointer address(const_reference r) const
    {return &r;}

    size_type max_size() const throw() {return size_t(-1) / sizeof(T);}

    void construct(pointer ptr, const_reference v) {
        new ((void*)ptr) T(v);
    }

    void destroy(pointer ptr) {ptr->~T();}

    pointer allocate(size_type n, const void* = 0);

    void deallocate(pointer ptr, size_type n);

private:

};

template<typename T, size_t align, size_t maxBlockBytes>
T*
triton_pool_allocator<T, align, maxBlockBytes>::allocate(size_type nBytes, const void*) {
    pointer ret = 0;
    if (__builtin_expect(nBytes != 0, true)) {

        if (nBytes > this->max_size())
            std::__throw_bad_alloc();

        size_t totalBytes = nBytes * sizeof(T);

        if (totalBytes < align || totalBytes > maxBlockBytes) {
            ret = static_cast<T*>(::operator new(totalBytes));
        }
        else {
            typename pool_allocator_base<align, maxBlockBytes>::blockNode* volatile* free_list = this->get_free_list(totalBytes);

            __gnu_cxx::__scoped_lock sentry(this->get_mutex());

            typename pool_allocator_base<align, maxBlockBytes>::blockNode* result = *free_list;
            if (__builtin_expect(result == 0, false)) {
                ret = reinterpret_cast<T*>(this->allocate_chunk(totalBytes));
            }
            else {
                *free_list = result->next;
                size_t bytes_to_allocate = align * (this->get_free_list_slot(totalBytes) + 1);
                ret = reinterpret_cast<T*>(result);

                this->update_index_on_allocation(totalBytes, *free_list);
                this->s_total_recycled_memory_size -= bytes_to_allocate;
            }
            if (ret == 0) {
                std::__throw_bad_alloc();
            }
        }
    }
    return ret;
}


template<typename T, size_t align, size_t maxBlockBytes>
void
triton_pool_allocator<T, align, maxBlockBytes>::deallocate(pointer p, size_type nBytes) {
    if (__builtin_expect(nBytes != 0 && p != 0, true)) {

        size_type totalBytes = nBytes * sizeof(T);
        if (totalBytes < align || totalBytes > maxBlockBytes) {
            ::operator delete(p);
        }
        else {
            __gnu_cxx::__scoped_lock sentry(this->get_mutex());
            this->deallocate_chunk(reinterpret_cast<char*>(p), totalBytes);
        }
    }
}

#endif	/* POOL_ALLOCATOR_H */

