/*
 * Copyright 2010-2017 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bx#license-bsd-2-clause
 */

#ifndef _hash_h
#	error "Must be included from hash.h!"
#endif

namespace pen
{
#define MURMUR_M 0x5bd1e995
#define MURMUR_R 24
#define mmix(_h, _k) { _k *= MURMUR_M; _k ^= _k >> MURMUR_R; _k *= MURMUR_M; _h *= MURMUR_M; _h ^= _k; }
    
    inline bool isAligned(const void* _ptr, size_t _align)
    {
        union { const void* ptr; uintptr_t addr; } un;
        un.ptr = _ptr;
        return 0 == (un.addr & (_align-1) );
    }

	inline void HashMurmur2A::begin(uint32_t _seed)
	{
		m_hash = _seed;
		m_tail = 0;
		m_count = 0;
		m_size = 0;
	}

	inline void HashMurmur2A::add(const void* _data, int _len)
	{
		if (!isAligned(_data, 4))
		{
			addUnaligned(_data, _len);
			return;
		}

		addAligned(_data, _len);
	}

	inline void HashMurmur2A::addAligned(const void* _data, int _len)
	{
		const uint8_t* data = (const uint8_t*)_data;
		m_size += _len;

		mixTail(data, _len);

		while(_len >= 4)
		{
			uint32_t kk = *(uint32_t*)data;

			mmix(m_hash, kk);

			data += 4;
			_len -= 4;
		}

		mixTail(data, _len);
	}

	inline void HashMurmur2A::addUnaligned(const void* _data, int _len)
	{
		const uint8_t* data = (const uint8_t*)_data;
		m_size += _len;

		mixTail(data, _len);

		while(_len >= 4)
		{
			uint32_t kk;
			readUnaligned(data, kk);

			mmix(m_hash, kk);

			data += 4;
			_len -= 4;
		}

		mixTail(data, _len);
	}

	template<typename Ty>
	inline void HashMurmur2A::add(Ty _value)
	{
		add(&_value, sizeof(Ty) );
	}

	inline uint32_t HashMurmur2A::end()
	{
		mmix(m_hash, m_tail);
		mmix(m_hash, m_size);

		m_hash ^= m_hash >> 13;
		m_hash *= MURMUR_M;
		m_hash ^= m_hash >> 15;

		return m_hash;
	}

	inline void HashMurmur2A::readUnaligned(const void* _data, uint32_t& _out)
	{
		const uint8_t* data = (const uint8_t*)_data;
		
        if ( 0 ) //big endian PPC.. any other platdorms todo
		{
			_out = 0
				| data[0]<<24
				| data[1]<<16
				| data[2]<<8
				| data[3]
				;
		}
		else
		{
			_out = 0
				| data[0]
				| data[1]<<8
				| data[2]<<16
				| data[3]<<24
				;
		}
	}

	inline void HashMurmur2A::mixTail(const uint8_t*& _data, int& _len)
	{
		while( _len && ((_len<4) || m_count) )
		{
			m_tail |= (*_data++) << (m_count * 8);

			m_count++;
			_len--;

			if(m_count == 4)
			{
				mmix(m_hash, m_tail);
				m_tail = 0;
				m_count = 0;
			}
		}
	}

#undef MURMUR_M
#undef MURMUR_R
#undef mmix

	inline uint32_t hashMurmur2A(const void* _data, uint32_t _size)
	{
		HashMurmur2A murmur;
		murmur.begin();
		murmur.add(_data, (int)_size);
		return murmur.end();
	}

	template <typename Ty>
	inline uint32_t hashMurmur2A(const Ty& _data)
	{
        //assert on non-pod?
        
		return hashMurmur2A(&_data, sizeof(Ty) );
	}

    /*
	inline uint32_t hashMurmur2A(const StringView& _data)
	{
		return hashMurmur2A(_data.getPtr(), _data.getLength() );
	}

	inline uint32_t hashMurmur2A(const char* _data)
	{
		return hashMurmur2A(StringView(_data) );
	}
    */

} // namespace bx
