#include "volume_generator.h"

#include "camera.h"
#include "timer.h"
#include "ces/ces_editor.h"
#include "ces/ces_resources.h"
#include "ces/ces_scene.h"
#include "ces/ces_utilities.h"
#include "debug_render.h"
#include "dev_ui.h"
#include "pmfx.h"

#include "console.h"
#include "data_struct.h"
#include "hash.h"
#include "memory.h"
#include "pen.h"

#include "sdf/makelevelset3.h"

#define PEN_SIMD 0

#if PEN_SIMD
#include <xmmintrin.h>
#endif

// Progress / Cancellation
extern mls_progress g_mls_progress;
std::atomic<bool>   g_cancel_volume_job;
std::atomic<bool>   g_cancel_handled;

namespace put
{
    using namespace ces;

    namespace vgt
    {
        struct triangle
        {
            vec3f v[3];
            vec3f n;
        };

        struct triangle_octree
        {
            triangle*        triangles        = nullptr;
            triangle*        failed_triangles = nullptr;
            triangle_octree* children         = nullptr;
            u32              id               = 0;
            extents          e;
        };

        static const int k_num_axes = 6;

        enum volume_types
        {
            VOLUME_RASTERISED_TEXELS = 0,
            VOLUME_SIGNED_DISTANCE_FIELD
        };

        enum volume_raster_axis
        {
            ZAXIS_POS,
            YAXIS_POS,
            XAXIS_POS,
            ZAXIS_NEG,
            YAXIS_NEG,
            XAXIS_NEG
        };

        enum volume_raster_axis_flags
        {
            Z_POS_MASK = 1 << 0,
            Y_POS_MASK = 1 << 1,
            X_POS_MASK = 1 << 2,
            Z_NEG_MASK = 1 << 3,
            Y_NEG_MASK = 1 << 4,
            X_NEG_MASK = 1 << 5,

            AXIS_ALL_MASK = (1 << 6) - 1
        };

        enum rasterise_capture_data
        {
            CAPTURE_ALBEDO = 0,
            CAPTURE_NORMALS,
            CAPTURE_BAKED_LIGHTING,
            CAPTURE_OCCUPANCY,
            CAPTURE_CUSTOM
        };

        enum capture
        {
            CAPTURE_ALL,
            CAPTURE_SELECTED
        };

        struct vgt_options
        {
            s32  volume_dimension = 7;
            u32  rasterise_axes   = AXIS_ALL_MASK;
            s32  volume_type      = VOLUME_RASTERISED_TEXELS;
            s32  capture_data     = 0;
            bool generate_mips    = true;
        };

        struct vgt_rasteriser_job
        {
            vgt_options options;
            void**      volume_slices[k_num_axes] = {0};
            s32         current_slice             = 0;
            s32         current_requested_slice   = -1;
            s32         current_axis              = 0;
            u32         dimension;
            extents     visible_extents;
            extents     current_slice_aabb;
            bool        rasterise_in_progress = false;
            a_u32       combine_in_progress;
            a_u32       combine_position;
            u32         block_size;
            u32         data_size;
            u8*         volume_data;
            s32         capture_type = 0;
        };
        static vgt_rasteriser_job k_rasteriser_job;

        struct dd
        {
            vec3f pos;
            vec4f closest;
        };

        struct vgt_sdf_job
        {
            vgt_options     options;
            entity_scene*   scene;
            triangle_octree tree;
            u32             volume_dim;
            u32             texture_format;
            u32             block_size;
            u32             data_size;
            u8*             volume_data;
            extents         scene_extents;
            bool            trust_sign = true;
            f32             padding;
            u32             generate_in_progress = 0;
            s32             capture_type         = 0;
        };
        static vgt_sdf_job k_sdf_job;

        static put::camera k_volume_raster_ortho;
        static pen::texture_creation_params* k_generated_volume_tcp;

        u8* get_texel(u32 axis, u32 x, u32 y, u32 z)
        {
            u32&    volume_dim    = k_rasteriser_job.dimension;
            void*** volume_slices = k_rasteriser_job.volume_slices;

            u32 block_size = 4;
            u32 row_pitch  = volume_dim * 4;

            u32 invx = volume_dim - x - 1;
            u32 invy = volume_dim - y - 1;
            u32 invz = volume_dim - z - 1;

            u8* slice = nullptr;

            u32 mask = k_rasteriser_job.options.rasterise_axes;

            if (!(mask & 1 << axis))
                return nullptr;

            swap(y, invy);

            switch (axis)
            {
                case ZAXIS_POS:
                {
                    u32 offset_zpos = y * row_pitch + invx * block_size;
                    slice           = (u8*)volume_slices[0][z];
                    return &slice[offset_zpos];
                }
                case ZAXIS_NEG:
                {
                    u32 offset_zneg = y * row_pitch + x * block_size;
                    slice           = (u8*)volume_slices[3][invz];
                    return &slice[offset_zneg];
                }
                case YAXIS_POS:
                {
                    u32 offset_ypos = invz * row_pitch + x * block_size;
                    slice           = (u8*)volume_slices[1][invy];
                    return &slice[offset_ypos];
                }
                case YAXIS_NEG:
                {
                    u32 offset_yneg = z * row_pitch + x * block_size;
                    slice           = (u8*)volume_slices[4][y];
                    return &slice[offset_yneg];
                }
                case XAXIS_POS:
                {
                    u32 offset_xpos = y * row_pitch + z * block_size;
                    slice           = (u8*)volume_slices[2][x];
                    return &slice[offset_xpos];
                }
                case XAXIS_NEG:
                {
                    u32 offset_xneg = y * row_pitch + invz * block_size;
                    slice           = (u8*)volume_slices[5][invx];
                    return &slice[offset_xneg];
                }
                default:
                    return nullptr;
            }

            return nullptr;
        }

        u32 get_texel_offset(u32 slice_pitch, u32 row_pitch, u32 block_size, u32 x, u32 y, u32 z)
        {
            return z * slice_pitch + y * row_pitch + x * block_size;
        }

        void image_read_back(void* p_data, u32 row_pitch, u32 depth_pitch, u32 block_size)
        {
            if (g_cancel_volume_job)
            {
                g_cancel_handled = true;
                return;
            }

            s32&    current_slice = k_rasteriser_job.current_slice;
            s32&    current_axis  = k_rasteriser_job.current_axis;
            void*** volume_slices = k_rasteriser_job.volume_slices;

            u32 w = row_pitch / block_size;
            u32 h = depth_pitch / row_pitch;

            if (w != k_rasteriser_job.dimension)
            {
                u32 dest_row_pitch = k_rasteriser_job.dimension * block_size;
                u8* src_iter       = (u8*)p_data;
                u8* dest_iter      = (u8*)volume_slices[current_axis][current_slice];
                for (u32 y = 0; y < h; ++y)
                {
                    pen::memory_cpy(dest_iter, src_iter, dest_row_pitch);
                    src_iter += row_pitch;
                    dest_iter += dest_row_pitch;
                }
            }
            else
            {
                pen::memory_cpy(volume_slices[current_axis][current_slice], p_data, depth_pitch);
            }

            current_slice++;
        }

