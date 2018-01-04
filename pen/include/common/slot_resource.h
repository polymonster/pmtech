#ifndef _slot_resource_h
#define _slot_resource_h

#include "definitions.h"

namespace pen
{
    struct free_slot_list
    {
        u32 index;
        free_slot_list* next;
    };
    
    struct slot_resources
    {
        free_slot_list* slots;
        free_slot_list* head;
    };
    
    inline u32 slot_resources_init( slot_resources* resources, u32 num )
    {
        resources->slots = new free_slot_list[num];
        
        //0 is reserved as null slot
        for( s32 i = num-1; i > 0; --i )
        {
            resources->slots[i].index = i;
            
            if(i >= num-1)
                resources->slots[i].next = nullptr;
            else
                resources->slots[i].next = resources->head;
            
            resources->head = &resources->slots[i];
        }
    }
    
    inline u32 slot_resources_get_next( slot_resources* resources )
    {
        u32 r = resources->head->index;
        resources->head = resources->head->next;
        
        return r;
    }
    
    inline void slot_resources_free( slot_resources* resources, u32 slot )
    {
        resources->slots[slot].next = resources->head;
        resources->head = &resources->slots[slot];
    }
}

#endif
