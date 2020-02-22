// physics.cpp
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "pen.h"
#include "pen_string.h"
#include "physics_bullet.h"
#include "slot_resource.h"
#include "timer.h"

// for multi body bullet
#include "BulletDynamics/Featherstone/btMultiBody.h"
#include "BulletDynamics/Featherstone/btMultiBodyConstraintSolver.h"
#include "BulletDynamics/Featherstone/btMultiBodyDynamicsWorld.h"
#include "BulletDynamics/Featherstone/btMultiBodyJointLimitConstraint.h"
#include "BulletDynamics/Featherstone/btMultiBodyJointMotor.h"
#include "BulletDynamics/Featherstone/btMultiBodyLink.h"
#include "BulletDynamics/Featherstone/btMultiBodyLinkCollider.h"
#include "BulletDynamics/Featherstone/btMultiBodyPoint2Point.h"
#include "btBulletDynamicsCommon.h"

namespace physics
{
    static pen::ring_buffer<physics_cmd> s_cmd_buffer;
    static pen::slot_resources           s_physics_slot_resources;
    static pen::slot_resources           s_p2p_slot_resources;

    void exec_cmd(const physics_cmd& cmd, f32 dt_ms)
    {
        switch (cmd.command_index)
        {
            case e_cmd::set_linear_velocity:
                set_linear_velocity_internal(cmd.set_v3);
                break;

            case e_cmd::set_angular_velocity:
                set_angular_velocity_internal(cmd.set_v3);
                break;

            case e_cmd::set_linear_factor:
                set_linear_factor_internal(cmd.set_v3);
                break;

            case e_cmd::set_angular_factor:
                set_angular_factor_internal(cmd.set_v3);
                break;

            case e_cmd::set_transform:
                set_transform_internal(cmd.set_transform);
                break;

            case e_cmd::add_rigid_body:
                add_rb_internal(cmd.add_rb, cmd.resource_slot);
                break;

            case e_cmd::add_ghost_rigid_body:
                add_rb_internal(cmd.add_rb, true);
                break;

            case e_cmd::set_gravity:
                set_gravity_internal(cmd.set_v3);
                break;

            case e_cmd::set_friction:
                set_friction_internal(cmd.set_float);
                break;

            case e_cmd::set_hinge_motor:
                set_hinge_motor_internal(cmd.set_v3);
                break;

            case e_cmd::set_button_motor:
                set_button_motor_internal(cmd.set_v3);
                break;

            case e_cmd::set_multi_joint_motor:
                set_multi_joint_motor_internal(cmd.set_multi_v3);
                break;

            case e_cmd::set_multi_joint_pos:
                set_multi_joint_pos_internal(cmd.set_multi_v3);
                break;

            case e_cmd::set_multi_joint_limits:
                set_multi_joint_limit_internal(cmd.set_multi_v3);
                break;

            case e_cmd::set_multi_base_velocity:
                set_multi_base_velocity_internal(cmd.set_multi_v3);
                break;

            case e_cmd::set_multi_base_pos:
                set_multi_base_pos_internal(cmd.set_multi_v3);
                break;

            case e_cmd::add_compound_rb:
                add_compound_rb_internal(cmd.add_compound_rb, cmd.resource_slot);
                break;

            case e_cmd::sync_compound_to_multi:
                sync_compound_multi_internal(cmd.sync_compound);
                break;

            case e_cmd::sync_rigid_body_transform:
                sync_rigid_bodies_internal(cmd.sync_rb);
                break;

            case e_cmd::sync_rigid_body_velocity:
                sync_rigid_body_velocity_internal(cmd.sync_rb);
                break;

            case e_cmd::set_p2p_constraint_pos:
                set_p2p_constraint_pos_internal(cmd.set_v3);
                break;

            case e_cmd::set_damping:
                set_damping_internal(cmd.set_v3);
                break;

            case e_cmd::set_group:
                set_group_internal(cmd.set_group);
                break;

            case e_cmd::add_compound_shape:
                add_compound_shape_internal(cmd.add_compound_rb.params, cmd.resource_slot);
                break;

            case e_cmd::attach_rb_to_compound:
                attach_rb_to_compound_internal(cmd.attach_compound);
                break;

            case e_cmd::remove_from_world:
                remove_from_world_internal(cmd.entity_index);
                break;

            case e_cmd::add_to_world:
                add_to_world_internal(cmd.entity_index);
                break;

            case e_cmd::release_entity:
                release_entity_internal(cmd.entity_index);
                break;

            case e_cmd::cast_ray:
                cast_ray_internal(cmd.ray_cast);
                break;

            case e_cmd::cast_sphere:
                cast_sphere_internal(cmd.sphere_cast);
                break;

            case e_cmd::add_constraint:
                add_constraint_internal(cmd.add_constraint_params, cmd.resource_slot);
                break;

            case e_cmd::add_central_force:
                add_central_force(cmd.set_v3);
                break;

            case e_cmd::add_central_impulse:
                add_central_impulse(cmd.set_v3);
                break;
            case e_cmd::contact_test:
                contact_test_internal(cmd.contact_test);
                break;
            case e_cmd::step:
                physics_update(dt_ms * 1000.0f);
                break;

            default:
                break;
        }
    }