        PEN_TRV raster_voxel_combine(void* params)
        {
            pen::job_thread_params* job_params     = (pen::job_thread_params*)params;
            vgt_rasteriser_job*     rasteriser_job = (vgt_rasteriser_job*)job_params->user_data;
            pen::job*               p_thread_info  = job_params->job_info;
            pen::thread_semaphore_signal(p_thread_info->p_sem_continue, 1);

            u32&    volume_dim    = rasteriser_job->dimension;
            void*** volume_slices = rasteriser_job->volume_slices;

            // create a simple 3d texture
            rasteriser_job->block_size = 4;
            rasteriser_job->data_size  = volume_dim * volume_dim * volume_dim * rasteriser_job->block_size;

            u8* volume_data = (u8*)pen::memory_alloc(rasteriser_job->data_size);
            u32 row_pitch   = volume_dim * rasteriser_job->block_size;
            u32 slice_pitch = volume_dim * row_pitch;

            rasteriser_job->combine_position = 0;

            for (u32 z = 0; z < volume_dim; ++z)
            {
                if (g_cancel_volume_job)
                    break;

                u8* slice_mem[6] = {0};
                for (u32 a = 0; a < 6; ++a)
                {
                    slice_mem[a] = (u8*)volume_slices[a][z];
                }

                for (u32 y = 0; y < volume_dim; ++y)
                {
                    for (u32 x = 0; x < volume_dim; ++x)
                    {
                        rasteriser_job->combine_position++;

                        u32 offset = z * slice_pitch + y * row_pitch + x * rasteriser_job->block_size;

                        u8 rgba[4] = {0};

                        for (u32 a = 0; a < 6; ++a)
                        {
                            u8* tex = get_texel(a, x, y, z);

                            if (!tex)
                                continue;

                            if (tex[3] > 8)
                                for (u32 p = 0; p < 4; ++p)
                                    rgba[p] = tex[p];
                        }

                        volume_data[offset + 0] = rgba[2];
                        volume_data[offset + 1] = rgba[1];
                        volume_data[offset + 2] = rgba[0];
                        volume_data[offset + 3] = rgba[3];
                    }
                }
            }

            if (g_cancel_volume_job)
            {
                pen::memory_free(volume_data);
                g_cancel_handled = true;
                return PEN_THREAD_OK;
            }

            // with the 3d texture now initialised, dilate colour edges so we can used bilinear
            u32 bs = rasteriser_job->block_size;
            u32 rp = row_pitch;
            u32 sp = slice_pitch;

            static vec3i nb[] = {
                {-1, -1, 0}, {-1, -1, 1}, {-1, -1, -1}, {0, -1, 0}, {0, -1, 1}, {0, -1, -1}, {1, -1, 0},
                {1, -1, 1},  {1, -1, -1}, {1, 0, 0},    {1, 0, 1},  {1, 0, -1}, {1, 1, 0},   {1, 1, 1},
                {1, 1, -1},  {0, 1, 0},   {0, 1, 1},    {0, 1, -1}, {-1, 1, 0}, {-1, 1, 1},  {-1, 1, -1},
                {-1, 0, 0},  {-1, 0, 1},  {-1, 0, -1},  {0, 0, 1},  {0, 0, -1},
            };

            vec3i clamp_min = vec3i::zero();
            vec3i clamp_max = vec3i(volume_dim - 1);

            for (u32 z = 0; z < volume_dim; ++z)
            {
                for (u32 y = 0; y < volume_dim; ++y)
                {
                    for (u32 x = 0; x < volume_dim; ++x)
                    {
                        // check neighbours and dilate rgb from edges
                        u32 offset = get_texel_offset(sp, rp, bs, x, y, z);

                        if (volume_data[offset + 3] == 0)
                        {
                            for (u32 n = 0; n < PEN_ARRAY_SIZE(nb); ++n)
                            {
                                vec3i nn = vec3i(x + nb[n].x, y + nb[n].y, z + nb[n].z);
                                nn       = vclamp(nn, clamp_min, clamp_max);

                                u32 noffset = get_texel_offset(sp, rp, bs, nn.x, nn.y, nn.z);

                                if (volume_data[noffset + 3] > 0)
                                {
                                    // copy rgb to dilate
                                    pen::memory_cpy(&volume_data[offset + 0], &volume_data[noffset + 0], 3);
                                }
                            }
                        }
                    }
                }
            }

            rasteriser_job->volume_data = volume_data;

            rasteriser_job->combine_in_progress = 2;

            return PEN_THREAD_OK;
        }

