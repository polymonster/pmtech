#include "pen_json.h"
#include "file_system.h"
#include "pen_string.h"
#include "memory.h"
#include "../third_party/jsmn/jsmn.c"

namespace pen
{
#define STRICT_NAME(V)V.append('\"')
#define NON_STRICT_NAME(V)
#define JSON_NAME NON_STRICT_NAME
    
    void create_json_object( json_object& jo );
    
    static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
        if ((int) strlen(s) == tok->end - tok->start &&
            strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
            return 0;
        }
        return -1;
    }
    
    static int _dump( Str& output, const char *js, jsmntok_t *t, size_t count, int indent )
    {
        int i, j, k;
        if (count == 0) {
            return 0;
        }
        if (t->type == JSMN_PRIMITIVE || t->type == JSMN_STRING)
        {
            if( t->type == JSMN_STRING)
                output.append('\"');
                
            for( s32 c = 0; c < t->end - t->start; ++c )
                output.append(*(js+t->start+c));
            
            if( t->type == JSMN_STRING)
                output.append('\"');
            return 1;
        } else if (t->type == JSMN_OBJECT) {
            output.append("\n");
            for (k = 0; k < indent; k++)
                output.append("\t");
            output.append("{\n");
            j = 0;
            for (i = 0; i < t->size; i++) {
                for (k = 0; k < indent+1; k++)
                    output.append("\t");
                j += _dump(output, js, t+1+j, count-j, indent+1);
                output.append(": ");
                j += _dump(output, js, t+1+j, count-j, indent+1);
                output.append(",\n");
            }
            for (k = 0; k < indent; k++)
                output.append("\t");
            output.append("}");
            return j+1;
        } else if (t->type == JSMN_ARRAY) {
            j = 0;
            output.append("[");
            for (i = 0; i < t->size; i++) {
                j += _dump(output, js, t+1+j, count-j, indent+1);
                if(i < t->size-1)
                    output.append(", ");
            }
            output.append("]");
            return j+1;
        }
        return 0;
    }
    
    struct json_object
    {
        jsmntok_t* tokens;
        s32        num_tokens;
        c8*        data;
        u32        size;
        c8*        name;
    
        json_object get_object_by_name( const c8* name );
        json_object get_object_by_index( const u32 index );
        
        Str         get_name();
    };
    
    union json_value
    {
        bool b;
        u32 u;
        s32 s;
        f32 f;
        const c8* str;
        json_object object;
    };
    
    enum PRIMITIVE_TYPE
    {
        JSON_STR = 0,
        JSON_U32,
        JSON_S32,
        JSON_F32,
        JSON_BOOL
    };
    
    struct enumerate_params
    {
        bool            get_next;
        int             return_value;
        jsmntok_t*      name_token;
    };
    
    static bool enumerate_primitve( const char *js, jsmntok_t *t, json_value& result, PRIMITIVE_TYPE  type )
    {
        if (t->type == JSMN_PRIMITIVE)
        {
            switch( type )
            {
                case JSON_STR:
                    result.str = js;
                    break;
                case JSON_U32:
                {
                    c8* tok_str = pen::sub_string(js + t->start, t->end - t->start);
                    result.u = atoi( tok_str );
                    pen::memory_free(tok_str);
                }
                    break;
                case JSON_S32:
                {
                    c8* tok_str = pen::sub_string(js + t->start, t->end - t->start);
                    result.s = atol( tok_str );
                    pen::memory_free( tok_str );
                }
                    break;
                case JSON_F32:
                {
                    c8* tok_str = pen::sub_string(js + t->start, t->end - t->start);
                    result.f = (f32)atof( tok_str );
                    pen::memory_free( tok_str );
                }
                    break;
                case JSON_BOOL:
                    if( *(js + t->start) == 't' )
                    {
                        result.b = true;
                        return true;
                    }
                    else if( *(js + t->start) == 'f' )
                    {
                        result.b = false;
                        return true;
                    }
                    return false;
                    break;
                    
            }
            
            return true;
        }
        
        return false;
    }

    static int enumerate( const char *js, jsmntok_t *t, size_t count, int indent, const c8* search_name, s32 search_index, json_value& result, enumerate_params& ep )
    {
        if( ep.return_value != 0)
            return ep.return_value;
        
        int i, j;
        if (count == 0)
            return 0;
        
        if (t->type == JSMN_PRIMITIVE)
        {
            if(result.object.name)
                pen::memory_free(result.object.name);
            
            result.object.name = pen::sub_string(js+t->start, t->end - t->start);
            
            if( indent==1  )
            {
                if( search_name == nullptr )
                {
                    ep.get_next = true;
                }
                else if( jsoneq(js, t, search_name) == 0 )
                {
                    ep.get_next = true;
                }
            }
            return 1;
        }
        else if (t->type == JSMN_STRING)
        {
            if(result.object.name)
                pen::memory_free(result.object.name);
            
            result.object.name = pen::sub_string(js+t->start, t->end - t->start);
            
            if( indent==1  )
            {
                if( search_name == nullptr )
                {
                    ep.get_next = true;
                }
                else if( jsoneq(js, t, search_name) == 0 )
                {
                    ep.get_next = true;
                }
            }
            return 1;
        }
        else if (t->type == JSMN_OBJECT)
        {
            j = 0;
            for (i = 0; i < t->size; i++)
            {
                j += enumerate(js, t+1+j, count-j, indent+1, search_name, search_index, result, ep );
                
                if( (indent == 0 && search_index == i) || ep.get_next )
                {
                    jsmntok_t* at = t+1+j;
                    
                    result.object.data = pen::sub_string(js+at->start, at->end - at->start);
                    result.object.size = at->end - at->start;
                
                    create_json_object( result.object );
                    
                    ep.return_value = 1;
                    ep.get_next = false;
                    return 1;
                }
                
                j += enumerate(js, t+1+j, count-j, indent+1, search_name, search_index, result, ep );
            }
            return j+1;
        }
        else if (t->type == JSMN_ARRAY)
        {
            if( ep.get_next)
            {
                result.object.data = pen::sub_string(js+t->start, t->end - t->start);
                result.object.size = t->end - t->start;
                
                create_json_object( result.object );
                
                ep.return_value = 1;
                ep.get_next = false;
                return 1;
            }
            
            j = 0;
            for (i = 0; i < t->size; i++)
            {
                if( indent == 0 && search_index == i )
                {
                    jsmntok_t* at = t+1+j;
                    
                    result.object.data = pen::sub_string(js+at->start, at->end - at->start);
                    result.object.size = at->end - at->start;
                    
                    create_json_object( result.object );
                    
                    ep.return_value = 1;
                    ep.get_next = false;
                    return 1;
                }
                
                j += enumerate(js, t+1+j, count-j, indent+1, search_name, search_index, result, ep );
            }
            return j+1;
        }
        
        return ep.return_value;
    }
    
    bool as_value( json_value& jv, json_object* jo, PRIMITIVE_TYPE type  )
    {
        if( jo->num_tokens <= 0 || jo->num_tokens > 1 )
            return false;
        
        return enumerate_primitve( jo->data, jo->tokens, jv, type );
    }
    
    json_object get_object( json_object* jo, const c8* name, s32 index )
    {
        json_value jv;
        jv.object.name = nullptr;
        jv.object.tokens = nullptr;
        jv.object.data = nullptr;
        jv.object.num_tokens = 0;
        
        enumerate_params ep =
        {
            false,
            0,
            nullptr
        };
        
        enumerate( jo->data, jo->tokens, jo->num_tokens, 0, name, index, jv, ep );
        
        return jv.object;
    }
    
    Str json_object::get_name()
    {
        return Str(this->name);
    }
    
    json_object json_object::get_object_by_name( const c8* name )
    {
        return get_object( this, name, -1 );
    }
    
    json_object json_object::get_object_by_index( const u32 index )
    {
        return get_object( this, "__unused_ai__", index );
    }
    
    void create_json_object( json_object& jo )
    {
        jsmn_parser p;
        
        //default try 64 tokens
        u32 token_count = 64;
        jo.tokens = new jsmntok_t[token_count];
        
        bool loaded = false;
        while( !loaded )
        {
            jsmn_init(&p);
            jo.num_tokens = jsmn_parse(&p, jo.data, jo.size, jo.tokens, token_count);
            if (jo.num_tokens < JSMN_ERROR_NOMEM)
            {
                PEN_PRINTF("Failed to parse JSON: %d\n", jo.num_tokens);
                break;
            }
            else if( jo.num_tokens == JSMN_ERROR_NOMEM )
            {
                //allocate space for more tokens
                delete jo.tokens;
                
                token_count <<= 1;
                jo.tokens = new jsmntok_t[token_count];
                
                continue;
            }

            loaded = true;
            break;
        }
        
        if(!loaded)
        {
            delete jo.tokens;
            jo.tokens = nullptr;
            
            jo.num_tokens = 0;
            
            pen::memory_free( jo.data );
            jo.data = nullptr;
        }
    }
    
    //------------------------------------------------------------------------------
    //C++ API
    //------------------------------------------------------------------------------
    json json::load_from_file( const c8* filename )
    {
        json new_json;
                
        void* data = nullptr;
        u32 size = 0;

        pen_error err = pen::filesystem_read_file_to_buffer(filename, &data, size); 

        if (err == PEN_ERR_OK)
        {
            new_json.m_internal_object = ( json_object* )memory_alloc( sizeof( json_object ) );
            new_json.m_internal_object->data = (c8*)data;
            new_json.m_internal_object->size = size;
            new_json.m_internal_object->name = nullptr;

            create_json_object( *new_json.m_internal_object );
        }

        return new_json;
    }
    
    json json::load( const c8* json_str )
    {
        json new_json;
        
        new_json.m_internal_object = (json_object*)memory_alloc(sizeof(json_object));
        
        new_json.m_internal_object->data = pen::sub_string(json_str, pen::string_length(json_str));
        
        new_json.m_internal_object->size = pen::string_length(json_str);
        new_json.m_internal_object->name = nullptr;
        
        create_json_object( *new_json.m_internal_object );
        
        return new_json;
    }
    
    enum combine_action
    {
        json_keep = 0,
        json_combine = 1,
        json_discard = 2
    };
    
    json json::combine( const json& j1, const json& j2, s32 indent )
    {
        //iterate member wise
        s32 s1 = j1.size();
        s32 s2 = j2.size();
        
        combine_action* j1_action = new combine_action[s1];
        combine_action* j2_action = new combine_action[s2];
        
        s32* combine_index = new s32[s1];
        
        pen::memory_set(j1_action, 0, sizeof(combine_action)*s1);
        pen::memory_set(j2_action, 0, sizeof(combine_action)*s2);
        
        Str json_string = "{\n";
        Str indent_str = "\t";
        
        for( s32 i = 0; i < s1; ++i )
        {
            json j3 = j1[i];
            
            Str name1 = j3.name();
            
            bool action = false;
            
            for( s32 j = 0; j < s2; ++j )
            {
                json j4 = j2[j];
                
                Str name2 = j4.name();
                
                if( name2 == name1 )
                {
                    if( j3.type() == JSMN_OBJECT && j4.type() == JSMN_OBJECT )
                    {
                        j1_action[i] = json_combine;
                        j2_action[j] = json_combine;
                        
                        combine_index[i] = j;
                    }
                    else
                    {
                        j1_action[i] = json_discard;
                        j2_action[j] = json_keep;
                    }
                    
                    action = true;
                }
            }
        }
        
        for( s32 i = 0; i < s1; ++i )
        {
            if( j1_action[i] == json_keep )
            {
                json_string.append(indent_str.c_str());
                JSON_NAME(json_string);
                json_string.append(j1[i].name().c_str());
                JSON_NAME(json_string);
                
                json_string.append(": ");
                json_string.append(j1[i].m_internal_object->data);
                json_string.append(",\n");
            }
            
            if( j1_action[i] == json_combine )
            {
                s32 j = combine_index[i];
                json combined = combine( j1[i], j2[j], indent+1 );
                
                json_string.append(indent_str.c_str());
                JSON_NAME(json_string);
                json_string.append(j1[i].name().c_str());
                JSON_NAME(json_string);
                json_string.append(":\n");
                
                json_string.append(combined.dumps().c_str());
                
                json_string.append(",\n");
            }
        }
        
        for( s32 i = 0; i < s2; ++i )
        {
            if( j2_action[i] == json_keep )
            {
                json_string.append(indent_str.c_str());
                JSON_NAME(json_string);
                json_string.append(j2[i].name().c_str());
                JSON_NAME(json_string);
                
                json_string.append(": ");
                json_string.append(j2[i].m_internal_object->data);
                json_string.append(",\n");
            }
        }
        
        json_string.append("}");
        json_string.append('\0');
        
        json test = json::load(json_string.c_str());
        
        delete[] j1_action;
        delete[] j2_action;
        delete[] combine_index;
        
        return test;
    }
    
    u32 json::size() const
    {
        if( m_internal_object->num_tokens > 0 )
            if( m_internal_object->tokens[0].type == JSMN_ARRAY || m_internal_object->tokens[0].type == JSMN_OBJECT )
                return m_internal_object->tokens[0].size;
        
        return 0;
    }
    
    json json::operator [] (const c8* name) const
    {
        json new_json;
        
        new_json.m_internal_object = (json_object*)memory_alloc(sizeof(json_object));

        *new_json.m_internal_object = m_internal_object->get_object_by_name(name);
        return new_json;
    }
    
    json json::operator [] (const u32 index) const
    {
        json new_json;
        
        new_json.m_internal_object = (json_object*)memory_alloc(sizeof(json_object));
        
        *new_json.m_internal_object = m_internal_object->get_object_by_index(index);
        return new_json;
    }
    
    json json::operator [] (const s32 index) const
    {
        return this->operator[]((u32)index);
    }

    json::json( )
    {
        m_internal_object = nullptr;
    }
    
    void json::copy( json* dst, const json& other )
    {
        dst->m_internal_object = nullptr;

        if (other.m_internal_object == nullptr)
            return;

        dst->m_internal_object = (json_object*)memory_alloc(sizeof(json_object));
        
        //shallow copy defauly copy ctor
        *dst->m_internal_object = *other.m_internal_object;
        
        u32 num_tokens = other.m_internal_object->num_tokens;
        dst->m_internal_object->tokens = new jsmntok_t[num_tokens];
        pen::memory_cpy(dst->m_internal_object->tokens, other.m_internal_object->tokens, sizeof(jsmntok_t)*num_tokens);
        
        //deep copy take ownership of mem
        s32 data_size = string_length(other.m_internal_object->data);
        
        dst->m_internal_object->data = (c8*)memory_alloc(data_size+1);
        pen::memory_cpy(dst->m_internal_object->data, other.m_internal_object->data, data_size);
        dst->m_internal_object->data[data_size] = '\0';
        
        if( other.m_internal_object->name )
        {
            s32 name_size = string_length(other.m_internal_object->name);
            m_internal_object->name = (c8*)memory_alloc(name_size);
            pen::memory_cpy(m_internal_object->name, other.m_internal_object->name, name_size);
        }
    }
    
    json::json( const json& other )
    {
        copy( this, other );
    }
    
    json& json::operator = (const json& other )
    {
        copy( this, other );
        
        return *this;
    }
    
    Str json::as_str( const c8* default_value )
    {
        json_value jv;
        if( as_value(jv, m_internal_object, JSON_STR ) )
            return m_internal_object->data;
        
        return default_value;
    }
    
    u32 json::as_u32( u32 default_value )
    {
        json_value jv;
        if( as_value(jv, m_internal_object, JSON_U32 ) )
            return jv.u;
        
        return default_value;
    }
    
    s32 json::as_s32( s32 default_value )
    {
        json_value jv;
        if( as_value(jv, m_internal_object, JSON_S32 ) )
            return jv.s;
        
        return default_value;
    }
    
    bool json::as_bool( bool default_value )
    {
        json_value jv;
        if( as_value(jv, m_internal_object, JSON_BOOL ) )
            return jv.b;
        
        return default_value;
    }
    
    f32 json::as_f32( f32 default_value )
    {
        json_value jv;
        if( as_value(jv, m_internal_object, JSON_F32 ) )
            return jv.f;
        
        return default_value;
    }
    
    Str json::dumps()
    {
        Str t;
        _dump( t, m_internal_object->data, m_internal_object->tokens, m_internal_object->num_tokens, 0);
        return t;
    }
    
    Str json::name()
    {
        return m_internal_object->get_name();
    }
    
    jsmntype_t json::type()
    {
        if(!m_internal_object)
            return JSMN_UNDEFINED;

        if(m_internal_object->num_tokens <= 0)
            return JSMN_UNDEFINED;
        
        return m_internal_object->tokens[0].type;
    }
    
    json::~json()
    {
        if(m_internal_object)
        {
            delete[] m_internal_object->tokens;
            
            pen::memory_free(m_internal_object->data);
            pen::memory_free(m_internal_object->name);
        }
        
        pen::memory_free(m_internal_object);
        m_internal_object = nullptr;
    }
    
    void json::set(const c8* name, const Str val)
    {
        Str new_json_object = "{";
        JSON_NAME(new_json_object);
        new_json_object.append(name);
        JSON_NAME(new_json_object);
        new_json_object.append(":");
        new_json_object.append(val.c_str());
        new_json_object.append("}");
        
        pen::json json_set = pen::json::load(new_json_object.c_str());
        
        if (m_internal_object)
        {
            pen::json combined = combine( *this, json_set );

            //free mem and copy combined
            this->~json();
            *this = combined;
        }
        else
        {
            *this = json_set;
        }
    }
}
