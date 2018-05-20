// Str v0.25
// Simple c++ string type with an optional local buffer, by omar cornut
// https://github.com/ocornut/str

// LICENSE
// This software is in the public domain. Where that dedication is not
// recognized, you are granted a perpetual, irrevocable license to copy,
// distribute, and modify this file as you see fit.

/*
- This isn't a fully featured string class. 
- It is a simple, bearable replacement to std::string that isn't heap abusive nor bloated (can actually be debugged by humans).
- String are mutable. We don't maintain size so length() is not-constant time. 
- Maximum string size currently limited to 2 MB (we allocate 21 bits to hold capacity).
- Local buffer size is currently limited to 1023 bytes (we allocate 10 bits to hold local buffer size).
- In "non-owned" mode for literals/reference we don't do any tracking/counting of references.
- Overhead is 8-bytes in 32-bits, 16-bits in 64-bits (12 + alignment).
- This code hasn't been tested very much. it is probably incomplete or broken. Made it for my own use.

The idea is that you can provide an arbitrary sized local buffer if you expect string to fit
most of the time, and then you avoid using costly heap.

No local buffer, always use heap, sizeof()==8~16 (depends if your pointers are 32-bits or 64-bits)

   Str s = "hey";

With a local buffer of 16 bytes, sizeof() == 8~16 + 16 bytes.

   Str16 s = "filename.h"; // copy into local buffer
   Str16 s = "long_filename_not_very_long_but_longer_than_expected.h";   // use heap

With a local buffer of 256 bytes, sizeof() == 8~16 + 256 bytes.

   Str256 s = "long_filename_not_very_long_but_longer_than_expected.h";  // copy into local buffer

Common sizes are defined at the bottom of Str.h, you may define your own.

Functions:

   Str256 s;
   s.set("hello sailor");                   // set (copy)
   s.setf("%s/%s.tmp", folder, filename);   // set (w/format)
   s.append("hello");                       // append. cost a length() calculation!
   s.appendf("hello %d", 42);               // append (w/format). cost a length() calculation!
   s.set_ref("Hey!");                       // set (literal/reference, just copy pointer, no tracking)

Constructor helper for format string: add a trailing 'f' to the type. Underlying type is the same.

   Str256f filename("%s/%s.tmp", folder, filename);             // construct (w/format)
   fopen(Str256f("%s/%s.tmp, folder, filename).c_str(), "rb");  // construct (w/format), use as function param, destruct

Constructor helper for reference/literal:

   StrRef ref("literal");                   // copy pointer, no allocation, no string copy
   StrRef ref2(GetDebugName());             // copy pointer. no tracking of anything whatsoever, know what you are doing!

All StrXXX types derives from Str and instance hold the local buffer capacity. So you can pass e.g. Str256* to a function taking base type Str* and it will be functional.

   void MyFunc(Str& s) { s = "Hello"; }     // will use local buffer if available in Str instance

(Using a template e.g. Str<N> we could remove the LocalBufSize storage but it would make passing typed Str<> to functions tricky.
 Instead we don't use template so you can pass them around as the base type Str*. Also, templates are ugly.)
*/

/*
 CHANGELOG
  0.25 - allow set(const char* NULL) or operator= NULL to clear the string. not that set() from range or other types are not allowed.
  0.24 - allow set_ref(const char* NULL) to clear the string. include fixes for linux.
  0.23 - added append(char). added append_from(int idx, XXX) functions. fixed some compilers warnings.
  0.22 - documentation improvements, comments. fixes for some compilers.
  0.21 - added StrXXXf() constructor to construct directly from a format string. 
*/

/*
TODO
- Since we lose 4-bytes of padding on 64-bits architecture, perhaps just spread the header to 8-bytes and lift size limits? 
- More functions/helpers.
*/

#pragma once

// Configuration
#ifndef STR_MEMALLOC
#define STR_MEMALLOC  malloc
#include <stdlib.h>
#endif
#ifndef STR_MEMFREE
#define STR_MEMFREE   free
#include <stdlib.h>
#endif
#ifndef STR_ASSERT
#define STR_ASSERT    assert
#include <assert.h>
#endif
#include <stdarg.h>   // for va_list

// Configuration: #define STR_SUPPORT_STD_STRING 0 to disable setters variants using const std::string& (on by default)
#ifndef STR_SUPPORT_STD_STRING
#define STR_SUPPORT_STD_STRING  1
#endif

#ifdef STR_SUPPORT_STD_STRING
#include <string>
#include <string.h>
#endif