        void generate_mips_r32f_simd(pen::texture_creation_params& tcp)
        {
#if PEN_SIMD
            // calc num mips
            u32              num_mips = 1;
            u32              data_size = 0;
            static const u32 block_size = 4;

            vec3ui m = vec3ui(tcp.width, tcp.height, tcp.num_arrays);

            while (m.x > 1 && m.y > 1 && m.z > 1)
            {
                data_size += m.x * m.y * m.z * block_size;

                num_mips++;

                m /= vec3ui(2, 2, 2);
                m = max_union(m, vec3ui::one());
            }

            // offsets
            vec3ui offsets[] = { vec3ui(0, 0, 0), vec3ui(0, 1, 0), vec3ui(0, 0, 1), vec3ui(0, 1, 1) };

            //u8* data = (u8*)pen::memory_alloc_align(data_size, 16);
            u8* data = (u8*)pen::memory_alloc(data_size);
            pen::memory_cpy(data, tcp.data, tcp.data_size);

            m = vec3ui(tcp.width, tcp.height, tcp.num_arrays);

            u8* prev_level = (u8*)data;
            u8* cur_level = prev_level + tcp.data_size;

            __m128 vrecip = _mm_set1_ps(1.0f / 8.0f);
            __m128 r00, r01, r10, r11;
            __m128 d00, d01, d10, d11;
            
            u32 p_offset[4];
            
            for (u32 i = 0; i < num_mips - 1; ++i)
            {
                u32 p_rp = m.x * block_size; // prev row pitch
                u32 p_sp = p_rp * m.y;       // prev slice pitch

                m /= vec3ui(2, 2, 2);
                m = max_union(m, vec3ui::one());

                u32 c_rp = m.x * block_size; // cur row pitch
                u32 c_sp = c_rp * m.y;       // cur slice pitch

                for (u32 z = 0; z < m.z; ++z)
                {
                    for (u32 y = 0; y < m.y; ++y)
                    {
                        for (u32 x = 0; x < m.x; x += 4)
                        {
                            vec3ui vobase = vec3ui(x * 2, y * 2, z * 2);
                            vec3ui vo = vobase;
                            p_offset[0] = p_sp * vo.z + p_rp * vo.y + block_size * vo.x;
                            
                            vo = vobase + offsets[1];
                            p_offset[1] = p_sp * vo.z + p_rp * vo.y + block_size * vo.x;
                            
                            vo = vobase + offsets[2];
                            p_offset[2] = p_sp * vo.z + p_rp * vo.y + block_size * vo.x;
                            
                            vo = vobase + offsets[3];
                            p_offset[3] = p_sp * vo.z + p_rp * vo.y + block_size * vo.x;
                            
                            pen::memory_cpy(&r00, &prev_level[p_offset[0]], 16);
                            pen::memory_cpy(&r01, &prev_level[p_offset[0] + 16], 16);
                            pen::memory_cpy(&r10, &prev_level[p_offset[1]], 16);
                            pen::memory_cpy(&r11, &prev_level[p_offset[1] + 16], 16);
                            
                            pen::memory_cpy(&d00, &prev_level[p_offset[2]], 16);
                            pen::memory_cpy(&d01, &prev_level[p_offset[2] + 16], 16);
                            pen::memory_cpy(&d10, &prev_level[p_offset[3]], 16);
                            pen::memory_cpy(&d11, &prev_level[p_offset[3] + 16], 16);
                            
                            __m128 x0 = _mm_shuffle_ps(r00, r01, _MM_SHUFFLE(2, 0, 2, 0));
                            __m128 x1 = _mm_shuffle_ps(r00, r01, _MM_SHUFFLE(3, 1, 3, 1));
                            __m128 x2 = _mm_shuffle_ps(r10, r11, _MM_SHUFFLE(2, 0, 2, 0));
                            __m128 x3 = _mm_shuffle_ps(r10, r11, _MM_SHUFFLE(3, 1, 3, 1));
                            __m128 x4 = _mm_shuffle_ps(d00, d01, _MM_SHUFFLE(2, 0, 2, 0));
                            __m128 x5 = _mm_shuffle_ps(d00, d01, _MM_SHUFFLE(3, 1, 3, 1));
                            __m128 x6 = _mm_shuffle_ps(d10, d11, _MM_SHUFFLE(2, 0, 2, 0));
                            __m128 x7 = _mm_shuffle_ps(d10, d11, _MM_SHUFFLE(3, 1, 3, 1));
                            
                            __m128 output = _mm_add_ps(x0, x1);
                            output = _mm_add_ps(output, x2);
                            output = _mm_add_ps(output, x3);
                            output = _mm_add_ps(output, x4);
                            output = _mm_add_ps(output, x5);
                            output = _mm_add_ps(output, x6);
                            output = _mm_add_ps(output, x7);
                            output = _mm_mul_ps(output, vrecip);
                            
                            u32 c_offset = c_sp * z + c_rp * y + block_size * x;
                            pen::memory_cpy(&cur_level[c_offset], &output, sizeof(__m128));
                        }
                    }
                }

                prev_level = cur_level;
                cur_level += c_sp * m.z;
            }

            tcp.num_mips = num_mips;
            tcp.data_size = data_size;

            tcp.data = data;
#endif
        }

        void generate_mips_r32f(pen::texture_creation_params& tcp)
        {
            // calc num mips
            u32              num_mips = 1;
            u32              data_size = 0;
            static const u32 block_size = 4;

            vec3ui m = vec3ui(tcp.width, tcp.height, tcp.num_arrays);

            while (m.x > 1 && m.y > 1 && m.z > 1)
            {
                data_size += m.x * m.y * m.z * block_size;

                num_mips++;

                m /= vec3ui(2, 2, 2);
                m = max_union(m, vec3ui::one());
            }

            // offsets
            vec3ui offsets[] = {    vec3ui(0, 0, 0),

                                    vec3ui(1, 0, 0), vec3ui(0, 1, 0), vec3ui(1, 1, 0),

                                    vec3ui(0, 0, 1), vec3ui(1, 0, 1), vec3ui(0, 1, 1), vec3ui(1, 1, 1) };

            u8* data = (u8*)pen::memory_alloc(data_size);
            pen::memory_cpy(data, tcp.data, tcp.data_size);

            m = vec3ui(tcp.width, tcp.height, tcp.num_arrays);

            u8* prev_level = (u8*)data;
            u8* cur_level = prev_level + tcp.data_size;

            for (u32 i = 0; i < num_mips - 1; ++i)
            {
                u32 p_rp = m.x * block_size; // prev row pitch
                u32 p_sp = p_rp * m.y;       // prev slice pitch

                m /= vec3ui(2, 2, 2);
                m = max_union(m, vec3ui::one());

                u32 c_rp = m.x * block_size; // cur row pitch
                u32 c_sp = c_rp * m.y;       // cur slice pitch

                for (u32 z = 0; z < m.z; ++z)
                {
                    for (u32 y = 0; y < m.y; ++y)
                    {
                        for (u32 x = 0; x < m.x; ++x)
                        {
                            u32 c_offset = c_sp * z + c_rp * y + block_size * x;

                            f32 texel = 0.0f;

                            for (u32 o = 0; o < PEN_ARRAY_SIZE(offsets); ++o)
                            {
                                vec3ui vo = vec3ui(x * 2, y * 2, z * 2) + offsets[o];

                                u32 p_offset = p_sp * vo.z + p_rp * vo.y + block_size * vo.x;

                                f32* ft = (f32*)&prev_level[p_offset];
                                texel += *ft;
                            }

                            texel /= PEN_ARRAY_SIZE(offsets);
                            pen::memory_cpy(&cur_level[c_offset], &texel, sizeof(f32));
                        }
                    }
                }

                prev_level = cur_level;
                cur_level += c_sp * m.z;
            }

            tcp.num_mips = num_mips;
            tcp.data_size = data_size;

            tcp.data = data;
        }