    // thread sync
    pen::job* p_physics_job_thread_info;

    void physics_consume_command_buffer()
    {
        pen::semaphore_post(p_physics_job_thread_info->p_sem_consume, 1);
        pen::semaphore_wait(p_physics_job_thread_info->p_sem_continue);
    }

    void* physics_thread_main(void* params)
    {
        pen::job_thread_params* job_params = (pen::job_thread_params*)params;
        pen::job*               p_thread_info = job_params->job_info;
        pen::semaphore_post(p_thread_info->p_sem_continue, 1);

        p_physics_job_thread_info = p_thread_info;

        pen::slot_resources_init(&s_physics_slot_resources, 1024);
        pen::slot_resources_init(&s_p2p_slot_resources, 16);

        physics_initialise();

        static pen::timer* physics_timer = pen::timer_create();
        pen::timer_start(physics_timer);

        // space for 8192 commands
        s_cmd_buffer.create(8192);

        for (;;)
        {
            f32 dt_ms = pen::timer_elapsed_ms(physics_timer);

            pen::timer_start(physics_timer);

            if (pen::semaphore_try_wait(p_physics_job_thread_info->p_sem_consume))
            {
                pen::semaphore_post(p_physics_job_thread_info->p_sem_continue, 1);

                physics_cmd* cmd = s_cmd_buffer.get();
                while (cmd)
                {
                    exec_cmd(*cmd, dt_ms);
                    cmd = s_cmd_buffer.get();
                }
            }

            if (pen::semaphore_try_wait(p_physics_job_thread_info->p_sem_exit))
                break;
        }

        pen::semaphore_post(p_physics_job_thread_info->p_sem_continue, 1);
        pen::semaphore_post(p_physics_job_thread_info->p_sem_terminated, 1);

        return PEN_THREAD_OK;
    }

    void set_v3(const u32& entity_index, const vec3f& v3, u32 cmd)
    {
        physics_cmd pc;
        pc.command_index = cmd;
        memcpy(&pc.set_v3.data, &v3, sizeof(vec3f));
        pc.set_v3.object_index = entity_index;

        s_cmd_buffer.put(pc);
    }

    void set_float(const u32& entity_index, const f32& fval, u32 cmd)
    {
        physics_cmd pc;
        pc.command_index = cmd;
        memcpy(&pc.set_float.data, &fval, sizeof(f32));
        pc.set_float.object_index = entity_index;

        s_cmd_buffer.put(pc);
    }

    void set_transform(const u32& entity_index, const vec3f& position, const quat& quaternion)
    {
        physics_cmd pc;
        pc.command_index = e_cmd::set_transform;
        memcpy(&pc.set_transform.position, &position, sizeof(vec3f));
        memcpy(&pc.set_transform.rotation, &quaternion, sizeof(quat));
        pc.set_transform.object_index = entity_index;

        s_cmd_buffer.put(pc);
    }

    mat4 get_rb_matrix(const u32& entity_index)
    {
        mat4* const& fb = g_readable_data.output_matrices.frontbuffer();
        return fb[entity_index];
    }

    maths::transform get_rb_transform(const u32& entity_index)
    {
        maths::transform* const& fb = g_readable_data.output_transforms.frontbuffer();
        return fb[entity_index];
    }

    bool has_rb_matrix(const u32& entity_index)
    {
        auto&        om = g_readable_data.output_matrices;
        mat4* const& fb = om._data[om._fb];
        if (entity_index >= sb_count(fb))
            return false;

        return true;
    }

    u32 add_rb(const rigid_body_params& rbp)
    {
        physics_cmd pc;

        pc.command_index = e_cmd::add_rigid_body;
        pc.add_rb = rbp;

        u32 resource_slot = pen::slot_resources_get_next(&s_physics_slot_resources);
        pc.resource_slot = resource_slot;

        s_cmd_buffer.put(pc);

        return resource_slot;
    }

    u32 add_ghost_rb(const rigid_body_params& rbp)
    {
        physics_cmd pc;

        pc.command_index = e_cmd::add_ghost_rigid_body;
        pc.add_rb = rbp;

        u32 resource_slot = pen::slot_resources_get_next(&s_physics_slot_resources);
        pc.resource_slot = resource_slot;

        s_cmd_buffer.put(pc);

        return resource_slot;
    }

    u32 add_multibody(const multi_body_params& mbp)
    {
        physics_cmd pc;

        pc.command_index = e_cmd::add_multi_body;
        pc.add_multi = mbp;

        u32 resource_slot = pen::slot_resources_get_next(&s_physics_slot_resources);
        pc.resource_slot = resource_slot;

        s_cmd_buffer.put(pc);

        return resource_slot;
    }

    void set_paused(bool val)
    {
        g_readable_data.b_paused = val;
    }

