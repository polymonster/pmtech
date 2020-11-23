// data_struct.h
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

// Minimalist, thread safe, bloat free data structures.. check description for thread safety, lock / lockless, etc

#pragma once

#include "console.h"
#include "memory.h"
#include "threads.h"

#ifndef NO_STRETCHY_BUFFER_SHORT_NAMES
#define sb_free stb_sb_free
#define sb_push stb_sb_push
#define sb_count stb_sb_count
#define sb_add stb_sb_add
#define sb_last stb_sb_last
#define sb_grow stb__sbgrow
#endif

#define stb_sb_free(a) ((a) ? free(stb__sbraw(a)), 0 : 0)
#define stb_sb_push(a, v) (stb__sbmaybegrow(a, 1), (a)[stb__sbn(a)++] = (v))
#define stb_sb_count(a) ((a) ? stb__sbn(a) : 0)
#define stb_sb_add(a, n) (stb__sbmaybegrow(a, n), stb__sbn(a) += (n), &(a)[stb__sbn(a) - (n)])
#define stb_sb_last(a) ((a)[stb__sbn(a) - 1])

#define stb__sbraw(a) ((int*)(a)-2)
#define stb__sbm(a) stb__sbraw(a)[0]
#define stb__sbn(a) stb__sbraw(a)[1]

#define stb__sbneedgrow(a, n) ((a) == 0 || stb__sbn(a) + (n) >= stb__sbm(a))
#define stb__sbmaybegrow(a, n) (stb__sbneedgrow(a, (n)) ? stb__sbgrow(a, n) : 0)
#define stb__sbgrow(a, n) (*((void**)&(a)) = stb__sbgrowf((a), (n), sizeof(*(a))))

#define sb_clear(v)                                                                                                          \
    stb_sb_free(v);                                                                                                          \
    v = nullptr

static void* stb__sbgrowf(void* arr, int increment, int itemsize)
{
    int start = stb_sb_count(arr);
    int dbl_cur = arr ? 2 * stb__sbm(arr) : 0;
    int min_needed = stb_sb_count(arr) + increment;
    int m = dbl_cur > min_needed ? dbl_cur : min_needed;

    // stretch buffer and zero mem
    int* p = nullptr;
    {
        uint32_t total_size = itemsize * m + sizeof(int) * 2;
        p = (int*)realloc(arr ? stb__sbraw(arr) : 0, total_size);

        char*    pp = (char*)p;
        uint32_t preserve_size = sizeof(int) * 2 + itemsize * start;
        memset(pp + preserve_size, 0x00, total_size - preserve_size);
    }

    if (p)
    {
        if (!arr)
            p[1] = 0;
        p[0] = m;
        return p + 2;
    }
    else
    {
#ifdef STRETCHY_BUFFER_OUT_OF_MEMORY
        STRETCHY_BUFFER_OUT_OF_MEMORY;
#endif
        return (void*)(2 * sizeof(int)); // try to force a NULL pointer exception later
    }
}

namespace pen
{
    // lightweight stack - single threaded
    template <typename T>
    struct stack
    {
        T*  data = nullptr;
        int pos = 0;

        void clear();
        void push(T i);
        T    pop();
        int  size();
    };

    // lockless single producer single consumer - thread safe ring buffer
    template <typename T>
    struct ring_buffer
    {
        T* data = nullptr;

        a_u32               get_pos;
        a_u32               put_pos;
        std::atomic<size_t> _capacity;

        ring_buffer();
        ~ring_buffer();

        void create(u32 capacity);
        void put(const T& item);
        T*   get();
        T*   check();
    };

    // lockless single producer multiple consumer - thread safe resource pool which will grow to accomodate contents
    template <typename T>
    struct res_pool
    {
        T*                  _resources = nullptr;
        std::atomic<size_t> _capacity;

        res_pool();
        ~res_pool();

        void init(u32 reserved_capacity);
        void grow(u32 min_capacity);
        void insert(const T& resource, u32 slot);
        T&   get(u32 slot);
        T&   operator[](u32 slot);
    };