        void generate_mips_rgba8(pen::texture_creation_params& tcp)
        {
            // calc num mips
            u32              num_mips   = 1;
            u32              data_size  = 0;
            static const u32 block_size = 4;

            vec3ui m = vec3ui(tcp.width, tcp.height, tcp.num_arrays);

            while (m.x > 1 && m.y > 1 && m.z > 1)
            {
                data_size += m.x * m.y * m.z * block_size;

                num_mips++;

                m /= vec3ui(2, 2, 2);
                m = max_union(m, vec3ui::one());
            }

            // offsets
            vec3ui offsets[] = {vec3ui(0, 0, 0),

                                vec3ui(1, 0, 0), vec3ui(0, 1, 0), vec3ui(1, 1, 0),

                                vec3ui(0, 0, 1), vec3ui(1, 0, 1), vec3ui(0, 1, 1), vec3ui(1, 1, 1)};

            u8* data = (u8*)pen::memory_alloc(data_size);
            pen::memory_cpy(data, tcp.data, tcp.data_size);

            m = vec3ui(tcp.width, tcp.height, tcp.num_arrays);

            u8* prev_level = (u8*)data;
            u8* cur_level  = prev_level + tcp.data_size;

            for (u32 i = 0; i < num_mips - 1; ++i)
            {
                u32 p_rp = m.x * block_size; // prev row pitch
                u32 p_sp = p_rp * m.y;       // prev slice pitch

                m /= vec3ui(2, 2, 2);
                m = max_union(m, vec3ui::one());

                u32 c_rp = m.x * block_size; // cur row pitch
                u32 c_sp = c_rp * m.y;       // cur slice pitch

                for (u32 z = 0; z < m.z; ++z)
                {
                    for (u32 y = 0; y < m.y; ++y)
                    {
                        for (u32 x = 0; x < m.x; ++x)
                        {
                            u32 c_offset = c_sp * z + c_rp * y + block_size * x;

                            u8 rgba[4] = {0};

                            for (u32 o = 0; o < PEN_ARRAY_SIZE(offsets); ++o)
                            {
                                vec3ui vo = vec3ui(x * 2, y * 2, z * 2) + offsets[o];

                                u32 p_offset = p_sp * vo.z + p_rp * vo.y + block_size * vo.x;

                                for (u32 r = 0; r < 4; ++r)
                                    rgba[r] = max<u8>(prev_level[p_offset + r], rgba[r]);
                            }

                            for (u32 r = 0; r < 4; ++r)
                            {
                                // rgba[r] /= PEN_ARRAY_SIZE(offsets);
                                cur_level[c_offset + r] = rgba[r];
                            }
                        }
                    }
                }

                prev_level = cur_level;
                cur_level += c_sp * m.z;
            }

            tcp.num_mips  = num_mips;
            tcp.data_size = data_size;

            tcp.data = data;
        }

        u32 create_volume_from_data(u32 volume_dim, u32 block_size, u32 data_size, u32 tex_format, u8* volume_data, bool generate_mips)
        {
            pen::texture_creation_params tcp;
            tcp.collection_type = pen::TEXTURE_COLLECTION_VOLUME;

            tcp.width            = volume_dim;
            tcp.height           = volume_dim;
            tcp.format           = tex_format;
            tcp.num_mips         = 1;
            tcp.num_arrays       = volume_dim;
            tcp.sample_count     = 1;
            tcp.sample_quality   = 0;
            tcp.usage            = PEN_USAGE_DEFAULT;
            tcp.bind_flags       = PEN_BIND_SHADER_RESOURCE;
            tcp.cpu_access_flags = 0;
            tcp.flags            = 0;
            tcp.block_size       = block_size;
            tcp.pixels_per_block = 1;
            tcp.data             = volume_data;
            tcp.data_size        = data_size;

            if (generate_mips)
            {
                // Mips will create their own copy of mem
                switch (tex_format)
                {
                case PEN_TEX_FORMAT_BGRA8_UNORM:
                    generate_mips_rgba8(tcp);
                    break;
                case PEN_TEX_FORMAT_R32_FLOAT:
                {
#if PEN_SIMD
                    generate_mips_r32f_simd(tcp);
#else
                    generate_mips_r32f(tcp);
#endif
                }
                    break;
                default:
                    PEN_ASSERT(0); //un-implemented mip map gen fucntion
                    break;
                }
            }
            else
            {
                // take a copy of volume data to keep inside k_generated_volume_tcp
                // so it can be saved out later

                tcp.data = pen::memory_alloc(data_size);
                pen::memory_cpy(tcp.data, volume_data, data_size);
            }

            // Keep around cpu texture data for all volumes to save them to disk later
            sb_push(k_generated_volume_tcp, tcp);

            return pen::renderer_create_texture(tcp);
        }

        void volume_raster_completed(ces::entity_scene* scene)
        {
            if (k_rasteriser_job.combine_in_progress == 0)
            {
                k_rasteriser_job.combine_in_progress = 1;
                pen::thread_create_job(raster_voxel_combine, 1024 * 1024 * 1024, &k_rasteriser_job,
                                       pen::THREAD_START_DETACHED);
                return;
            }
            else
            {
                if (k_rasteriser_job.combine_in_progress < 2)
                    return;
            }

            u32& volume_dim = k_rasteriser_job.dimension;

            // create texture
            u32 volume_texture = create_volume_from_data(volume_dim, k_rasteriser_job.block_size, k_rasteriser_job.data_size,
                                                         PEN_TEX_FORMAT_BGRA8_UNORM, k_rasteriser_job.volume_data, k_rasteriser_job.options.generate_mips);

            // create material for volume ray trace
            material_resource* volume_material = new material_resource;
            volume_material->material_name     = "volume_material";
            volume_material->shader_name       = "pmfx_utility";
            volume_material->id_shader         = PEN_HASH("pmfx_utility");
            volume_material->id_technique      = PEN_HASH("volume_texture");

            // volume_material->id_sampler_state[SN_VOLUME_TEXTURE] = PEN_HASH("clamp_linear_sampler_state");
            volume_material->id_sampler_state[SN_VOLUME_TEXTURE] = PEN_HASH("clamp_point_sampler_state");

            volume_material->texture_handles[SN_VOLUME_TEXTURE] = volume_texture;
            add_material_resource(volume_material);

            geometry_resource* cube = get_geometry_resource(PEN_HASH("cube"));

            vec3f scale = (k_rasteriser_job.visible_extents.max - k_rasteriser_job.visible_extents.min) / 2.0f;
            vec3f pos   = k_rasteriser_job.visible_extents.min + scale;

            u32 new_prim           = get_new_node(scene);
            scene->names[new_prim] = "volume";
            scene->names[new_prim].appendf("%i", new_prim);
            scene->transforms[new_prim].rotation    = quat();
            scene->transforms[new_prim].scale       = scale;
            scene->transforms[new_prim].translation = pos;
            scene->entities[new_prim] |= CMP_TRANSFORM | CMP_VOLUME;
            scene->parents[new_prim] = new_prim;
            instantiate_geometry(cube, scene, new_prim);
            instantiate_material(volume_material, scene, new_prim);
            instantiate_model_cbuffer(scene, new_prim);

            // clean up
            for (u32 a = 0; a < 6; ++a)
            {
                for (u32 s = 0; s < k_rasteriser_job.dimension; ++s)
                    pen::memory_free(k_rasteriser_job.volume_slices[a][s]);

                pen::memory_free(k_rasteriser_job.volume_slices[a]);
            }

            // save to disk?
            pen::memory_free(k_rasteriser_job.volume_data);

            // completed
            k_rasteriser_job.rasterise_in_progress = false;
            k_rasteriser_job.combine_in_progress   = 0;
        }

