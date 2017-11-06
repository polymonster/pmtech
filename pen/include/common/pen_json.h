#ifndef _pen_json_h
#define _pen_json_h

#include "definitions.h"
#include "jsmn/jsmn.h"
#include "str/Str.h"

namespace pen
{
    struct json_object;
    
    class json
    {
    public:
        ~json();
        
        static json load_from_file( c8* filename );
        static json load( c8* json_str );
        
        u32        size() const;
        
        json operator [] (const c8* name) const;
        json operator [] (const u32 index) const;
        
        Str     as_str();
        u32     as_u32();
        s32     as_s32();
        bool    as_bool();
        f32     as_f32();
        
    private:
        json_object* m_internal_object;
    };
}

#endif

