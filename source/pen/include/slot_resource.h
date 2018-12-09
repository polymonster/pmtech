#ifndef _slot_resource_h
#define _slot_resource_h

// Simple slot resource api can be used to allocate an array slot to a generic opaque resource via a handle.
// Implements a free list so getting a new resource slot is an o(1) operation.

#include "pen.h"

namespace pen
{
    enum e_resource_flags
    {
        RESOURCE_FREE = 1,
        RESOURCE_USED = 1 << 1
    };

    struct free_slot_list
    {
        u32             index;
        free_slot_list* next;
        u32             flags;
    };

    struct slot_resources
    {
        free_slot_list* slots;
        free_slot_list* head;
    };
    
    // Function decl
    
    void slot_resources_init(slot_resources* resources, u32 num);
    u32  slot_resources_get_next(slot_resources* resources);
    bool slot_resources_free(slot_resources* resources, const u32 slot);

    // Implementation
    
    inline void slot_resources_init(slot_resources* resources, u32 num)
    {
        resources->slots = new free_slot_list[num];

        // 0 is reserved as null slot
        for (s32 i = num - 1; i > 0; --i)
        {
            resources->slots[i].index = i;

            if (i >= num - 1)
                resources->slots[i].next = nullptr;
            else
                resources->slots[i].next = resources->head;

            resources->head = &resources->slots[i];

            resources->slots[i].flags |= RESOURCE_FREE;
        }
    }

    inline u32 slot_resources_get_next(slot_resources* resources)
    {
        u32 r = resources->head->index;
        resources->head->flags &= ~RESOURCE_FREE;
        resources->head->flags |= RESOURCE_USED;

        resources->head = resources->head->next;

        return r;
    }

    inline bool slot_resources_free(slot_resources* resources, const u32 slot)
    {
        if (slot == 0)
            return false;

        // avoid double free
        if (resources->slots[slot].flags & RESOURCE_FREE)
            return false;

        // mark free and add to free list
        resources->slots[slot].flags |= RESOURCE_FREE;
        resources->slots[slot].next = resources->head;
        resources->head = &resources->slots[slot];

        return true;
    }
} // namespace pen

#endif