        void volume_rasteriser_update(put::scene_controller* sc)
        {
            if (g_cancel_volume_job)
            {
                k_rasteriser_job.rasterise_in_progress = 0;
                if (k_rasteriser_job.current_requested_slice == k_rasteriser_job.current_slice)
                    g_cancel_handled = true;
            }

            // update incremental job
            if (!k_rasteriser_job.rasterise_in_progress)
                return;

            if (k_rasteriser_job.current_requested_slice == k_rasteriser_job.current_slice)
                return;

            if (k_rasteriser_job.current_slice >= k_rasteriser_job.dimension)
            {
                while (!(k_rasteriser_job.options.rasterise_axes & 1 << (++k_rasteriser_job.current_axis)))
                    if (k_rasteriser_job.current_axis > 5)
                        break;

                k_rasteriser_job.current_slice = 0;
            }

            if (k_rasteriser_job.current_axis > 5)
            {
                volume_raster_completed(sc->scene);
                return;
            }

            if (!(k_rasteriser_job.options.rasterise_axes & 1 << k_rasteriser_job.current_axis))
            {
                k_rasteriser_job.current_axis++;
                return;
            }

            u32& volume_dim              = k_rasteriser_job.dimension;
            s32& current_slice           = k_rasteriser_job.current_slice;
            s32& current_axis            = k_rasteriser_job.current_axis;
            s32& current_requested_slice = k_rasteriser_job.current_requested_slice;

            vec3f min = k_rasteriser_job.visible_extents.min;
            vec3f max = k_rasteriser_job.visible_extents.max;

            vec3f dim           = max - min;
            f32   texel_boarder = component_wise_max(dim) / volume_dim;

            min -= texel_boarder;
            max += texel_boarder;

            static mat4 axis_swaps[] = {mat::create_axis_swap(vec3f::unit_x(), vec3f::unit_y(), -vec3f::unit_z()),
                                        mat::create_axis_swap(vec3f::unit_x(), -vec3f::unit_z(), vec3f::unit_y()),
                                        mat::create_axis_swap(-vec3f::unit_z(), vec3f::unit_y(), vec3f::unit_x()),

                                        mat::create_axis_swap(vec3f::unit_x(), vec3f::unit_y(), -vec3f::unit_z()),
                                        mat::create_axis_swap(vec3f::unit_x(), -vec3f::unit_z(), vec3f::unit_y()),
                                        mat::create_axis_swap(-vec3f::unit_z(), vec3f::unit_y(), vec3f::unit_x())};

            vec3f smin[] = {vec3f(max.x, min.y, min.z), vec3f(min.x, min.z, min.y), vec3f(min.z, min.y, min.x),

                            vec3f(min.x, min.y, max.z), vec3f(min.x, max.z, max.y), vec3f(max.z, min.y, max.x)};

            vec3f smax[] = {vec3f(min.x, max.y, max.z), vec3f(max.x, max.z, max.y), vec3f(max.z, max.y, max.x),

                            vec3f(max.x, max.y, min.z), vec3f(max.x, min.z, min.y), vec3f(min.z, max.y, min.x)};

            vec3f mmin = smin[current_axis];
            vec3f mmax = smax[current_axis];

            f32 slice_thickness = (mmax.z - mmin.z) / volume_dim;
            f32 near_slice      = mmin.z + slice_thickness * current_slice;

            mmin.z = near_slice;
            mmax.z = near_slice + slice_thickness;

            put::camera_create_orthographic(&k_volume_raster_ortho, mmin.x, mmax.x, mmin.y, mmax.y, mmin.z, mmax.z);
            k_volume_raster_ortho.view = axis_swaps[current_axis];

            k_rasteriser_job.current_slice_aabb.min = k_volume_raster_ortho.view.transform_vector(mmin);
            k_rasteriser_job.current_slice_aabb.max = k_volume_raster_ortho.view.transform_vector(mmax);

            k_rasteriser_job.current_slice_aabb.min.z *= -1;
            k_rasteriser_job.current_slice_aabb.max.z *= -1;

            static hash_id             id_volume_raster = PEN_HASH("volume_raster");
            const pmfx::render_target* rt               = pmfx::get_render_target(id_volume_raster);

            pen::resource_read_back_params rrbp;
            rrbp.block_size         = 4;
            rrbp.row_pitch          = volume_dim * rrbp.block_size;
            rrbp.depth_pitch        = volume_dim * rrbp.row_pitch;
            rrbp.data_size          = rrbp.depth_pitch;
            rrbp.resource_index     = rt->handle;
            rrbp.format             = PEN_TEX_FORMAT_BGRA8_UNORM;
            rrbp.call_back_function = image_read_back;

            pen::renderer_read_back_resource(rrbp);
            current_requested_slice = current_slice;
        }

