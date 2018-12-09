#ifndef _pen_json_h
#define _pen_json_h

// C++ wrapper api for JSMN.
// Provides operators to access JSON objects and arrays and get retreive typed values.
// json file is kept in a char buffer and jsmn tokens are used to iterate.
// this api does not use any vectors or maps to store the json data.

// Examples:
// Load:
// json j = load_from_file("filename");
//
// Value by Key:
// json member = j["key"];
//
// As Type:
// u32 value = member.as_u32();
//
// json can be an object or an array
// query it with:
// j.get_type() == JSMN_ARRAY or JSMN_OBJECT
//
// Arrays:
// u32 num_array_elements = j.size()
// for(u32 i = 0; i < num_array_elements; ++i)
//        value = j[i].as_bool;
//
// Iterate Members:
// u32 num_members = j.size()
// for(u32 i = 0; i < num_members; ++i)
//        printf(j[0].name); // print member name
//
// Print:
// printf(j.dumps().c_str())

// To use unstrict json without the need for quotes around keys and string values
// care must be taken with filenames, colons (:) need to be stripped from filenames (ie C:\windows)
// use set_filename and as filen which will replace : with @ (ie C:@windows) or the inverse.

// Combine will combine members of j1 and j2 on an object by object, member by members basis
// if duplicate members exist j2.member will replace j1.member

// API for writing json is limited, if you want to write to nested members or arrays
// you will need to create copies of the objects and then manually recursively write the objects
// back upwards once you have written to a value (leaf).

#include "hash.h"
#include "jsmn/jsmn.h"
#include "pen.h"
#include "str/Str.h"

namespace pen
{
    struct json_object;
    class json;

    // functions
    Str to_str(const c8* val);
    Str to_str(const u32 val);
    Str to_str(const s32 val);
    Str to_str(const f32 val);
    Str to_str(const bool val);
    Str to_str(const json& val);

    class json
    {
      public:
        ~json();
        json();
        json(const json& other);

        static json load_from_file(const c8* filename);
        static json load(const c8* json_str);
        static json combine(const json& j1, const json& j2, s32 indent = 0);

        Str        dumps() const;
        Str        key() const;
        Str        name() const; // same as key
        jsmntype_t type() const;
        bool       is_null() const; // jsmntype_t == JSMN_UNDEFINED
        u32        size() const;

        json  operator[](const c8* name) const;
        json  operator[](const u32 index) const;
        json  operator[](const s32 index) const;
        json& operator=(const json& other);

        Str       as_str(const c8* default_value = nullptr) const;
        const c8* as_cstr(const c8* default_value = nullptr) const;
        hash_id   as_hash_id(hash_id default_value = 0) const;
        u32       as_u32(u32 default_value = 0) const;
        s32       as_s32(s32 default_value = 0) const;
        u64       as_u64(u64 default_value = 0) const;
        s64       as_s64(s64 default_value = 0) const;
        bool      as_bool(bool default_value = false) const;
        f32       as_f32(f32 default_value = 0.0f) const;
        u8        as_u8_hex(u8 default_value = 0) const;
        u32       as_u32_hex(u32 default_value = 0) const;
        Str       as_filename(const c8* default_value = nullptr) const;

        // set master functions
        void set(const c8* name, const Str val);
        void set_array(const c8* name, const Str* val, u32 count);
        void set_filename(const c8* name, const Str& filename);

        // set templated functions
        template <class T>
        inline void set(const c8* name, const T val)
        {
            Str fmt = to_str((T)val);
            set(name, fmt);
        }

        template <class T>
        inline void set_array(const c8* name, const T* val, u32 count)
        {
            Str* array = new Str[count];
            for (u32 i = 0; i < count; ++i)
                array[i] = to_str((T)val[i]);

            set_array(name, array, count);
            delete[] array;
        }

      private:
        json_object* m_internal_object;
        void         copy(json* dst, const json& other);
    };

    // inline functions
    inline Str to_str(const c8* val)
    {
        Str fmt;
        fmt.appendf("%u", val);
        return fmt;
    }

    inline Str to_str(const u32 val)
    {
        Str fmt;
        fmt.appendf("%u", val);
        return fmt;
    }

    inline Str to_str(const s32 val)
    {
        Str fmt;
        fmt.appendf("%i", val);
        return fmt;
    }

    inline Str to_str(const bool val)
    {
        Str fmt;
        if (val)
            fmt.appendf("true");
        else
            fmt.appendf("false");
        return fmt;
    }

    inline Str to_str(const f32 val)
    {
        Str fmt;
        fmt.appendf("%f", val);
        return fmt;
    }

    inline Str to_str(const json& val)
    {
        return val.dumps();
    }

} // namespace pen

#endif
