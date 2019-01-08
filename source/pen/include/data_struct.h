#ifndef _pen_data_struct_h
#define _pen_data_struct_h

// Minimalist data structures to use as bloat free alternative to vector et al.

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

template <typename T>
struct pen_stack
{
    T*  data = nullptr;
    int pos = 0;

    void clear()
    {
        pos = 0;
    }

    void push(T i)
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

    T pop()
    {
        --pos;
        if (pos < 0)
            pos = 0;

        return data[pos];
    }

    int size()
    {
        return pos;
    }
};

template <typename T>
struct pen_ring_buffer
{
    T* data = nullptr;

    a_u32 get_pos;
    a_u32 put_pos;
    a_u32 _capacity;

    pen_ring_buffer()
    {
        get_pos = 0;
        put_pos = 0;
        _capacity = 0;
    }

    void create(u32 capacity)
    {
        get_pos = 0;
        put_pos = 0;
        _capacity = capacity;

        data = new T[_capacity.load()];
    }

    ~pen_ring_buffer()
    {
        delete data;
    }

    void put(const T& item)
    {
        data[put_pos] = item;
        put_pos = (put_pos + 1) % _capacity;
    }

    T* get()
    {
        u32 gp = get_pos;
        if (gp == put_pos)
            return nullptr;

        get_pos = (get_pos + 1) % _capacity;

        return &data[gp];
    }
};

#endif //_pen_data_struct