        PEN_TRV sdf_generate(void* params)
        {
            pen::job_thread_params* job_params = (pen::job_thread_params*)params;
            vgt_sdf_job*            sdf_job    = (vgt_sdf_job*)job_params->user_data;

            pen::job* p_thread_info = job_params->job_info;
            pen::thread_semaphore_signal(p_thread_info->p_sem_continue, 1);

            u32 volume_dim = 1 << sdf_job->options.volume_dimension;

            // create a simple 3d texture
            u32 block_size = sdf_job->block_size;
            u32 data_size  = volume_dim * volume_dim * volume_dim * block_size;

            u8* volume_data = (u8*)pen::memory_alloc(data_size);
            u32 row_pitch   = volume_dim * block_size;
            u32 slice_pitch = volume_dim * row_pitch;

            std::vector<vec3f>  vertices;
            std::vector<vec3ui> triangles;

            extents ve = {vec3f(FLT_MAX), vec3f(-FLT_MAX)};

            u32 index_offset = 0;

            for (u32 n = 0; n < sdf_job->scene->nodes_size; ++n)
            {
                if (sdf_job->scene->entities[n] & CMP_GEOMETRY)
                {
                    if (k_sdf_job.capture_type == CAPTURE_SELECTED)
                    {
                        if (!(sdf_job->scene->state_flags[n] & SF_SELECTED) &&
                            !(sdf_job->scene->state_flags[n] & SF_CHILD_SELECTED))
                            continue;

                        ve.min = min_union(ve.min, sdf_job->scene->bounding_volumes[n].transformed_min_extents);
                        ve.max = max_union(ve.max, sdf_job->scene->bounding_volumes[n].transformed_max_extents);
                    }
                    else
                    {
                        ve = k_sdf_job.scene->renderable_extents;
                    }

                    geometry_resource* gr = get_geometry_resource(sdf_job->scene->id_geometry[n]);

                    vec4f* vertex_positions = (vec4f*)gr->cpu_position_buffer;

                    if (!gr->cpu_index_buffer || !vertex_positions)
                    {
                        dev_console_log_level(dev_ui::CONSOLE_ERROR,
                                              "[error] mesh %s does not have cpu vertex / triangle data",
                                              sdf_job->scene->names[n].c_str());

                        continue;
                    }

                    index_offset = vertices.size();
                    for (u32 i = 0; i < gr->num_vertices; ++i)
                    {
                        vec3f tv = sdf_job->scene->world_matrices[n].transform_vector(vertex_positions[i].xyz);
                        vertices.push_back(tv);
                    }

                    for (u32 i = 0; i < gr->num_indices; i += 3)
                    {
                        if (gr->index_type == PEN_FORMAT_R32_UINT)
                        {
                            u32* indices = (u32*)gr->cpu_index_buffer;
                            u32  i0, i1, i2;
                            i0 = index_offset + indices[i + 0];
                            i1 = index_offset + indices[i + 1];
                            i2 = index_offset + indices[i + 2];
                            triangles.push_back(vec3ui(i0, i1, i2));
                        }
                        else
                        {
                            u16* indices = (u16*)gr->cpu_index_buffer;
                            u16  i0, i1, i2;
                            i0 = index_offset + indices[i + 0];
                            i1 = index_offset + indices[i + 1];
                            i2 = index_offset + indices[i + 2];
                            triangles.push_back(vec3ui(i0, i1, i2));
                        }
                    }
                }
            }

            k_sdf_job.scene_extents = ve;

            vec3f sd = k_sdf_job.scene_extents.max - k_sdf_job.scene_extents.min;
            k_sdf_job.scene_extents.min -= sd * k_sdf_job.padding;
            k_sdf_job.scene_extents.max += sd * k_sdf_job.padding;

            extents scene_extents   = k_sdf_job.scene_extents;
            vec3f   scene_dimension = scene_extents.max - scene_extents.min;

            sdf_job->volume_data = volume_data;
            sdf_job->volume_dim  = volume_dim;
            sdf_job->data_size   = data_size;

            if (triangles.size() > 0)
            {
                f32 dx = component_wise_max(scene_dimension) / (f32)volume_dim;

                vec3f centre = scene_extents.min + ((scene_extents.max - scene_extents.min) / 2.0f);

                vec3f grid_origin = centre - vec3f(component_wise_max(scene_dimension) / 2.0f);

                Array3f phi_grid;
                make_level_set3(triangles, vertices, grid_origin, dx, volume_dim, volume_dim, volume_dim, phi_grid);

                if (g_cancel_volume_job)
                {
                    k_sdf_job.generate_in_progress = false;
                    g_mls_progress.sweeps          = 0;
                    g_mls_progress.triangles       = 0;
                    pen::memory_free(volume_data);
                    g_cancel_handled = true;
                    return PEN_THREAD_OK;
                }

                for (u32 z = 0; z < volume_dim; ++z)
                {
                    for (u32 y = 0; y < volume_dim; ++y)
                    {
                        for (u32 x = 0; x < volume_dim; ++x)
                        {
                            u32 offset = z * slice_pitch + y * row_pitch + x * block_size;

                            f32* f = (f32*)(&volume_data[offset + 0]);

                            // non water tight meshes signs cannot be trusted
                            f32 tsf = phi_grid(x, y, z);
                            if (!sdf_job->trust_sign)
                                tsf = fabs(tsf);

                            *f = tsf;
                        }
                    }
                }
            }
            else
            {
                dev_console_log_level(dev_ui::CONSOLE_ERROR, "%s", "[error] no triangles in scene to generate sdf");
            }

            if (p_thread_info->p_completion_callback)
                p_thread_info->p_completion_callback(nullptr);

            sdf_job->generate_in_progress = 2;
            return PEN_THREAD_OK;
        }

        static ces::entity_scene* k_main_scene;
        void                      init(ces::entity_scene* scene)
        {
            k_main_scene = scene;
            put::scene_controller cc;
            cc.camera          = &k_volume_raster_ortho;
            cc.update_function = &volume_rasteriser_update;
            cc.name            = "volume_rasteriser_camera";
            cc.id_name         = PEN_HASH(cc.name.c_str());
            cc.scene           = scene;

            pmfx::register_scene_controller(cc);
        }

        static vgt_options k_options;

        void rasterise_ui()
        {
            static const c8* axis_names[] = {"z+", "y+", "x+", "z-", "y-", "x-"};

            ImGui::Text("Rasterise Axes");

            for (u32 a = 0; a < k_num_axes; a++)
            {
                ImGui::CheckboxFlags(axis_names[a], &k_options.rasterise_axes, 1 << a);

                if (a < k_num_axes - 1)
                    ImGui::SameLine();
            }

            static const c8* capture_data_names[] = {"Albedo", "Normals", "Baked Lighting", "Occupancy", "Custom"};

            ImGui::Combo("Capture", &k_options.capture_data, capture_data_names, PEN_ARRAY_SIZE(capture_data_names));

            static u32* hidden_entities = nullptr;

            if (!k_rasteriser_job.rasterise_in_progress)
            {
                if (ImGui::Button("Generate"))
                {
                    g_cancel_volume_job = 0;

                    if (k_rasteriser_job.capture_type == CAPTURE_SELECTED)
                    {
                        // hide stuff we dont want
                        extents ve = {vec3f(FLT_MAX), vec3f(-FLT_MAX)};

                        for (u32 n = 0; n < k_main_scene->num_nodes; ++n)
                        {
                            if (k_main_scene->state_flags[n] & SF_HIDDEN)
                                continue;

                            if (!(k_main_scene->state_flags[n] & SF_SELECTED) &&
                                !(k_main_scene->state_flags[n] & SF_CHILD_SELECTED))
                            {
                                k_main_scene->state_flags[n] |= SF_HIDDEN;
                                sb_push(hidden_entities, n);
                            }
                            else
                            {
                                ve.min = min_union(ve.min, k_main_scene->bounding_volumes[n].transformed_min_extents);
                                ve.max = max_union(ve.max, k_main_scene->bounding_volumes[n].transformed_max_extents);
                            }
                        }

                        k_rasteriser_job.visible_extents = ve;
                    }
                    else
                    {
                        k_rasteriser_job.visible_extents = k_main_scene->renderable_extents;
                    }

                    // setup new job
                    k_rasteriser_job.options = k_options;
                    u32 dim                  = 1 << k_rasteriser_job.options.volume_dimension;

                    k_rasteriser_job.dimension     = dim;
                    k_rasteriser_job.current_axis  = 0;
                    k_rasteriser_job.current_slice = 0;

                    // allocate cpu mem for rasterised slices
                    for (u32 a = 0; a < 6; ++a)
                    {
                        // alloc slices array
                        k_rasteriser_job.volume_slices[a] =
                            (void**)pen::memory_alloc(k_rasteriser_job.dimension * sizeof(void**));

                        // alloc slices mem
                        for (u32 s = 0; s < k_rasteriser_job.dimension; ++s)
                            k_rasteriser_job.volume_slices[a][s] = pen::memory_alloc(pow(k_rasteriser_job.dimension, 2) * 4);
                    }

                    // flag to start reasterising
                    k_rasteriser_job.rasterise_in_progress = true;
                }

                ImGui::SameLine();
                ImGui::Combo("", &k_rasteriser_job.capture_type, "Whole Scene\0Selected");
            }
            else
            {
                ImGui::Separator();

                if (ImGui::Button("Cancel"))
                    g_cancel_volume_job = 1;

                ImGui::SameLine();

                if (k_rasteriser_job.combine_in_progress > 0)
                {
                    if (hidden_entities)
                    {
                        // unhide
                        u32 c = sb_count(hidden_entities);
                        for (u32 i = 0; i < c; ++i)
                        {
                            u32 n = hidden_entities[i];
                            k_main_scene->state_flags[n] &= ~SF_HIDDEN;
                        }

                        sb_clear(hidden_entities);
                    }

                    f32 progress = (f32)k_rasteriser_job.combine_position / (f32)pow(k_rasteriser_job.dimension, 3);
                    ImGui::ProgressBar(progress);
                }
                else
                {
                    f32 a = (f32)(k_rasteriser_job.current_axis * k_rasteriser_job.dimension) + k_rasteriser_job.current_slice;
                    f32 total = 6 * k_rasteriser_job.dimension;

                    f32 progress = 0.0f;
                    if( a > 0.0f)
                        progress = a / total;

                    ImGui::ProgressBar(progress);

                    if (ImGui::CollapsingHeader("Render Target Output"))
                    {
                        static hash_id             id_volume_raster_rt = PEN_HASH("volume_raster");
                        const pmfx::render_target* volume_rt           = pmfx::get_render_target(id_volume_raster_rt);
                        ImGui::Image((void*)&volume_rt->handle, ImVec2(256, 256));
                    }

                    put::dbg::add_aabb(k_rasteriser_job.current_slice_aabb.min, k_rasteriser_job.current_slice_aabb.max,
                                       vec4f::cyan());
                }
            }
        }