// This is the base class that you can pass around
// Footprint is 8-bytes (32-bits arch) or 16-bytes (64-bits arch)
class Str
{
    char*               Data;                   // Point to LocalBuf() or heap allocated
    int                 Capacity : 21;          // Max 2 MB
    int                 LocalBufSize : 10;      // Max 1023 bytes
    unsigned int        Owned : 1;              // Set when we have ownership of the pointed data (most common, unless using set_ref() method or StrRef constructor)

public:
    inline char*        c_str()                                 { return Data; }
    inline const char*  c_str() const                           { return Data; }
    inline bool         empty() const                           { return Data[0] == 0; }
    inline int          length() const                          { return (int)strlen(Data); }    // by design, allow user to write into the buffer at any time
    inline int          capacity() const                        { return Capacity; }

    inline void         set_ref(const char* src);
    int                 setf(const char* fmt, ...);
    int                 setfv(const char* fmt, va_list args);
    int                 setf_nogrow(const char* fmt, ...);
    int                 setfv_nogrow(const char* fmt, va_list args);
	int                 append(char c);
    int                 append(const char* s, const char* s_end = NULL);
    int                 appendf(const char* fmt, ...);
    int                 appendfv(const char* fmt, va_list args);
	int                 append_from(int idx, char c);
	int                 append_from(int idx, const char* s, const char* s_end = NULL);		// If you know the string length or want to append from a certain point
	int                 appendf_from(int idx, const char* fmt, ...);
	int                 appendfv_from(int idx, const char* fmt, va_list args);

    void                clear();
    void                reserve(int cap);
    void                reserve_discard(int cap);
    void                shrink_to_fit();

    inline char&        operator[](size_t i)                    { return Data[i]; }
    inline char         operator[](size_t i) const              { return Data[i]; }
    //explicit operator const char*() const{ return Data; }

    inline Str();
    inline Str(const char* rhs);
    inline void         set(const char* src);
    inline void         set(const char* src, const char* src_end);
    inline Str&         operator=(const char* rhs)              { set(rhs); return *this; }
    inline bool         operator==(const char* rhs) const       { return strcmp(c_str(), rhs) == 0; }

    inline Str(const Str& rhs);
    inline void         set(const Str& src);
    inline Str&         operator=(const Str& rhs)               { set(rhs); return *this; }
    inline bool         operator==(const Str& rhs) const        { return strcmp(c_str(), rhs.c_str()) == 0; }

#if STR_SUPPORT_STD_STRING
    inline Str(const std::string& rhs);
    inline void         set(const std::string& src);
    inline Str&         operator=(const std::string& rhs)       { set(rhs); return *this; }
    inline bool         operator==(const std::string& rhs)const { return strcmp(c_str(), rhs.c_str()) == 0; }
#endif

    // Destructor for all variants
    inline ~Str()
    {
        if (Owned && !is_using_local_buf())
            STR_MEMFREE(Data);
    }

    static char*        EmptyBuffer;

protected:
    inline char*        local_buf()                             { return (char*)this + sizeof(Str); }
    inline const char*  local_buf() const                       { return (char*)this + sizeof(Str); }
    inline bool         is_using_local_buf() const              { return Data == local_buf() && LocalBufSize != 0; }

    // Constructor for StrXXX variants with local buffer
    Str(unsigned short local_buf_size)
    {
        STR_ASSERT(local_buf_size < 1024);
        Data = local_buf();
        Data[0] = '\0';
        Capacity = local_buf_size;
        LocalBufSize = local_buf_size;
        Owned = 1;
    }
};

void    Str::set(const char* src)
{
    // We allow set(NULL) or via = operator to clear the string.
    if (src == NULL)
    {
        clear();
        return;
    }
    int buf_len = (int)strlen(src)+1;
    if ((int)Capacity < buf_len)
        reserve_discard(buf_len);
    memcpy(Data, src, buf_len);
    Owned = 1;
}

void    Str::set(const char* src, const char* src_end)
{
    STR_ASSERT(src != NULL && src_end >= src);
    int buf_len = (int)(src_end-src)+1;
    if ((int)Capacity < buf_len)
        reserve_discard(buf_len);
    memcpy(Data, src, buf_len-1);
    Data[buf_len] = 0;
    Owned = 1;
}

void    Str::set(const Str& src)
{
    int buf_len = (int)strlen(src.c_str())+1;
    if ((int)Capacity < buf_len)
        reserve_discard(buf_len);
    memcpy(Data, src.c_str(), buf_len);
    Owned = 1;
}

#if STR_SUPPORT_STD_STRING
void    Str::set(const std::string& src)
{
    int buf_len = (int)src.length()+1;
    if ((int)Capacity < buf_len)
        reserve_discard(buf_len);
    memcpy(Data, src.c_str(), buf_len);
    Owned = 1;
}
#endif

