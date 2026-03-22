// physics_stub.cpp
// Copyright 2014 - 2025 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "physics.h"

namespace physics
{
    readable_data g_readable_data;

    void physics_initialise()
    {

    }

    void physics_shutdown()
    {

    }

    void update_output_matrices()
    {

    }

    void physics_update(f32 dt)
    {

    }

    void add_rb_internal(const rigid_body_params& params, u32 resource_slot, bool ghost)
    {

    }

    void add_compound_rb_internal(const compound_rb_cmd& cmd, u32 resource_slot)
    {

    }

    void add_compound_shape_internal(const compound_rb_params& params, u32 resource_slot)
    {

    }

    void add_hinge_internal(const constraint_params& params, u32 resource_slot)
    {

    }

    void add_constraint_internal(const constraint_params& params, u32 resource_slot)
    {

    }

    void add_central_force(const set_v3_params& cmd)
    {

    }

    void add_central_impulse(const set_v3_params& cmd)
    {

    }

    void add_force(const set_v3_v3_params& cmd)
    {

    }

    void set_linear_velocity_internal(const set_v3_params& cmd)
    {

    }

    void set_angular_velocity_internal(const set_v3_params& cmd)
    {

    }

    void set_linear_factor_internal(const set_v3_params& cmd)
    {

    }

    void set_angular_factor_internal(const set_v3_params& cmd)
    {

    }

    void set_transform_internal(const set_transform_params& cmd)
    {

    }

    void set_gravity_internal(const set_v3_params& cmd)
    {

    }

    void set_friction_internal(const set_float_params& cmd)
    {

    }

    void set_hinge_motor_internal(const set_v3_params& cmd)
    {

    }

    void set_button_motor_internal(const set_v3_params& cmd)
    {

    }

    void set_multi_joint_motor_internal(const set_multi_v3_params& cmd)
    {

    }

    void set_multi_joint_pos_internal(const set_multi_v3_params& cmd)
    {

    }

    void set_multi_joint_limit_internal(const set_multi_v3_params& cmd)
    {
    }

    void set_multi_base_velocity_internal(const set_multi_v3_params& cmd)
    {

    }

    void set_multi_base_pos_internal(const set_multi_v3_params& cmd)
    {

    }

    void sync_rigid_bodies_internal(const sync_rb_params& cmd)
    {

    }

    void sync_rigid_body_velocity_internal(const sync_rb_params& cmd)
    {

    }

    void sync_compound_multi_internal(const sync_compound_multi_params& cmd)
    {

    }

    void add_p2p_constraint_internal(const add_p2p_constraint_params& cmd, u32 resource_slot)
    {

    }

    void set_p2p_constraint_pos_internal(const set_v3_params& cmd)
    {

    }

    void set_damping_internal(const set_v3_params& cmd)
    {

    }

    void set_group_internal(const set_group_params& cmd)
    {

    }

    mat4 get_rb_start_matrix(u32 rb_index)
    {
        return mat4::create_identity();
    }

    void attach_rb_to_compound_internal(const attach_to_compound_params& params)
    {

    }

    void release_entity_internal(u32 entity_index)
    {

    }

    void remove_from_world_internal(u32 entity_index)
    {

    }

    void add_to_world_internal(u32 entity_index)
    {

    }

    cast_result cast_ray_internal(const ray_cast_params& rcp)
    {
        return {};
    }

    cast_result cast_sphere_internal(const sphere_cast_params& scp)
    {
        return {};
    }

    void contact_test_internal(const contact_test_params& ctp)
    {
    }
} // namespace physics