        void add_volume_test()
        {
            u32 volume_texture = put::load_texture("data/textures/tester.dds");

            // create material for volume sdf sphere trace
            material_resource* sdf_material                   = new material_resource;
            sdf_material->material_name                       = "volume_sdf_material";
            sdf_material->shader_name                         = "pmfx_utility";
            sdf_material->id_shader                           = PEN_HASH("pmfx_utility");
            sdf_material->id_technique                        = PEN_HASH("volume_sdf");
            sdf_material->id_sampler_state[SN_VOLUME_TEXTURE] = PEN_HASH("clamp_linear_sampler_state");
            sdf_material->texture_handles[SN_VOLUME_TEXTURE]  = volume_texture;
            add_material_resource(sdf_material);

            geometry_resource* cube = get_geometry_resource(PEN_HASH("cube"));

            vec3f scale = vec3f(1.0f);
            vec3f pos   = vec3f::zero();

            u32 new_prim                  = get_new_node(k_main_scene);
            k_main_scene->names[new_prim] = "volume";
            k_main_scene->names[new_prim].appendf("%i", new_prim);
            k_main_scene->transforms[new_prim].rotation    = quat();
            k_main_scene->transforms[new_prim].scale       = scale;
            k_main_scene->transforms[new_prim].translation = pos;
            k_main_scene->entities[new_prim] |= CMP_TRANSFORM | CMP_SDF_SHADOW;
            k_main_scene->parents[new_prim] = new_prim;
            instantiate_geometry(cube, k_main_scene, new_prim);
            instantiate_material(sdf_material, k_main_scene, new_prim);
            instantiate_model_cbuffer(k_main_scene, new_prim);
        }

        void sdf_ui()
        {
            static const c8* texture_fromat[] = {
                "8bit",
                "32bit Floating Point",
            };

            static s32 sdf_texture_format = 1;
            ImGui::Combo("Capture", &sdf_texture_format, texture_fromat, PEN_ARRAY_SIZE(texture_fromat));
            ImGui::Checkbox("Signed", &k_sdf_job.trust_sign);
            ImGui::InputFloat("Padding", &k_sdf_job.padding);

            if (!k_sdf_job.generate_in_progress)
            {
                if (ImGui::Button("Generate"))
                {
                    g_cancel_volume_job = 0;

                    if (sdf_texture_format == 0)
                    {
                        k_sdf_job.block_size     = 1;
                        k_sdf_job.texture_format = PEN_TEX_FORMAT_R8_UNORM;
                    }
                    else
                    {
                        k_sdf_job.block_size     = 4;
                        k_sdf_job.texture_format = PEN_TEX_FORMAT_R32_FLOAT;
                    }

                    k_sdf_job.generate_in_progress = 1;
                    k_sdf_job.scene                = k_main_scene;
                    k_sdf_job.options              = k_options;

                    pen::thread_create_job(sdf_generate, 1024 * 1024 * 1024, &k_sdf_job, pen::THREAD_START_DETACHED);
                    return;
                }

                ImGui::SameLine();
                ImGui::Combo("", &k_sdf_job.capture_type, "Whole Scene\0Selected");
            }
            else
            {
                if (ImGui::Button("Cancel"))
                    g_cancel_volume_job = 1;

                ImGui::SameLine();

                ImGui::ProgressBar((g_mls_progress.triangles + g_mls_progress.sweeps) * 0.5f, ImVec2(-1, 0));

                if (k_sdf_job.generate_in_progress == 2)
                {
                    // create texture
                    u32 volume_texture =
                        create_volume_from_data(k_sdf_job.volume_dim, k_sdf_job.block_size, k_sdf_job.data_size,
                                                k_sdf_job.texture_format, k_sdf_job.volume_data, k_sdf_job.options.generate_mips);

                    // create material for volume sdf sphere trace
                    material_resource* sdf_material                   = new material_resource;
                    sdf_material->material_name                       = "volume_sdf_material";
                    sdf_material->shader_name                         = "pmfx_utility";
                    sdf_material->id_shader                           = PEN_HASH("pmfx_utility");
                    sdf_material->id_technique                        = PEN_HASH("volume_sdf");
                    sdf_material->id_sampler_state[SN_VOLUME_TEXTURE] = PEN_HASH("clamp_linear_sampler_state");
                    sdf_material->texture_handles[SN_VOLUME_TEXTURE]  = volume_texture;
                    add_material_resource(sdf_material);

                    geometry_resource* cube = get_geometry_resource(PEN_HASH("cube"));

                    f32 single_scale = component_wise_max((k_sdf_job.scene_extents.max - k_sdf_job.scene_extents.min) / 2.0f);
                    vec3f scale      = vec3f(single_scale);
                    vec3f pos        = k_sdf_job.scene_extents.min + scale;

                    u32 new_prim                  = get_new_node(k_main_scene);
                    k_main_scene->names[new_prim] = "volume";
                    k_main_scene->names[new_prim].appendf("%i", new_prim);
                    k_main_scene->transforms[new_prim].rotation    = quat();
                    k_main_scene->transforms[new_prim].scale       = scale;
                    k_main_scene->transforms[new_prim].translation = pos;
                    k_main_scene->entities[new_prim] |= CMP_TRANSFORM | CMP_SDF_SHADOW;
                    k_main_scene->parents[new_prim] = new_prim;
                    instantiate_geometry(cube, k_main_scene, new_prim);
                    instantiate_material(sdf_material, k_main_scene, new_prim);
                    instantiate_model_cbuffer(k_main_scene, new_prim);

                    // add shadow receiver
                    material_resource* sdf_shadow_material                   = new material_resource;
                    sdf_shadow_material->material_name                       = "shadow_sdf_material";
                    sdf_shadow_material->shader_name                         = "pmfx_utility";
                    sdf_shadow_material->id_shader                           = PEN_HASH("pmfx_utility");
                    sdf_shadow_material->id_technique                        = PEN_HASH("shadow_sdf");
                    sdf_shadow_material->id_sampler_state[SN_VOLUME_TEXTURE] = PEN_HASH("clamp_linear_sampler_state");
                    sdf_shadow_material->texture_handles[SN_VOLUME_TEXTURE]  = volume_texture;
                    add_material_resource(sdf_shadow_material);

                    new_prim                      = get_new_node(k_main_scene);
                    k_main_scene->names[new_prim] = "volume_receiever";
                    k_main_scene->names[new_prim].appendf("%i", new_prim);
                    k_main_scene->transforms[new_prim].rotation    = quat();
                    k_main_scene->transforms[new_prim].scale       = vec3f(10, 1, 10);
                    k_main_scene->transforms[new_prim].translation = vec3f(0, -1, 0);
                    k_main_scene->entities[new_prim] |= CMP_TRANSFORM;
                    k_main_scene->parents[new_prim] = new_prim;
                    instantiate_geometry(cube, k_main_scene, new_prim);
                    instantiate_material(sdf_shadow_material, k_main_scene, new_prim);
                    instantiate_model_cbuffer(k_main_scene, new_prim);

                    k_sdf_job.generate_in_progress = 0;
                }
            }
        }

