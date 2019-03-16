// data_struct.h
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#ifndef _pen_data_struct_h
#define _pen_data_struct_h

// Minimalist, fast, bloat free data structures with thread safe versions

#include "memory.h"

// Stretchy buffer itself akin to vector.. shoutout stb.

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
        T pop();
        int size();
    };

    // single producer single consumer - thread safe ring buffer
    template <typename T>
    struct ring_buffer
    {
        T* data = nullptr;

        a_u32 get_pos;
        a_u32 put_pos;
        a_u32 _capacity;

        ring_buffer();
        ~ring_buffer();
        
        void create(u32 capacity);
        void put(const T& item);
        T* get();
    };

    // single producer multiple consumer - thread safe resource pool which will grow to accomodate contents
    template <typename T>
    struct res_pool
    {
        T*  _resources = nullptr;
        a_u32 _capacity;
        a_u32 _size;
        
        res_pool();
        ~res_pool();
        
        void init(u32 reserved_capacity);
        void grow(u32 min_capacity);
        void insert(const T& resource, u32 slot);
        T& get(u32 slot);
    };

    // single produce multiple consumer - thread safe multi buffer
    template <typename T, u32 N>
    struct multi_buffer
    {
        T _data[N];
        u32   _num_buffers = N;
        a_u32 _fb;
        a_u32 _bb;

        multi_buffer();

        T& backbuffer();
        const T& frontbuffer();
        void swap_buffers();
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
    pen_inline void ring_buffer<T>::create(u32 capacity)
    {
        get_pos = 0;
        put_pos = 0;
        _capacity = capacity;
        
        data = (T*)pen::memory_alloc(sizeof(T) * _capacity.load());
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
    pen_inline res_pool<T>::res_pool()
    {
        _capacity = 0;
        _size = 0;
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
        
        _size = 0;
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
        
        _size = min_capacity;
    }
    
    template <typename T>
    pen_inline void res_pool<T>::insert(const T& resource, u32 slot)
    {
        grow(slot);
        _resources[slot] = resource;
    }
    
    template <typename T>
    pen_inline T& res_pool<T>::get(u32 slot)
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
        _fb = _bb.load();
        _bb = (_bb + 1) % _num_buffers;
    }
    
} // namespace pen

#endif //_pen_data_struct
