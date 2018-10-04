#ifndef _hash_h
#define _hash_h

#include "str/Str.h"
#include "types.h"
//#include <string.h>

namespace pen
{
    // Hash class for all your hashing needs
    // use hashes as much as possible instead of
    // string compares :)

    class HashMurmur2A
    {
      public:
        void begin(uint32_t _seed = 0);
        void add(const void* _data, int _len);
        void addAligned(const void* _data, int _len);
        void addUnaligned(const void* _data, int _len);

        template <typename Ty>
        void add(Ty _value);

        uint32_t end();

      private:
        static void readUnaligned(const void* _data, uint32_t& _out);
        void        mixTail(const uint8_t*& _data, int& _len);

        uint32_t m_hash;
        uint32_t m_tail;
        uint32_t m_count;
        uint32_t m_size;
    };

    uint32_t hashMurmur2A(const void* _data, uint32_t _size);

    template <typename Ty>
    uint32_t hashMurmur2A(const Ty& _data);

    template <>
    uint32_t hashMurmur2A(const Str& s);

    uint32_t hashMurmur2A(const char* _data);

    uint32_t hashMurmur2A(char* _data);

    typedef HashMurmur2A hash_murmur;
} // namespace pen

#define PEN_HASH(V) pen::hashMurmur2A(V)

#include "hash.inl"

#endif