        void show_dev_ui()
        {
            // main menu option -------------------------------------------------
            ImGui::BeginMainMenuBar();

            static bool open_vgt = false;
            if (ImGui::Button(ICON_FA_CUBE))
            {
                open_vgt = true;
            }
            put::dev_ui::set_tooltip("Volume Generator");

            ImGui::EndMainMenuBar();

            // volume generator ui -----------------------------------------------
            if (open_vgt)
            {
                ImGui::Begin("Volume Generator", &open_vgt, ImGuiWindowFlags_AlwaysAutoResize);

                // choose volume data type
                static const c8* volume_type[] = { "Rasterised Texels", "Signed Distance Field" };

                ImGui::Combo("Type", &k_options.volume_type, volume_type, PEN_ARRAY_SIZE(volume_type));

                // choose resolution
                static const c8* dimensions[] = {"1", "2", "4", "8", "16", "32", "64", "128", "256", "512"};

                ImGui::Combo("Resolution", &k_options.volume_dimension, dimensions, PEN_ARRAY_SIZE(dimensions));

                ImGui::SameLine();

                float size_mb = (pow(1 << k_options.volume_dimension, 3) * 4) / 1024 / 1024;

                ImGui::LabelText("Size", "%.2f(mb)", size_mb);

                ImGui::Checkbox("Generate Mip Maps", &k_options.generate_mips);

                ImGui::Separator();

                // Generation Jobs
                if (g_cancel_volume_job && !g_cancel_handled)
                {
                    ImGui::Text("%s", "Cancelling Job");
                }
                else
                {
                    if (k_options.volume_type == VOLUME_RASTERISED_TEXELS)
                    {
                        rasterise_ui();
                    }
                    else if (k_options.volume_type == VOLUME_SIGNED_DISTANCE_FIELD)
                    {
                        sdf_ui();
                    }
                }

                // Volumes Generated
                bool has_volumes = false;
                for (u32 n = 0; n < k_main_scene->num_nodes; ++n)
                    if (k_main_scene->entities[n] & (CMP_SDF_SHADOW | CMP_VOLUME))
                        has_volumes = true;

                if (has_volumes)
                {
                    static bool      save_dialog_open = false;
                    static const c8* save_location    = nullptr;

                    ImGui::Separator();
                    ImGui::Text("Generated Volumes");
                    ImGui::Separator();

                    ImGui::BeginGroup();

                    ImGui::Columns(3);

                    for (u32 n = 0; n < k_main_scene->num_nodes; ++n)
                    {
                        if (!(k_main_scene->entities[n] & (CMP_SDF_SHADOW | CMP_VOLUME)))
                            continue;

                        if (ImGui::Selectable(k_main_scene->names[n].c_str()))
                            ces::add_selection(k_main_scene, n);

                        ImGui::NextColumn();

                        if ((k_main_scene->entities[n] & CMP_SDF_SHADOW))
                            ImGui::Text("Signed Distance Field");
                        else
                            ImGui::Text("Volume Texture");

                        ImGui::NextColumn();
                        if (ImGui::Button("Save"))
                            save_dialog_open = true;

                        ImGui::SameLine();
                        if (ImGui::Button("Delete"))
                        {
                            ces::delete_entity(k_main_scene, n);

                            // todo free tcp mem
                        }

                        ImGui::NextColumn();
                    }

                    ImGui::Columns(1);

                    ImGui::EndGroup();

                    if (save_dialog_open)
                    {
                        save_location = dev_ui::file_browser(save_dialog_open, dev_ui::FB_SAVE);
                        if (save_location)
                        {
                            int i = 0;
                        }
                    }
                }

                ImGui::End();
            }
        }

        void post_update()
        {
            static u32     dim                 = 128;
            static hash_id id_volume_raster_rt = PEN_HASH("volume_raster");
            static hash_id id_volume_raster_ds = PEN_HASH("volume_raster_ds");

            u32 cur_dim = 1 << k_options.volume_dimension;

            // resize targets
            if (cur_dim != dim)
            {
                dim = cur_dim;

                pmfx::resize_render_target(id_volume_raster_rt, dim, dim, "rgba8");
                pmfx::resize_render_target(id_volume_raster_ds, dim, dim, "d24s8");
                pmfx::resize_viewports();

                pen::renderer_consume_cmd_buffer();
            }
        }
    } // namespace vgt
} // namespace put