    // lockless single producer multiple consumer -thread safe multi buffer
    template <typename T, u32 N>
    struct multi_buffer
    {
        T     _data[N];
        a_u32 _fb;
        a_u32 _bb;
        a_u32 _swaps;
        u32   _frame;

        multi_buffer();

        T&       backbuffer();
        const T& frontbuffer();
        void     swap_buffers();
    };

    // lockless single producer multiple consumer - thread safe multi buffer of arrays
    template <typename T, size_t N>
    struct multi_array_buffer
    {
        T*       _data[N] = {0};
        a_size_t _capacity[N];

        a_size_t _fb;
        a_size_t _bb;
        a_u32    _swaps;
        u32      _frame;

        multi_array_buffer();
        ~multi_array_buffer();

        T*       backbuffer();
        const T* frontbuffer();
        void     swap_buffers();

        void init(size_t size);
        void grow(size_t size);
    };

    // multiple producer, multiple consumer buffer - partially lock-free but will lock when re-sizing.
    template <typename T>
    struct mpmc_stretchy_buffer
    {
        T*          _data[2] = {nullptr, nullptr};
        a_size_t    _fb = {0};
        pen::mutex* _mut = nullptr;

        mpmc_stretchy_buffer()
        {
            _mut = pen::mutex_create();
            _fb = 0;
        }

        size_t size();
        void   push_back(T item);
        T&     operator[](size_t slot);
    };

    // function impls with always inline for fast data structs
    template <typename T>
    pen_inline void stack<T>::clear()
    {
        pos = 0;
    }

    template <typename T>
    pen_inline void stack<T>::push(T i)
    {
        if (pos >= sb_count(data))
        {
            sb_push(data, i);
        }
        else
        {
            data[pos] = i;
        }

        ++pos;
    }

    template <typename T>
    pen_inline T stack<T>::pop()
    {
        --pos;
        if (pos < 0)
            pos = 0;

        return data[pos];
    }

    template <typename T>
    pen_inline int stack<T>::size()
    {
        return pos;
    }

    template <typename T>
    pen_inline ring_buffer<T>::ring_buffer()
    {
        get_pos = 0;
        put_pos = 0;
        _capacity = 0;
    }

    template <typename T>
    pen_inline ring_buffer<T>::~ring_buffer()
    {
        pen::memory_free(data);
    }

    template <typename T>
    inline void ring_buffer<T>::create(u32 capacity)
    {
        get_pos = 0;
        put_pos = 0;
        _capacity = capacity;

        data = (T*)pen::memory_alloc(sizeof(T) * _capacity.load());
        memset(data, 0x0, sizeof(T) * _capacity.load());
    }

    template <typename T>
    pen_inline void ring_buffer<T>::put(const T& item)
    {
        data[put_pos] = item;
        put_pos = (put_pos + 1) % _capacity;
    }

    template <typename T>
    pen_inline T* ring_buffer<T>::get()
    {
        u32 gp = get_pos;
        if (gp == put_pos)
            return nullptr;

        get_pos = (get_pos + 1) % _capacity;

        return &data[gp];
    }

    template <typename T>
    pen_inline T* ring_buffer<T>::check()
    {
        u32 gp = get_pos;
        if (gp == put_pos)
            return nullptr;
        return &data[gp];
    }

    template <typename T>
    pen_inline res_pool<T>::res_pool()
    {
        _capacity = 0;
    }

    template <typename T>
    pen_inline res_pool<T>::~res_pool()
    {
        pen::memory_free(_resources);
    }

    template <typename T>
    pen_inline void res_pool<T>::init(u32 reserved_capacity)
    {
        _capacity = reserved_capacity;
        _resources = (T*)pen::memory_alloc(sizeof(T) * reserved_capacity);

        memset(_resources, 0x0, sizeof(T) * _capacity);
    }

    template <typename T>
    pen_inline void res_pool<T>::grow(u32 min_capacity)
    {
        if (_capacity <= min_capacity)
        {
            size_t new_cap = (min_capacity * 2);

            _resources = (T*)pen::memory_realloc(_resources, new_cap * sizeof(T));

            size_t existing_offset = _capacity * sizeof(T);
            memset(((u8*)_resources) + existing_offset, 0x0, sizeof(T) * _capacity);

            _capacity = new_cap;
        }
    }