    void set_multi_v3(const u32& object_index, const u32& link_index, const vec3f& v3_data, const u32& cmd)
    {
        physics_cmd pc;

        pc.command_index = cmd;

        memcpy(&pc.set_multi_v3.data, &v3_data, sizeof(vec3f));
        pc.set_multi_v3.multi_index = object_index;
        pc.set_multi_v3.link_index = link_index;

        s_cmd_buffer.put(pc);
    }

    u32 add_compound_rb(const compound_rb_params& crbp, u32** child_handles_out)
    {
        physics_cmd pc;

        pc.command_index = e_cmd::add_compound_rb;
        pc.add_compound_rb.params = crbp;

        u32 resource_slot = pen::slot_resources_get_next(&s_physics_slot_resources);
        pc.resource_slot = resource_slot;

        pc.add_compound_rb.children_handles = nullptr;
        *child_handles_out = nullptr;
        for (u32 i = 0; i < crbp.num_shapes; ++i)
        {
            u32 cs = pen::slot_resources_get_next(&s_physics_slot_resources);
            sb_push(pc.add_compound_rb.children_handles, cs);
            sb_push(*child_handles_out, cs);
        }

        s_cmd_buffer.put(pc);

        return resource_slot;
    }

    void sync_compound_multi(const u32& compound_index, const u32& multi_index)
    {
        physics_cmd pc;

        pc.command_index = e_cmd::sync_compound_to_multi;

        pc.sync_compound.compound_index = compound_index;
        pc.sync_compound.multi_index = multi_index;

        s_cmd_buffer.put(pc);
    }

    void sync_rigid_bodies(const u32& master, const u32& slave, const s32& link_index, u32 cmd)
    {
        physics_cmd pc;

        pc.command_index = cmd;

        pc.sync_rb.master = master;
        pc.sync_rb.slave = slave;
        pc.sync_rb.link_index = link_index;

        s_cmd_buffer.put(pc);
    }

    u32 add_constraint(const constraint_params& crbp)
    {
        physics_cmd pc;

        pc.command_index = e_cmd::add_constraint;
        pc.add_constraint_params = crbp;

        u32 resource_slot = pen::slot_resources_get_next(&s_physics_slot_resources);
        pc.resource_slot = resource_slot;

        s_cmd_buffer.put(pc);

        return resource_slot;
    }

    void set_collision_group(const u32& object_index, const u32& group, const u32& mask)
    {
        physics_cmd pc;

        pc.command_index = e_cmd::set_group;
        pc.set_group.group = group;
        pc.set_group.mask = mask;

        s_cmd_buffer.put(pc);
    }

    u32 add_compound_shape(const compound_rb_params& crbp)
    {
        physics_cmd pc;

        pc.command_index = e_cmd::add_compound_shape;
        pc.add_compound_rb.params = crbp;

        u32 resource_slot = pen::slot_resources_get_next(&s_physics_slot_resources);
        pc.resource_slot = resource_slot;

        s_cmd_buffer.put(pc);

        return resource_slot;
    }

    u32 attach_rb_to_compound(const attach_to_compound_params& params)
    {
        physics_cmd pc;

        pc.command_index = e_cmd::attach_rb_to_compound;
        pc.attach_compound = params;

        s_cmd_buffer.put(pc);

        return 0;
    }

    void remove_from_world(const u32& entity_index)
    {
        physics_cmd pc;

        pc.command_index = e_cmd::remove_from_world;
        pc.entity_index = entity_index;

        s_cmd_buffer.put(pc);
    }

    void add_to_world(const u32& entity_index)
    {
        physics_cmd pc;

        pc.command_index = e_cmd::add_to_world;
        pc.entity_index = entity_index;

        s_cmd_buffer.put(pc);
    }

    void release_entity(const u32& entity_index)
    {
        if (!pen::slot_resources_free(&s_physics_slot_resources, entity_index))
            return;

        physics_cmd pc;

        pc.command_index = e_cmd::release_entity;
        pc.entity_index = entity_index;

        s_cmd_buffer.put(pc);
    }

    void cast_ray(const ray_cast_params& rcp)
    {
        if (mag(rcp.start - rcp.end) < 0.0001f)
            return;

        physics_cmd pc;
        pc.command_index = e_cmd::cast_ray;
        pc.ray_cast = rcp;
        s_cmd_buffer.put(pc);
    }

    void cast_sphere(const sphere_cast_params& scp)
    {
        if (mag(scp.from - scp.to) < 0.0001f)
            return;

        physics_cmd pc;
        pc.command_index = e_cmd::cast_sphere;
        pc.sphere_cast = scp;
        s_cmd_buffer.put(pc);
    }

    cast_result cast_ray_immediate(const ray_cast_params& rcp)
    {
        return cast_ray_internal(rcp);
    }

    cast_result cast_sphere_immediate(const sphere_cast_params& scp)
    {
        return cast_sphere_internal(scp);
    }

    void contact_test(const contact_test_params& ctp)
    {
        physics_cmd pc;
        pc.command_index = e_cmd::contact_test;
        pc.contact_test = ctp;
        s_cmd_buffer.put(pc);
    }

    void step()
    {
        physics_cmd pc;
        pc.command_index = e_cmd::step;
        s_cmd_buffer.put(pc);
    }
} // namespace physics