inline void Str::set_ref(const char* src)
{
    if (Owned && !is_using_local_buf())
        STR_MEMFREE(Data);
    Data = src ? (char*)src : EmptyBuffer;
    Capacity = 0;
    Owned = 0;
}

Str::Str()
{
    Data = EmptyBuffer;      // Shared READ-ONLY initial buffer for 0 capacity
    Capacity = 0;
    LocalBufSize = 0;
    Owned = 0;
}

Str::Str(const Str& rhs)
{
    Data = EmptyBuffer;
    Capacity = 0;
    LocalBufSize = 0;
    Owned = 0;
    set(rhs);
}

Str::Str(const char* rhs)
{
    Data = EmptyBuffer;
    Capacity = 0;
    LocalBufSize = 0;
    Owned = 0;
    set(rhs);
}

#if STR_SUPPORT_STD_STRING
Str::Str(const std::string& rhs)
{
    Data = EmptyBuffer;
    Capacity = 0;
    LocalBufSize = 0;
    Owned = 0;
    set(rhs);
}
#endif

// Literal/reference string
class StrRef : public Str
{
public:
    StrRef(const char* s) : Str() { set_ref(s); }
};

// Types embedding a local buffer
// NB: we need to override the constructor and = operator for both Str& and TYPENAME (without the later compiler will call a default copy operator)
#if STR_SUPPORT_STD_STRING

#define STR_DEFINETYPE(TYPENAME, LOCALBUFSIZE)                                      \
class TYPENAME : public Str                                                         \
{                                                                                   \
    char local_buf[LOCALBUFSIZE];                                                   \
public:                                                                             \
    TYPENAME() : Str(LOCALBUFSIZE) {}                                               \
    TYPENAME(const Str& rhs) : Str(LOCALBUFSIZE) { set(rhs); }                      \
    TYPENAME(const char* rhs) : Str(LOCALBUFSIZE) { set(rhs); }                     \
    TYPENAME(const TYPENAME& rhs) : Str(LOCALBUFSIZE) { set(rhs); }                 \
    TYPENAME(const std::string& rhs) : Str(LOCALBUFSIZE) { set(rhs); }              \
    TYPENAME&   operator=(const char* rhs)          { set(rhs); return *this; }     \
    TYPENAME&   operator=(const Str& rhs)           { set(rhs); return *this; }     \
    TYPENAME&   operator=(const TYPENAME& rhs)      { set(rhs); return *this; }     \
    TYPENAME&   operator=(const std::string& rhs)   { set(rhs); return *this; }     \
};

#else

#define STR_DEFINETYPE(TYPENAME, LOCALBUFSIZE)                                      \
class TYPENAME : public Str                                                         \
{                                                                                   \
    char local_buf[LOCALBUFSIZE];                                                   \
public:                                                                             \
    TYPENAME() : Str(LOCALBUFSIZE) {}                                               \
    TYPENAME(const Str& rhs) : Str(LOCALBUFSIZE) { set(rhs); }                      \
    TYPENAME(const char* rhs) : Str(LOCALBUFSIZE) { set(rhs); }                     \
    TYPENAME(const TYPENAME& rhs) : Str(LOCALBUFSIZE) { set(rhs); }                 \
    TYPENAME&   operator=(const char* rhs)          { set(rhs); return *this; }     \
    TYPENAME&   operator=(const Str& rhs)           { set(rhs); return *this; }     \
    TYPENAME&   operator=(const TYPENAME& rhs)      { set(rhs); return *this; }     \
};

#endif

// Helper to define StrXXXf constructors
#define STR_DEFINETYPE_F(TYPENAME, TYPENAME_F)                                      \
class TYPENAME_F : public TYPENAME                                                  \
{                                                                                   \
public:                                                                             \
    TYPENAME_F(const char* fmt, ...) : TYPENAME() { va_list args; va_start(args, fmt); setfv(fmt, args); va_end(args); } \
};

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-private-field"         // warning : private field 'local_buf' is not used
#endif

// Declaring types for common sizes here
STR_DEFINETYPE(Str16, 16)
STR_DEFINETYPE(Str32, 32)
STR_DEFINETYPE(Str64, 64)
STR_DEFINETYPE(Str128, 128)
STR_DEFINETYPE(Str256, 256)
STR_DEFINETYPE(Str512, 512)

// Declaring helper constructors to pass in format strings in one statement
STR_DEFINETYPE_F(Str16, Str16f)
STR_DEFINETYPE_F(Str32, Str32f)
STR_DEFINETYPE_F(Str64, Str64f)
STR_DEFINETYPE_F(Str128, Str128f)
STR_DEFINETYPE_F(Str256, Str256f)
STR_DEFINETYPE_F(Str512, Str512f)

#ifdef __clang__
#pragma clang diagnostic pop
#endif

//-------------------------------------------------------------------------
