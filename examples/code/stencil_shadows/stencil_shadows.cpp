#include "../example_common.h"
#include "../../shader_structs/forward_render.h"

using namespace put;
using namespace put::ecs;

pen::window_creation_params pen_window{
    1280,               // width
    720,                // height
    4,                  // MSAA samples
    "stencil_shadows"   // window title / process name
};

namespace {
    struct shadow_volume_vertex
    {
        vec4f pos;
        vec4f face_normal_0;
        vec4f face_normal_1;
    };
    
    struct shadow_volume_edge
    {
        vec4f pos_0;
        vec4f pos_1;
        vec4f face_normal_0;
        vec4f face_normal_1;
    };
    
    bool almost_equalf(vec4f v1, vec4f v2, f32 epsilon_sq)
    {
        if(dist2(v1, v2) < epsilon_sq)
            return true;
        
        return false;
    }
    
    u32 cube_entity = 0;
    u32 light_0 = 0;
    shadow_volume_edge* s_sve;
}

void generate_edge_mesh(geometry_resource* gr, shadow_volume_edge** sve_out)
{
    vertex_model* vm = (vertex_model*)gr->cpu_vertex_buffer;
    u16* ib = (u16*)gr->cpu_index_buffer;
    
    static u32 k[] = {
        1, 2, 0
    };
    
    shadow_volume_edge* sve = nullptr;
    
    static const f32 epsilon = 0.1f;
    for(u32 i = 0; i < gr->num_indices; i+=3)
    {
        shadow_volume_edge e[3];
        for(u32 j = 0; j < 3; ++j)
        {
            e[j].pos_0 = vm[ib[i+j]].pos;
            e[j].pos_1 = vm[ib[i+k[j]]].pos;
        }
        
        vec3f fn = maths::get_normal(e[0].pos_0.xyz, e[1].pos_0.xyz, e[2].pos_0.xyz);
        vec4f fn4 = vec4f(-fn, 1.0f);
        
        for(u32 j = 0; j < 3; ++j)
        {
            e[j].pos_0 = vm[ib[i+j]].pos;
            e[j].pos_1 = vm[ib[i+k[j]]].pos;
            
            s32 found = -1;
            u32 ne = sb_count(sve);
            for(u32 x = 0; x < ne; ++x)
            {
                if(almost_equal(sve[x].pos_0, e[j].pos_0, epsilon) && almost_equal(sve[x].pos_1, e[j].pos_1, epsilon)) {
                    found = x;
                    break;
                }
                
                if(almost_equal(sve[x].pos_1, e[j].pos_0, epsilon) && almost_equal(sve[x].pos_0, e[j].pos_1, epsilon)) {
                    found = x;
                    break;
                }
            }
            
            if(found == -1)
            {
                e[j].face_normal_0 = fn4;
                sb_push(sve, e[j]);
            }
            else
            {
                sve[found].face_normal_1 = fn4;
            }
        }
    }
    
    *sve_out = sve;
}

void example_setup(ecs::ecs_scene* scene, camera& cam)
{
    pmfx::init("data/configs/stencil_shadows.jsn");
    
    clear_scene(scene);
    
    material_resource* default_material = get_material_resource(PEN_HASH("default_material"));
    
    geometry_resource* box = get_geometry_resource(PEN_HASH("cube"));
    
    // add light
    u32 light = get_new_entity(scene);
    scene->names[light] = "front_light";
    scene->id_name[light] = PEN_HASH("front_light");
    scene->lights[light].colour = vec3f::one();
    scene->lights[light].direction = vec3f::one();
    scene->lights[light].type = LIGHT_TYPE_DIR;
    scene->transforms[light].translation = vec3f::zero();
    scene->transforms[light].rotation = quat();
    scene->transforms[light].scale = vec3f::one();
    scene->entities[light] |= CMP_LIGHT;
    scene->entities[light] |= CMP_TRANSFORM;
    light_0 = light;
    
    // ground
    u32 ground = get_new_entity(scene);
    scene->names[ground] = "ground";
    scene->transforms[ground].translation = vec3f::zero();
    scene->transforms[ground].rotation = quat();
    scene->transforms[ground].scale = vec3f(50.0f, 1.0f, 50.0f);
    scene->entities[ground] |= CMP_TRANSFORM;
    scene->parents[ground] = ground;
    instantiate_geometry(box, scene, ground);
    instantiate_material(default_material, scene, ground);
    instantiate_model_cbuffer(scene, ground);
    
    // cube
    cube_entity = get_new_entity(scene);
    scene->names[cube_entity] = "cube";
    scene->transforms[cube_entity].translation = vec3f(0.0f, 11.0f, 0.0f);
    scene->transforms[cube_entity].rotation = quat();
    scene->transforms[cube_entity].scale = vec3f(10.0f, 10.0f, 10.0f);
    scene->entities[cube_entity] |= CMP_TRANSFORM;
    scene->parents[cube_entity] = cube_entity;
    instantiate_geometry(box, scene, cube_entity);
    instantiate_material(default_material, scene, cube_entity);
    instantiate_model_cbuffer(scene, cube_entity);
    
    generate_edge_mesh(box, &s_sve);
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
    geometry_resource* box = get_geometry_resource(PEN_HASH("cube"));
    
    vertex_model* vm = (vertex_model*)box->cpu_vertex_buffer;
    mat4 wm = scene->world_matrices[cube_entity];
    
    for(u32 i = 0; i < box->num_vertices; ++i)
    {
        vec4f tp = wm.transform_vector(vm[i].pos);
        dbg::add_point(tp.xyz, 1.0f);
    }
    
    static s32 edge = 0;
    static bool isolate = false;
    ImGui::Checkbox("Isolate", &isolate);
    ImGui::InputInt("Edge", &edge);
    
    u32 ne = sb_count(s_sve);
    for(u32 j = 0; j < ne; ++j)
    {
        if(isolate)
            if(j != edge)
                continue;
        
        vec4f p0 = wm.transform_vector(s_sve[j].pos_0);
        vec4f p1 = wm.transform_vector(s_sve[j].pos_1);
        
        vec3f c = p0.xyz + (p1.xyz - p0.xyz) * 0.5f;
        
        dbg::add_line(c, c + s_sve[j].face_normal_0.xyz, vec4f::red());
        dbg::add_line(c, c + s_sve[j].face_normal_1.xyz, vec4f::magenta());
        
        vec3f ld = -vec3f::one();
        f32 d0 = dot(ld, s_sve[j].face_normal_0.xyz);
        f32 d1 = dot(ld, s_sve[j].face_normal_1.xyz);
        
        if((d0 > 0.0f && d1 < 0.0f) || (d1 > 0.0f && d0 < 0.0f))
        {
            dbg::add_line(p0.xyz, p0.xyz + ld * 100.0f);
            dbg::add_line(p1.xyz, p1.xyz + ld * 100.0f);
            dbg::add_line(p0.xyz, p1.xyz, vec4f::white());
        }
        else
        {
            dbg::add_line(p0.xyz, p1.xyz, vec4f::green());
        }
    }
}