    template <typename T>
    pen_inline void res_pool<T>::insert(const T& resource, u32 slot)
    {
        grow(slot);
        memcpy(&_resources[slot], &resource, sizeof(T));
        //_resources[slot] = resource;
    }

    template <typename T>
    pen_inline T& res_pool<T>::get(u32 slot)
    {
        return _resources[slot];
    }

    template <typename T>
    pen_inline T& res_pool<T>::operator[](u32 slot)
    {
        return _resources[slot];
    }

    template <typename T, u32 N>
    pen_inline multi_buffer<T, N>::multi_buffer()
    {
        _bb = 0;
        _fb = 1;
    }

    template <typename T, u32 N>
    pen_inline T& multi_buffer<T, N>::backbuffer()
    {
        return _data[_bb];
    }

    template <typename T, u32 N>
    pen_inline const T& multi_buffer<T, N>::frontbuffer()
    {
        return _data[_fb];
    }

    template <typename T, u32 N>
    pen_inline void multi_buffer<T, N>::swap_buffers()
    {
        _fb = (_fb + 1) % N;
        _bb = (_bb + 1) % N;
        _swaps++;
    }

    template <typename T, size_t N>
    pen_inline multi_array_buffer<T, N>::multi_array_buffer()
    {
        _bb = 0;
        _fb = 1;
    }

    template <typename T, size_t N>
    pen_inline multi_array_buffer<T, N>::~multi_array_buffer()
    {
        for (size_t i = 0; i < N; ++i)
            pen::memory_free(_data[i]);
    }

    template <typename T, size_t N>
    pen_inline T* multi_array_buffer<T, N>::backbuffer()
    {
        return _data[_bb];
    }

    template <typename T, size_t N>
    pen_inline const T* multi_array_buffer<T, N>::frontbuffer()
    {
        return _data[_fb];
    }

    template <typename T, size_t N>
    pen_inline void multi_array_buffer<T, N>::swap_buffers()
    {
        _fb = (_fb + 1) % N;
        _bb = (_bb + 1) % N;
        _swaps++;
    }

    template <typename T, size_t N>
    pen_inline void multi_array_buffer<T, N>::init(size_t size)
    {
        for (size_t i = 0; i < N; ++i)
        {
            _data[i] = (T*)pen::memory_alloc(sizeof(T) * size);
            memset(_data[i], 0x00, sizeof(T) * size);
            _capacity[i] = size;
        }
    }

    template <typename T, size_t N>
    pen_inline void multi_array_buffer<T, N>::grow(size_t size)
    {
        if (_capacity[_bb].load() >= size)
            return;

        _data[_bb] = (T*)pen::memory_realloc(_data[_bb], sizeof(T) * size);

        // zero the rest
        size_t cur = _capacity[_bb].load();
        size_t diff = size - cur;
        memset(&_data[cur], 0x00, sizeof(T) * diff);
        _capacity[_bb] = size;
    }

    template <typename T>
    pen_inline void mpmc_stretchy_buffer<T>::push_back(T item)
    {
        u32 cc = sb_count(_data[_fb]);
        if (cc > 0 && cc < stb__sbm(_data[_fb]))
        {
            sb_push(_data[_fb], item);
        }
        else
        {
            // lock and resize
            pen::mutex_lock(_mut);

            if (cc == 0 || cc >= stb__sbm(_data[_fb]))
            {
                u32 bb = _fb ^ 1;
                sb_free(_data[bb]);
                _data[bb] = nullptr;

                // todo: this could be improved with memcpy
                for (u32 i = 0; i < cc; ++i)
                    sb_push(_data[bb], _data[_fb][i]);

                // swap buffers
                _fb ^= 1;
            }

            pen::mutex_unlock(_mut);

            // push new entry
            sb_push(_data[_fb], item);
        }
    }

    template <typename T>
    pen_inline size_t mpmc_stretchy_buffer<T>::size()
    {
        return sb_count(_data[_fb]);
    }

    template <typename T>
    pen_inline T& mpmc_stretchy_buffer<T>::operator[](size_t slot)
    {
        return _data[_fb][slot];
    }
} // namespace pen
