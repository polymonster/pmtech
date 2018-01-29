#include <stdlib.h>

#include <windows.h>
#include <d3d11_1.h>

#include "renderer.h"
#include "structs.h"
#include "memory.h"
#include "pen_string.h"
#include "threads.h"
#include "timer.h"
#include "pen.h"

#include <vector>

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
D3D_DRIVER_TYPE         g_driverType = D3D_DRIVER_TYPE_HARDWARE;
D3D_FEATURE_LEVEL       g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device*           g_device = nullptr;
ID3D11Device1*          g_device_1 = nullptr;
IDXGISwapChain*         g_swap_chain = nullptr;
IDXGISwapChain1*        g_swap_chain_1 = nullptr;
ID3D11RenderTargetView* g_backbuffer_rtv = nullptr;

ID3D11DeviceContext*    g_immediate_context = nullptr;
ID3D11DeviceContext1*   g_immediate_context_1 = nullptr;

extern pen::window_creation_params pen_window;

a_u8 g_window_resize( 0 );

namespace pen
{
    void create_rtvs( u32 crtv, u32 dsv, uint32_t w, uint32_t h );
	void release_render_target_internal(u32 render_target, bool remove_managed = false);

	//--------------------------------------------------------------------------------------
	//  COMMON API
	//--------------------------------------------------------------------------------------
	#define NUM_QUERY_BUFFERS		4
	#define MAX_QUERIES				64 
	#define NUM_CUBEMAP_FACES		6
 
	#define QUERY_DISJOINT			1
	#define QUERY_ISSUED			(1<<1)
	#define QUERY_SO_STATS			(1<<2)

	struct context_state
	{
		context_state()
		{
			active_query_index = 0;
		}

		u32 backbuffer_colour;
		u32 backbuffer_depth;

        u32 active_colour_target[8] = { 0 };
		u32 active_depth_target = 0;
        u32 num_active_colour_targets = 1;

		u32	active_query_index;

	};

	struct clear_state_internal
	{
		FLOAT rgba[ 4 ];
		f32 depth;
        u8 stencil;
		u32 flags;

		mrt_clear mrt[PEN_MAX_MRT];
		u32 num_colour_targets;
	};

	struct texture2d_internal
	{
		ID3D11Texture2D*		  texture = nullptr;
		ID3D11ShaderResourceView* srv = nullptr;
	};

	struct render_target_internal
	{
		texture2d_internal			tex;
		ID3D11RenderTargetView*		rt[NUM_CUBEMAP_FACES] = { nullptr };

        texture2d_internal			tex_msaa;
        ID3D11RenderTargetView*		rt_msaa[NUM_CUBEMAP_FACES] = { nullptr };

		texture2d_internal			tex_read_back;

        DXGI_FORMAT                 format;
		texture_creation_params*	tcp;
	};

	struct depth_stencil_target_internal
	{
		texture2d_internal			tex;
		ID3D11DepthStencilView*		ds[NUM_CUBEMAP_FACES];

        texture2d_internal			tex_msaa;
        ID3D11DepthStencilView*		ds_msaa[NUM_CUBEMAP_FACES];

		texture2d_internal			tex_read_back;

        DXGI_FORMAT                 format;
		texture_creation_params*	tcp;
	};

	struct shader_program
	{
		u32 vertex_shader;
		u32 pixel_shader;
		u32 input_layout;
	};

	enum e_resource_type
	{
		RES_NONE,
		RES_BUFFER,
		RES_TEXTURE,
		RES_RENDER_TARGET
	};

	struct resource_allocation
	{
		u32 type = 0;

		union 
		{
			clear_state_internal*			clear_state;
			ID3D11VertexShader*				vertex_shader;
			ID3D11InputLayout*				input_layout;
			ID3D11PixelShader*				pixel_shader;
			ID3D11GeometryShader*			geometry_shader;
			ID3D11Buffer*					generic_buffer;
			texture2d_internal*				texture_2d;
			ID3D11SamplerState*				sampler_state;
			ID3D11RasterizerState*			raster_state;
			ID3D11BlendState*				blend_state;
			ID3D11DepthStencilState*		depth_stencil_state;
			render_target_internal*			render_target;
			depth_stencil_target_internal*	depth_target;
			shader_program					shader_program;
		};
	};

	struct query_allocation
	{
		u8 asigned_flag;
		ID3D11Query* query			[NUM_QUERY_BUFFERS];
		u32			 flags			[NUM_QUERY_BUFFERS];
		a_u64		 last_result;
	};

	resource_allocation		 resource_pool	[MAX_RENDERER_RESOURCES];
	query_allocation	     query_pool		[MAX_QUERIES];

	void clear_resource_table( )
	{
		pen::memory_zero( &resource_pool[ 0 ], sizeof( resource_allocation ) * MAX_RENDERER_RESOURCES );
	}

	void clear_query_table()
	{
		pen::memory_zero(&query_pool[0], sizeof(query_allocation) * MAX_QUERIES);
	}

	u32 get_next_query_index(u32 domain)
	{
		u32 i = 0;
		while (query_pool[i].asigned_flag & domain)
		{
			++i;
		}

		query_pool[i].asigned_flag |= domain;

		return i;
	};
    
    struct managed_rt
    {
        u32 resource_index;
        texture_creation_params tcp;
    };
    std::vector<managed_rt> k_managed_render_targets;

	context_state			 g_context;

	void direct::renderer_create_clear_state( const clear_state &cs, u32 resource_slot )
	{
		resource_pool[ resource_slot ].clear_state = (pen::clear_state_internal*)pen::memory_alloc( sizeof( clear_state_internal ) );

		resource_pool[ resource_slot ].clear_state->rgba[ 0 ] = cs.r;
		resource_pool[ resource_slot ].clear_state->rgba[ 1 ] = cs.g;
		resource_pool[ resource_slot ].clear_state->rgba[ 2 ] = cs.b;
		resource_pool[ resource_slot ].clear_state->rgba[ 3 ] = cs.a;
		resource_pool[ resource_slot ].clear_state->depth = cs.depth;
        resource_pool[ resource_slot ].clear_state->stencil = cs.stencil;
		resource_pool[ resource_slot ].clear_state->flags = cs.flags;

		resource_pool[resource_slot].clear_state->num_colour_targets = cs.num_colour_targets;

		pen::memory_cpy(resource_pool[resource_slot].clear_state->mrt, cs.mrt, sizeof(mrt_clear)*cs.num_colour_targets);

		mrt_clear* mrt = resource_pool[resource_slot].clear_state->mrt;

		//convert int clears (required on gl) to floats for d3d
		for (s32 i = 0; i < cs.num_colour_targets; ++i)
			if (mrt[i].type == CLEAR_U32)
				for (s32 c = 0; c < 4; ++c)
					mrt[i].f[c] = (f32)cs.mrt[i].u[c];
	}

	f64 renderer_get_last_query(u32 query_index)
	{
		f64 res;
		pen::memory_cpy(&res, &query_pool[query_index].last_result, sizeof(f64));

		return res;
	}

	//--------------------------------------------------------------------------------------
	//  DIRECT API
	//--------------------------------------------------------------------------------------
    void direct::renderer_make_context_current( )
    {
        //unused on dx11 so far
    }

	void direct::renderer_clear( u32 clear_state_index, u32 colour_face, u32 depth_face )
	{
        u32 flags = resource_pool[ clear_state_index ].clear_state->flags;
        
		clear_state_internal* cs = resource_pool[clear_state_index].clear_state;

        //clear colour
        if( flags & PEN_CLEAR_COLOUR_BUFFER )
        {
			if (cs->num_colour_targets == 0)
			{
				for (s32 i = 0; i < g_context.num_active_colour_targets; ++i)
				{
					s32 ct = g_context.active_colour_target[i];

					ID3D11RenderTargetView* colour_rtv = resource_pool[ct].render_target->rt[colour_face];

					if (resource_pool[ct].render_target->rt_msaa[colour_face])
						colour_rtv = resource_pool[ct].render_target->rt_msaa[colour_face];

					g_immediate_context->ClearRenderTargetView(colour_rtv, &resource_pool[clear_state_index].clear_state->rgba[0]);
				}
			}
        }

		//MRT clear colour
		for (s32 i = 0; i < cs->num_colour_targets; ++i)
		{
			s32 ct = g_context.active_colour_target[i];

			ID3D11RenderTargetView* colour_rtv = resource_pool[ct].render_target->rt[colour_face];

			if (resource_pool[ct].render_target->rt_msaa[colour_face])
				colour_rtv = resource_pool[ct].render_target->rt_msaa[colour_face];

			g_immediate_context->ClearRenderTargetView(colour_rtv, cs->mrt[i].f);
		}
        
        u32 d3d_flags = 0;
        if( flags & PEN_CLEAR_DEPTH_BUFFER )
            d3d_flags |= D3D11_CLEAR_DEPTH;
        if( flags & PEN_CLEAR_STENCIL_BUFFER )
            d3d_flags |= D3D11_CLEAR_STENCIL;
        
        u8 stencil_val = resource_pool[ clear_state_index ].clear_state->stencil;

		if ( d3d_flags && g_context.active_depth_target )
		{
            ID3D11DepthStencilView* dsv = resource_pool[g_context.active_depth_target].depth_target->ds[depth_face];
            
            if (resource_pool[g_context.active_depth_target].depth_target->ds_msaa[depth_face])
                dsv = resource_pool[g_context.active_depth_target].depth_target->ds_msaa[depth_face];

			//clear depth
			g_immediate_context->ClearDepthStencilView( dsv, d3d_flags, resource_pool[ clear_state_index ].clear_state->depth, stencil_val );
		}
	}

	static u32 k_resize_counter = 0;
	static bool k_needs_resize = 0;

	void direct::renderer_present( )
	{
		// Just present
		g_swap_chain->Present( 0, 0 );

        if (g_window_resize == 1)
        {
			g_immediate_context->OMSetRenderTargets(0, 0, 0);

			// Release all outstanding references to the swap chain's buffers.
			resource_pool[g_context.backbuffer_depth].depth_target->ds[0]->Release();
			resource_pool[g_context.backbuffer_depth].depth_target->tex.texture->Release();

			resource_pool[g_context.backbuffer_colour].render_target->rt[0]->Release();
			resource_pool[g_context.backbuffer_colour].render_target->tex.texture->Release();

			uint32_t w = pen_window.width;
			uint32_t h = pen_window.height;

			g_swap_chain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);

			create_rtvs(g_context.backbuffer_colour, g_context.backbuffer_depth, w, h);

			k_needs_resize = true;
			k_resize_counter = 0;
            g_window_resize = 0;
        }
		else
		{
			k_resize_counter++;
		}

		if (k_needs_resize && k_resize_counter > 5)
		{
			//recreate dynamic buffers
			for (auto& rt : k_managed_render_targets)
			{
				release_render_target_internal(rt.resource_index);

				renderer_create_render_target(rt.tcp, rt.resource_index, false);
			}
			
			k_needs_resize = 0;
			k_resize_counter = 0;
		}
	}

	void direct::renderer_create_query( u32 query_type, u32 flags )
	{
		u32 resoruce_index = get_next_query_index(1);

		D3D11_QUERY_DESC desc;
		desc.MiscFlags = flags;
		desc.Query = (D3D11_QUERY)query_type;

		for (u32 i = 0; i < NUM_QUERY_BUFFERS; ++i)
		{
			g_device->CreateQuery( &desc, &query_pool[resoruce_index].query[i] );

			if (desc.Query == D3D11_QUERY_TIMESTAMP_DISJOINT)
			{
				query_pool[resoruce_index].flags[i] |= QUERY_DISJOINT;
			}

			if( desc.Query == D3D11_QUERY_SO_STATISTICS )
			{
				query_pool[ resoruce_index ].flags[ i ] |= QUERY_SO_STATS;
			}
		}
	}

	void direct::renderer_set_query(u32 query_index, u32 action)
	{
		u32 qi = g_context.active_query_index;

		query_pool[query_index].flags[qi] |= QUERY_ISSUED;

		if (action == PEN_QUERY_BEGIN)
		{
			g_immediate_context->Begin(query_pool[query_index].query[qi]);
		}
		else if (action == PEN_QUERY_END)
		{
			g_immediate_context->End(query_pool[query_index].query[qi]);
		}
	}

	void direct::renderer_load_shader(const pen::shader_load_params &params, u32 resource_slot )
	{
		HRESULT hr = -1;
		u32 handle_out = (u32)-1;

		u32 resource_index = resource_slot;

		if( params.type == PEN_SHADER_TYPE_VS )
		{
			// Create a vertex shader
			hr = g_device->CreateVertexShader( params.byte_code, params.byte_code_size, nullptr, &resource_pool[ resource_index ].vertex_shader );
		}
		else if( params.type == PEN_SHADER_TYPE_PS )
		{
			// Create a pixel shader
			if (params.byte_code)
			{
				hr = g_device->CreatePixelShader(params.byte_code, params.byte_code_size, nullptr, &resource_pool[resource_index].pixel_shader);
			}
			else
			{
				resource_pool[resource_index].pixel_shader = NULL;
				hr = S_OK;
			}
		}

		if (FAILED( hr ))
		{
            //shader has failed to create
            PEN_ASSERT( 0 );

			resource_index = (u32)-1;
		}
	}

	void direct::renderer_set_shader( u32 shader_index, u32 shader_type )
	{
		if( shader_type == PEN_SHADER_TYPE_VS )
		{
			g_immediate_context->VSSetShader( resource_pool[shader_index].vertex_shader, nullptr, 0);
		}
		else if( shader_type == PEN_SHADER_TYPE_PS )
		{
			g_immediate_context->PSSetShader( resource_pool[shader_index].pixel_shader, nullptr, 0);
		}
		else if( shader_type == PEN_SHADER_TYPE_GS )
		{
			g_immediate_context->GSSetShader( resource_pool[ shader_index ].geometry_shader, nullptr, 0 );
		}
	}

	void direct::renderer_link_shader_program(const shader_link_params &params, u32 resource_slot)
	{
        u32 resource_index = resource_slot;

		auto& sp = resource_pool[resource_index].shader_program;

		//for now d3d just keeps handles to vs, ps and il..
		//the additional requirements of gl and buffer bindings could provide useful reflection info.
		sp.input_layout = params.input_layout;
		sp.pixel_shader = params.pixel_shader;
		sp.vertex_shader = params.vertex_shader;
	}

	void direct::renderer_create_buffer( const buffer_creation_params &params, u32 resource_slot )
	{
		u32 resource_index = resource_slot;

		D3D11_BUFFER_DESC bd;
		ZeroMemory( &bd, sizeof(bd) );

		bd.Usage			= (D3D11_USAGE)params.usage_flags;
		bd.ByteWidth		= params.buffer_size;
		bd.BindFlags		= (D3D11_BIND_FLAG)params.bind_flags;
		bd.CPUAccessFlags	= params.cpu_access_flags;

		HRESULT hr;

		if( params.data )
		{
			D3D11_SUBRESOURCE_DATA initial_data;
			ZeroMemory( &initial_data, sizeof(initial_data) );

			initial_data.pSysMem = params.data;

			hr = g_device->CreateBuffer( &bd, &initial_data, &resource_pool[ resource_index ].generic_buffer );
		}
		else
		{
			hr = g_device->CreateBuffer( &bd, nullptr, &resource_pool[ resource_index ].generic_buffer );
		}

		if ( FAILED( hr ) )
		{
			PEN_ASSERT( 0 );
		}
	}

	void direct::renderer_create_input_layout( const input_layout_creation_params &params, u32 resource_slot )
	{
		u32 resource_index = resource_slot;

		// Create the input layout
		HRESULT hr = g_device->CreateInputLayout( 
			(D3D11_INPUT_ELEMENT_DESC*)params.input_layout , 
			params.num_elements, params.vs_byte_code, 
			params.vs_byte_code_size, 
			&resource_pool[ resource_index ].input_layout );
		
		if ( FAILED( hr ) )
		{
			resource_index = (u32)-1;

			PEN_ASSERT( 0 );
		}
	}
    
    void direct::renderer_set_vertex_buffer( u32 buffer_index, u32 start_slot, u32 num_buffers, const u32* strides, const u32* offsets )
    {
        g_immediate_context->IASetVertexBuffers(
                                                start_slot,
                                                num_buffers,
                                                &resource_pool[ buffer_index ].generic_buffer,
                                                strides,
                                                offsets );
    }
    
    void direct::renderer_set_vertex_buffers( u32* buffer_indices, u32 num_buffers, u32 start_slot, const u32* strides, const u32* offsets )
    {
        ID3D11Buffer* buffers[4];
        
		for (s32 i = 0; i < num_buffers; ++i)
			buffers[i] = resource_pool[buffer_indices[i]].generic_buffer;
            
        g_immediate_context->IASetVertexBuffers(
                                                start_slot,
                                                num_buffers,
                                                buffers,
                                                strides,
                                                offsets );
    }

	void direct::renderer_set_input_layout( u32 layout_index )
	{
		g_immediate_context->IASetInputLayout(  resource_pool[ layout_index ].input_layout );
	}

	void direct::renderer_set_index_buffer( u32 buffer_index, u32 format, u32 offset )
	{
		g_immediate_context->IASetIndexBuffer( resource_pool[ buffer_index ].generic_buffer, (DXGI_FORMAT)format, offset );
	}

	void direct::renderer_draw( u32 vertex_count, u32 start_vertex, u32 primitive_topology )
	{
		g_immediate_context->IASetPrimitiveTopology( ( D3D11_PRIMITIVE_TOPOLOGY ) primitive_topology );
		g_immediate_context->Draw( vertex_count, start_vertex );
	}

	void direct::renderer_draw_indexed( u32 index_count, u32 start_index, u32 base_vertex, u32 primitive_topology )
	{
		g_immediate_context->IASetPrimitiveTopology( ( D3D11_PRIMITIVE_TOPOLOGY ) primitive_topology );
		g_immediate_context->DrawIndexed( index_count, start_index, base_vertex );
	}
    
    void direct::renderer_draw_indexed_instanced(
         u32 instance_count,
         u32 start_instance,
         u32 index_count,
         u32 start_index,
         u32 base_vertex,
         u32 primitive_topology )
    {
        g_immediate_context->IASetPrimitiveTopology( ( D3D11_PRIMITIVE_TOPOLOGY ) primitive_topology );
        g_immediate_context->DrawIndexedInstanced( index_count, instance_count, start_index, base_vertex, start_instance );
    }

	u32 depth_texture_format_to_dsv_format( u32 tex_format )
	{
		switch( tex_format)
		{
		case DXGI_FORMAT_R16_TYPELESS:
			return DXGI_FORMAT_D16_UNORM;

		case DXGI_FORMAT_R32_TYPELESS:
			return DXGI_FORMAT_D32_FLOAT;

		case DXGI_FORMAT_R24G8_TYPELESS:
			return DXGI_FORMAT_D24_UNORM_S8_UINT;

		case DXGI_FORMAT_R32G8X24_TYPELESS:
			return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		}

		//unsupported depth texture type
		PEN_ASSERT(0);

		return 0;
	}

	u32 depth_texture_format_to_srv_format( u32 tex_format )
	{
		switch( tex_format )
		{
		case DXGI_FORMAT_R16_TYPELESS:
			return DXGI_FORMAT_R16_FLOAT;

		case DXGI_FORMAT_R32_TYPELESS:
			return DXGI_FORMAT_R32_FLOAT;

		case DXGI_FORMAT_R24G8_TYPELESS:
			return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

		case DXGI_FORMAT_R32G8X24_TYPELESS:
			return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
		}

		//unsupported depth texture type
		PEN_ASSERT( 0 );

		return 0;
	}


    void renderer_create_render_target_multi( const texture_creation_params& tcp, texture2d_internal* texture_container, ID3D11DepthStencilView** dsv, ID3D11RenderTargetView** rtv )
    {
        HRESULT hr;

        //create an empty texture
        D3D11_TEXTURE2D_DESC texture_desc;
        pen::memory_cpy( &texture_desc, ( void* )&tcp, sizeof( D3D11_TEXTURE2D_DESC ) );
        
        u32 array_size = texture_desc.ArraySize;
        bool texture2dms = texture_desc.SampleDesc.Count > 1;

        if (array_size > 1 && texture2dms)
        {
            //arrays and cubes don't support msaa yet
            PEN_ASSERT( 0 );
        }

        hr = g_device->CreateTexture2D( &texture_desc, NULL, &texture_container->texture );
        PEN_ASSERT( hr == 0 );

        D3D11_SHADER_RESOURCE_VIEW_DESC resource_view_desc;

        D3D_SRV_DIMENSION srv_dimension = texture2dms ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;

        if (array_size == 6)
            srv_dimension = D3D_SRV_DIMENSION_TEXTURECUBE;

        if (texture_desc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
        {
            //depth target
            D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc;
            dsv_desc.Format = ( DXGI_FORMAT )depth_texture_format_to_dsv_format( texture_desc.Format );
            dsv_desc.Flags = 0;

            // Create the render target view.
            if (array_size == 1)
            {
                dsv_desc.ViewDimension = texture2dms ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
                dsv_desc.Texture2D.MipSlice = 0;

                g_device->CreateDepthStencilView( texture_container->texture, &dsv_desc, &dsv[0] );
                PEN_ASSERT( hr == 0 );
            }
            else
            {
                for (u32 a = 0; a < array_size; ++a)
                {
                    dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
                    dsv_desc.Texture2DArray.FirstArraySlice = a;
                    dsv_desc.Texture2DArray.MipSlice = 0;
                    dsv_desc.Texture2DArray.ArraySize = 1;
                    g_device->CreateDepthStencilView( texture_container->texture, &dsv_desc, &dsv[a] );
                    PEN_ASSERT( hr == 0 );
                }
            }

            //create shader resource view
            resource_view_desc.Format = ( DXGI_FORMAT )depth_texture_format_to_srv_format( texture_desc.Format );
            resource_view_desc.ViewDimension = srv_dimension;
            resource_view_desc.Texture2D.MipLevels = -1;
            resource_view_desc.Texture2D.MostDetailedMip = 0;
        }
        else if (texture_desc.BindFlags & D3D11_BIND_RENDER_TARGET)
        {
            //render target
            D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
            rtv_desc.Format = texture_desc.Format;

            // Create the render target view.
            if (array_size == 1)
            {
                rtv_desc.ViewDimension = texture2dms ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
                rtv_desc.Texture2D.MipSlice = 0;

                hr = g_device->CreateRenderTargetView( texture_container->texture, &rtv_desc, &rtv[0] );
                PEN_ASSERT( hr == 0 );
            }
            else
            {
                for (u32 a = 0; a < array_size; ++a)
                {
                    rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                    rtv_desc.Texture2DArray.FirstArraySlice = a;
                    rtv_desc.Texture2DArray.MipSlice = 0;
                    rtv_desc.Texture2DArray.ArraySize = 1;
                    hr = g_device->CreateRenderTargetView( texture_container->texture, &rtv_desc, &rtv[a] );
                    PEN_ASSERT( hr == 0 );
                }
            }

            //create shader resource view
            resource_view_desc.Format = texture_desc.Format;
            resource_view_desc.ViewDimension = srv_dimension;
            resource_view_desc.Texture2D.MipLevels = -1;
            resource_view_desc.Texture2D.MostDetailedMip = 0;
        }
        else
        {
            //m8 this is not a render target
            PEN_ASSERT( 0 );
        }

        hr = g_device->CreateShaderResourceView( texture_container->texture, &resource_view_desc, &texture_container->srv );
        PEN_ASSERT( hr == 0 );
    }

    void direct::renderer_create_render_target( const texture_creation_params& tcp, u32 resource_slot, bool track )
    {
        u32 resource_index = resource_slot;

		resource_pool[resource_index].type = RES_RENDER_TARGET;

        //alloc rt
        resource_pool[resource_index].render_target = ( render_target_internal* )pen::memory_alloc( sizeof( render_target_internal ) );
        pen::memory_zero( resource_pool[resource_index].render_target, sizeof( render_target_internal ) );

        //format required for resolve
        resource_pool[resource_index].render_target->format = (DXGI_FORMAT)tcp.format;
		resource_pool[resource_index].render_target->tcp = nullptr;

        texture_creation_params _tcp = tcp;
        
        if( _tcp.width == -1 )
        {
            _tcp.width = pen_window.width / _tcp.height;
            _tcp.height = pen_window.height / _tcp.height;
            
            if(track)
                k_managed_render_targets.push_back({resource_index, tcp});
        }

		if (_tcp.cpu_access_flags != 0)
		{
			texture_creation_params read_back_tcp = _tcp;
			read_back_tcp.bind_flags = 0;
			read_back_tcp.usage = D3D11_USAGE_STAGING;

			D3D11_TEXTURE2D_DESC texture_desc;
			pen::memory_cpy(&texture_desc, (void*)&read_back_tcp, sizeof(D3D11_TEXTURE2D_DESC));

			HRESULT hr = g_device->CreateTexture2D(&texture_desc, NULL, &resource_pool[resource_index].render_target->tex_read_back.texture );
			PEN_ASSERT(hr==0);

			_tcp.cpu_access_flags = 0;
		}
        
        if (tcp.sample_count > 1)
        {
            //create msaa 
            renderer_create_render_target_multi( _tcp, &resource_pool[resource_index].render_target->tex_msaa, resource_pool[resource_index].depth_target->ds_msaa, resource_pool[resource_index].render_target->rt_msaa );

			//for resolve later
			resource_pool[resource_index].render_target->tcp = new texture_creation_params;
			*resource_pool[resource_index].render_target->tcp = tcp;
        }
		else
		{
			texture_creation_params resolve_tcp = _tcp;
			resolve_tcp.sample_count = 1;
			renderer_create_render_target_multi(resolve_tcp, &resource_pool[resource_index].render_target->tex, resource_pool[resource_index].depth_target->ds, resource_pool[resource_index].render_target->rt);
		}
	}

	void direct::renderer_set_resolve_targets( u32 colour_target, u32 depth_target )
	{
		ID3D11RenderTargetView* colour_rtv[PEN_MAX_MRT] = { 0 };

		if( colour_target > 0 )
			colour_rtv[0] = resource_pool[colour_target].render_target->rt[0];

		ID3D11DepthStencilView* dsv = nullptr; 
		if(depth_target > 0)
			dsv = resource_pool[depth_target].depth_target->ds[0];

		g_immediate_context->OMSetRenderTargets(1, colour_rtv, dsv);
	}

	void direct::renderer_set_targets( const u32* const colour_targets, u32 num_colour_targets, u32 depth_target, u32 colour_face, u32 depth_face )
	{
		g_context.active_depth_target = depth_target;
		g_context.num_active_colour_targets = num_colour_targets;

		u32 num_views = num_colour_targets;
        ID3D11RenderTargetView* colour_rtv[ PEN_MAX_MRT ] = { 0 };
        for( s32 i = 0; i < num_colour_targets; ++i )
        {
            u32 colour_target = colour_targets[i];
            g_context.active_colour_target[i] = colour_target;
            
            if (colour_target != 0)
            {
                if (resource_pool[colour_target].render_target->rt_msaa[colour_face])
                    colour_rtv[ i ] = resource_pool[colour_target].render_target->rt_msaa[colour_face];
                else
                    colour_rtv[ i ] = resource_pool[colour_target].render_target->rt[colour_face];
            }
        }

		ID3D11DepthStencilView* dsv = nullptr;
		if (depth_target != 0)
		{
            if(resource_pool[depth_target].depth_target->ds_msaa[depth_face])
			    dsv = resource_pool[depth_target].depth_target->ds_msaa[depth_face];
            else
                dsv = resource_pool[depth_target].depth_target->ds[depth_face];
		}

		g_immediate_context->OMSetRenderTargets( num_views, colour_rtv, dsv );
	}

	void direct::renderer_create_texture(const texture_creation_params& tcp, u32 resource_slot)
	{
		u32 resource_index = resource_slot;

		HRESULT hr;

		//create an empty texture
		D3D11_TEXTURE2D_DESC texture_desc;
		pen::memory_cpy( &texture_desc, (void*)&tcp, sizeof( D3D11_TEXTURE2D_DESC ) );

		resource_pool[ resource_index ].texture_2d = (texture2d_internal*)pen::memory_alloc( sizeof( texture2d_internal ) );

		hr = g_device->CreateTexture2D( &texture_desc, NULL, &(resource_pool[ resource_index ].texture_2d->texture) );
		PEN_ASSERT( hr == 0 );

		//fill with data 
		if( tcp.data )
		{
			u8* image_data = (u8*)tcp.data;

			u32 current_width = tcp.width / tcp.pixels_per_block;
			u32 current_height = tcp.height / tcp.pixels_per_block;
			u32 block_size = tcp.block_size;

			for (u32 i = 0; i < tcp.num_mips; ++i)
			{
				u32 depth_pitch = current_width * current_height * block_size;
				u32 row_pitch = current_width * block_size;

				g_immediate_context->UpdateSubresource( resource_pool[ resource_index ].texture_2d->texture, i, NULL, image_data, row_pitch, depth_pitch );

				image_data += depth_pitch;

				min( current_width /= 2, 1 );
				min( current_height /= 2, 1 );
			}
		}

		//create shader resource view
		D3D11_SHADER_RESOURCE_VIEW_DESC resource_view_desc;
		resource_view_desc.Format = texture_desc.Format;
		resource_view_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
		resource_view_desc.Texture2D.MipLevels = -1;
		resource_view_desc.Texture2D.MostDetailedMip = 0;

		hr = g_device->CreateShaderResourceView( resource_pool[ resource_index ].texture_2d->texture, &resource_view_desc, &resource_pool[ resource_index ].texture_2d->srv );
		PEN_ASSERT( hr == 0 );
	}

	void direct::renderer_create_sampler( const sampler_creation_params& scp, u32 resource_slot )
	{
		u32 resource_index = resource_slot;

		HRESULT hr;
		hr = g_device->CreateSamplerState( (D3D11_SAMPLER_DESC*)&scp, &resource_pool[ resource_index ].sampler_state );
		PEN_ASSERT( hr == 0 );
	}

	void direct::renderer_set_texture( u32 texture_index, u32 sampler_index, u32 resource_slot, u32 shader_type, u32 flags )
	{
		ID3D11SamplerState* null_sampler = NULL;
		ID3D11ShaderResourceView* null_srv = NULL;

		if ( resource_pool[texture_index].type == RES_RENDER_TARGET && flags & TEXTURE_BIND_MSAA )
		{
			render_target_internal* rt = resource_pool[texture_index].render_target;

			if (shader_type == PEN_SHADER_TYPE_PS)
			{
				g_immediate_context->PSSetSamplers(resource_slot, 1, sampler_index == 0 ? &null_sampler : &resource_pool[sampler_index].sampler_state);
				g_immediate_context->PSSetShaderResources(resource_slot, 1, texture_index == 0 ? &null_srv : &rt->tex_msaa.srv);
			}
			else if (shader_type == PEN_SHADER_TYPE_VS)
			{
				g_immediate_context->VSSetSamplers(resource_slot, 1, sampler_index == 0 ? &null_sampler : &resource_pool[sampler_index].sampler_state);
				g_immediate_context->VSSetShaderResources(resource_slot, 1, texture_index == 0 ? &null_srv : &rt->tex_msaa.srv);
			}
		}
		else
		{
			if (shader_type == PEN_SHADER_TYPE_PS)
			{
				g_immediate_context->PSSetSamplers(resource_slot, 1, sampler_index == 0 ? &null_sampler : &resource_pool[sampler_index].sampler_state);
				g_immediate_context->PSSetShaderResources(resource_slot, 1, texture_index == 0 ? &null_srv : &resource_pool[texture_index].texture_2d->srv);
			}
			else if (shader_type == PEN_SHADER_TYPE_VS)
			{
				g_immediate_context->VSSetSamplers(resource_slot, 1, sampler_index == 0 ? &null_sampler : &resource_pool[sampler_index].sampler_state);
				g_immediate_context->VSSetShaderResources(resource_slot, 1, texture_index == 0 ? &null_srv : &resource_pool[texture_index].texture_2d->srv);
			}
		}
	}

	void direct::renderer_create_rasterizer_state( const rasteriser_state_creation_params &rscp, u32 resource_slot )
	{
		u32 resource_index = resource_slot;
		
		g_device->CreateRasterizerState( (D3D11_RASTERIZER_DESC*)&rscp, &resource_pool[ resource_index ].raster_state );
	}

	void direct::renderer_set_rasterizer_state( u32 rasterizer_state_index )
	{
		g_immediate_context->RSSetState( resource_pool[ rasterizer_state_index ].raster_state );
	}

	void direct::renderer_set_viewport( const viewport &vp )
	{
		g_immediate_context->RSSetViewports( 1, (D3D11_VIEWPORT*)&vp );
	}

	void direct::renderer_create_blend_state( const blend_creation_params &bcp, u32 resource_slot )
	{
		u32 resource_index = resource_slot;

		D3D11_BLEND_DESC bd;
		pen::memory_zero( &bd, sizeof(D3D11_BLEND_DESC) );

		bd.AlphaToCoverageEnable = bcp.alpha_to_coverage_enable;
		bd.IndependentBlendEnable = bcp.independent_blend_enable;

		for (u32 i = 0; i < bcp.num_render_targets; ++i)
		{
			pen::memory_cpy( &bd.RenderTarget[ i ], ( void* ) &(bcp.render_targets[ i ]), sizeof(render_target_blend) );
		}

		g_device->CreateBlendState( &bd, &resource_pool[resource_index].blend_state );
	}

	void direct::renderer_set_blend_state( u32 blend_state_index )
	{
		g_immediate_context->OMSetBlendState( resource_pool[ blend_state_index ].blend_state, NULL, 0xffffffff );
	}

	void direct::renderer_set_constant_buffer( u32 buffer_index, u32 resource_slot, u32 shader_type )
	{
		if (shader_type == PEN_SHADER_TYPE_PS)
		{
			g_immediate_context->PSSetConstantBuffers( resource_slot, 1, &resource_pool[ buffer_index ].generic_buffer );
		}
		else if (shader_type == PEN_SHADER_TYPE_VS)
		{
			g_immediate_context->VSSetConstantBuffers( resource_slot, 1, &resource_pool[ buffer_index ].generic_buffer );
		}
	}

	void direct::renderer_update_buffer( u32 buffer_index, const void* data, u32 data_size, u32 offset )
	{
        D3D11_MAPPED_SUBRESOURCE mapped_res = { 0 };

        g_immediate_context->Map( resource_pool[ buffer_index ].generic_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res );
       
        void* p_data = (void*)((size_t)mapped_res.pData + offset);
        memcpy( p_data, data, data_size );
       
        g_immediate_context->Unmap( resource_pool[ buffer_index ].generic_buffer, 0 );
	}
    
    void direct::renderer_read_back_resource( const resource_read_back_params& rrbp )
    {
		D3D11_MAPPED_SUBRESOURCE mapped_res = { 0 };

		if (resource_pool[rrbp.resource_index].type == RES_RENDER_TARGET)
		{
			render_target_internal* rt = resource_pool[rrbp.resource_index].render_target;

			g_immediate_context->CopyResource(rt->tex_read_back.texture, rt->tex.texture);

			g_immediate_context->Map(rt->tex_read_back.texture, 0, D3D11_MAP_READ, 0, &mapped_res);

			rrbp.call_back_function((void*)mapped_res.pData, mapped_res.RowPitch, mapped_res.DepthPitch, rrbp.block_size );

			g_immediate_context->Unmap(rt->tex_read_back.texture, 0);
		}
    }

	void direct::renderer_create_depth_stencil_state( const depth_stencil_creation_params& dscp, u32 resource_slot )
	{
		u32 resource_index = resource_slot;

		g_device->CreateDepthStencilState( (D3D11_DEPTH_STENCIL_DESC*)&dscp, &resource_pool[ resource_index ].depth_stencil_state );
	}

	void direct::renderer_set_depth_stencil_state( u32 depth_stencil_state )
	{
		g_immediate_context->OMSetDepthStencilState( resource_pool[ depth_stencil_state ].depth_stencil_state, 0xffffffff );
	}

	void direct::renderer_release_shader( u32 shader_index, u32 shader_type )
	{
		if (shader_type == PEN_SHADER_TYPE_PS)
		{
			resource_pool[shader_index].pixel_shader->Release( );
		}
		else if (shader_type == PEN_SHADER_TYPE_VS)
		{
			resource_pool[shader_index].vertex_shader->Release( );
		}
		else if (shader_type == PEN_SHADER_TYPE_GS)
		{
			resource_pool[shader_index].geometry_shader->Release( );
		}
	}

	void direct::renderer_release_buffer( u32 buffer_index )
	{
		resource_pool[buffer_index].generic_buffer->Release( );
	}

	void direct::renderer_release_texture( u32 texture_index )
	{
		resource_pool[texture_index].texture_2d->texture->Release( );
		resource_pool[texture_index].texture_2d->srv->Release( );
	}

	void direct::renderer_release_raster_state( u32 raster_state_index )
	{
		resource_pool[ raster_state_index ].raster_state->Release( );
	}

	void direct::renderer_release_blend_state( u32 blend_state )
	{
	    resource_pool[ blend_state ].blend_state->Release();
	}

	void release_render_target_internal(u32 render_target, bool remove_managed )
	{
		if (remove_managed)
		{
			//remove from managed rt
			for (s32 i = k_managed_render_targets.size() - 1; i >= 0; --i)
			{
				if (k_managed_render_targets[i].resource_index == render_target)
				{
					k_managed_render_targets.erase(k_managed_render_targets.begin() + i);
					break;
				}
			}
		}

		render_target_internal* rt = resource_pool[render_target].render_target;

		for (s32 i = 0; i < NUM_CUBEMAP_FACES; ++i)
		{
			if (rt->rt[i])
			{
				rt->rt[i]->Release();
				rt->rt[i] = nullptr;
			}

			if (rt->rt_msaa[i])
			{
				rt->rt_msaa[i]->Release();
				rt->rt_msaa[i] = nullptr;
			}
		}

		if (rt->tex.texture)
			rt->tex.texture->Release();

		if (rt->tex.srv)
			rt->tex.srv->Release();

		if (rt->tex_msaa.texture)
			rt->tex_msaa.texture->Release();

		if (rt->tex_msaa.srv)
			rt->tex_msaa.srv->Release();

		if (rt->tex_read_back.texture)
			rt->tex_read_back.texture->Release();

		rt->tex_msaa.srv = nullptr;
		rt->tex_msaa.texture = nullptr;
		rt->tex.srv = nullptr;
		rt->tex.texture = nullptr;

		delete rt->tcp;
	}

	void direct::renderer_release_render_target( u32 render_target )
	{
		release_render_target_internal(render_target, true);
	}

	void direct::renderer_release_input_layout( u32 input_layout )
	{
		resource_pool[ input_layout ].input_layout->Release();
	}

	void direct::renderer_release_sampler( u32 sampler )
	{
		resource_pool[ sampler ].sampler_state->Release();
	}

	void direct::renderer_release_depth_stencil_state( u32 depth_stencil_state )
	{
		resource_pool[ depth_stencil_state ].depth_stencil_state->Release();
	}

	void direct::renderer_release_program(u32 program)
	{
		//program is for opengl
	}

	void direct::renderer_release_clear_state(u32 clear_state)
	{

	}

	void direct::renderer_release_query( u32 query )
	{
		for( u32 i = 0; i < NUM_QUERY_BUFFERS; ++i )
		{
			query_pool[ query ].query[ i ]->Release();

			query_pool[query].asigned_flag = 0;
		}
	}

	void direct::renderer_set_stream_out_target( u32 buffer_index )
	{
		if( buffer_index == 0 )
		{
			g_immediate_context->SOSetTargets( 0, NULL, 0 );
		}
		else
		{
			ID3D11Buffer* buffers[] = { resource_pool[buffer_index].generic_buffer };
			UINT offsets[] = {0};

			g_immediate_context->SOSetTargets( 1, buffers, offsets );
		}
	}

	extern resolve_resources g_resolve_resources;
    void direct::renderer_resolve_target( u32 target, e_msaa_resolve_type type )
    {
        render_target_internal* rti = resource_pool[target].render_target;

		//get dimensions for shader
		f32 w = rti->tcp->width;
		f32 h = rti->tcp->height;

		if (rti->tcp->width == -1)
		{
			w = pen_window.width / h;
			h = pen_window.height / h;
		}

		//create resolve surface if required
		if (!rti->tex.texture)
		{
			//create a resolve surface
			texture_creation_params resolve_tcp = *rti->tcp;
			resolve_tcp.sample_count = 1;

			texture_creation_params& _tcp = resolve_tcp;
			_tcp.width = w;
			_tcp.height = h;

			//depth gets resolved into colour textures
			if (rti->format == PEN_TEX_FORMAT_D24_UNORM_S8_UINT)
			{
				resolve_tcp.bind_flags &= ~D3D11_BIND_DEPTH_STENCIL;
				resolve_tcp.bind_flags |= D3D11_BIND_RENDER_TARGET;
				resolve_tcp.format = PEN_TEX_FORMAT_R32_FLOAT;
			}

			renderer_create_render_target_multi(_tcp, &rti->tex, resource_pool[target].depth_target->ds, rti->rt);
		}

		if (!rti->tex_msaa.texture)
		{
			PEN_PRINTF("[error] renderer : render target %i is not an msaa target\n", target);
			return;
		}

		if (type == RESOLVE_CUSTOM)
		{
			resolve_cbuffer cbuf = { w, h, 0.0f, 0.0f };

			direct::renderer_set_resolve_targets(target, 0);

			direct::renderer_update_buffer(g_resolve_resources.constant_buffer, &cbuf, sizeof(cbuf), 0);
			direct::renderer_set_constant_buffer(g_resolve_resources.constant_buffer, 0, PEN_SHADER_TYPE_PS);

			pen::viewport vp = { 0.0f, 0.0f, w, h, 0.0f, 1.0f };
			direct::renderer_set_viewport(vp);

			u32 stride = 24;
			u32 offset = 0;
			direct::renderer_set_vertex_buffer(g_resolve_resources.vertex_buffer, 0, 1, &stride, &offset);
			direct::renderer_set_index_buffer(g_resolve_resources.index_buffer, PEN_FORMAT_R16_UINT, 0);


			direct::renderer_set_texture(target, 0, 0, PEN_SHADER_TYPE_PS, pen::TEXTURE_BIND_MSAA);

			direct::renderer_draw_indexed(6, 0, 0, PEN_PT_TRIANGLELIST);
		}
		else
		{
			if (rti->format == PEN_TEX_FORMAT_D24_UNORM_S8_UINT)
			{
				PEN_PRINTF("[error] renderer : render target %i cannot be resolved as it is a depth target\n", target);
				return;
			}

			g_immediate_context->ResolveSubresource(rti->tex.texture, 0, rti->tex_msaa.texture, 0, rti->format);
		}
    }

	void direct::renderer_create_stream_out_shader( const pen::shader_load_params &params, u32 resource_slot )
	{
		u32 resource_index = resource_slot;

		HRESULT hr = g_device->CreateGeometryShaderWithStreamOutput(
			params.byte_code, 
			params.byte_code_size, 
			(const D3D11_SO_DECLARATION_ENTRY*)params.so_decl_entries, 
			params.so_num_entries, 
			NULL, 
			0, 
			0, 
			NULL, 
			&resource_pool[ resource_index ].geometry_shader );

		pen::memory_free(params.byte_code);
		pen::memory_free(params.so_decl_entries);

		if( FAILED( hr ) )
		{
			PEN_ASSERT(0);
		}
	}

	void direct::renderer_draw_auto()
	{
		g_immediate_context->IASetPrimitiveTopology( ( D3D11_PRIMITIVE_TOPOLOGY ) PEN_PT_POINTLIST );
		g_immediate_context->DrawAuto();
	}

    void direct::renderer_set_scissor_rect( const rect &r )
    {
        const D3D11_RECT rd3d = { ( LONG ) r.left, ( LONG ) r.top, ( LONG ) r.right, ( LONG ) r.bottom };
        g_immediate_context->RSSetScissorRects( 1, &rd3d );
    }

	void direct::renderer_update_queries()
	{
		//update query info
		g_context.active_query_index = (g_context.active_query_index + 1) % NUM_QUERY_BUFFERS;
		u32 qi = g_context.active_query_index;

		u32 current_query = 0;
		D3D10_QUERY_DATA_TIMESTAMP_DISJOINT disjoint;
		u32 frame_ready = 0;
		while (query_pool[current_query].asigned_flag &= 1)
		{
			if (query_pool[current_query].flags[qi] & QUERY_ISSUED)
			{
				if (query_pool[current_query].flags[qi] & QUERY_DISJOINT)
				{
					if (g_immediate_context->GetData(query_pool[current_query].query[qi], &disjoint, sizeof(disjoint), 0) != S_FALSE)
					{
						frame_ready = 1;
					}
				}
				else if( query_pool[current_query].flags[qi] & QUERY_SO_STATS)
				{
					/*
					D3D11_QUERY_DATA_SO_STATISTICS  stats = { 0 };
					u32 size = sizeof( D3D11_QUERY_DATA_SO_STATISTICS );
					while( g_immediate_context->GetData( query_pool[ current_query ].query[ qi ], &stats, size, 0 ) != S_OK )
					{
						int i = 0;
					}

					int i = 0;
					*/
				}
				else
				{
					if (frame_ready)
					{
						if (!disjoint.Disjoint)
						{
							UINT64 TS;
							g_immediate_context->GetData(query_pool[current_query].query[qi], &TS, sizeof(UINT64), 0);

							f64 res = (f64)TS / (f64)disjoint.Frequency;
							pen::memory_cpy(&query_pool[current_query].last_result,&res,sizeof(UINT64));
						}
					}
				}

				if (frame_ready)
				{
					query_pool[current_query].flags[qi] &= ~QUERY_ISSUED;
				}
			}

			current_query++;
		}
	}

	void direct::renderer_replace_resource(u32 dest, u32 src, e_renderer_resource type)
	{
		switch (type)
		{
		case RESOURCE_TEXTURE:
			direct::renderer_release_texture(dest);
			break;
		case RESOURCE_BUFFER:
			direct::renderer_release_buffer(dest);
			break;
		case RESOURCE_VERTEX_SHADER:
			direct::renderer_release_shader(dest, PEN_SHADER_TYPE_VS);
			break;
		case RESOURCE_PIXEL_SHADER:
			direct::renderer_release_shader(dest, PEN_SHADER_TYPE_PS);
			break;
		default:
			break;
		}

		resource_pool[dest] = resource_pool[src];
	}

	//--------------------------------------------------------------------------------------
	// D3D Device Creation
	//--------------------------------------------------------------------------------------
    void create_rtvs( u32 crtv, u32 dsv, uint32_t w, uint32_t h )
    {
		PEN_ASSERT(crtv == PEN_BACK_BUFFER_COLOUR);
		PEN_ASSERT(dsv == PEN_BACK_BUFFER_DEPTH);

		if (!resource_pool[crtv].render_target)
			resource_pool[crtv].render_target = (render_target_internal*)pen::memory_alloc(sizeof(render_target_internal));

		pen::memory_zero(resource_pool[crtv].render_target, sizeof(render_target_internal));

		if (!resource_pool[dsv].depth_target)
			resource_pool[dsv].depth_target = (depth_stencil_target_internal*)pen::memory_alloc(sizeof(depth_stencil_target_internal));

		pen::memory_zero(resource_pool[dsv].depth_target, sizeof(depth_stencil_target_internal));

        // Create a render target view
        HRESULT hr = g_swap_chain->GetBuffer( 0, __uuidof(ID3D11Texture2D), reinterpret_cast< void** >(&resource_pool[crtv].render_target->tex.texture) );
        if (FAILED( hr ))
            return;

        hr = g_device->CreateRenderTargetView(resource_pool[crtv].render_target->tex.texture, nullptr, &resource_pool[crtv].render_target->rt[0] );
        if (FAILED( hr ))
            return;

		g_context.active_depth_target = PEN_BACK_BUFFER_DEPTH;
        g_context.active_colour_target[0] = PEN_BACK_BUFFER_COLOUR;
        g_context.num_active_colour_targets = 1;
		g_context.backbuffer_colour = crtv;
		g_context.backbuffer_depth = dsv;

        // Create depth stencil texture
        D3D11_TEXTURE2D_DESC descDepth;
        ZeroMemory( &descDepth, sizeof( descDepth ) );
        descDepth.Width = w;
        descDepth.Height = h;
        descDepth.MipLevels = 1;
        descDepth.ArraySize = 1;
        descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        descDepth.SampleDesc.Count = pen_window.sample_count;
        descDepth.SampleDesc.Quality = 0;
        descDepth.Usage = D3D11_USAGE_DEFAULT;
        descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        descDepth.CPUAccessFlags = 0;
        descDepth.MiscFlags = 0;
        hr = g_device->CreateTexture2D( &descDepth, nullptr, &resource_pool[dsv].depth_target->tex.texture );
        if (FAILED( hr ))
            return;

        // Create the depth stencil view
        D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
        ZeroMemory( &descDSV, sizeof( descDSV ) );
        descDSV.Format = descDepth.Format;
        descDSV.ViewDimension = pen_window.sample_count > 1 ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
        descDSV.Texture2D.MipSlice = 0;
        hr = g_device->CreateDepthStencilView( resource_pool[dsv].depth_target->tex.texture, &descDSV, &resource_pool[dsv].depth_target->ds[0] );
        if (FAILED( hr ))
            return;

        g_immediate_context->OMSetRenderTargets(1, &resource_pool[crtv].render_target->rt[0], NULL);
    }

	u32 direct::renderer_initialise( void* params, u32 bb_res, u32 bb_depth_res )
	{
		clear_resource_table( );
		
		renderer_params* rp = (renderer_params*)params;
		 
		HRESULT hr = S_OK;

		RECT rc;
		GetClientRect(rp->hwnd, &rc);
		UINT width = rc.right - rc.left;
		UINT height = rc.bottom - rc.top;

		UINT createDeviceFlags = 0;

#ifdef _DEBUG
		createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

		D3D_DRIVER_TYPE driverTypes[] =
		{
			D3D_DRIVER_TYPE_HARDWARE,
			D3D_DRIVER_TYPE_WARP,
			D3D_DRIVER_TYPE_REFERENCE,
		};
		UINT numDriverTypes = ARRAYSIZE(driverTypes);

		D3D_FEATURE_LEVEL featureLevels[] =
		{
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0,
		};
		UINT numFeatureLevels = ARRAYSIZE(featureLevels);

		for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
		{
			g_driverType = driverTypes[driverTypeIndex];
			hr = D3D11CreateDevice(nullptr, g_driverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels,
				D3D11_SDK_VERSION, &g_device, &g_featureLevel, &g_immediate_context);

			if (hr == E_INVALIDARG)
			{
				// DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
				hr = D3D11CreateDevice(nullptr, g_driverType, nullptr, createDeviceFlags, &featureLevels[1], numFeatureLevels - 1,
					D3D11_SDK_VERSION, &g_device, &g_featureLevel, &g_immediate_context);
			}

			if (SUCCEEDED(hr))
				break;
		}
		if (FAILED(hr))
			return hr;

		// Obtain DXGI factory from device (since we used nullptr for pAdapter above)
		IDXGIFactory1* dxgiFactory = nullptr;
		{
			IDXGIDevice* dxgiDevice = nullptr;
			hr = g_device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
			if (SUCCEEDED(hr))
			{
				IDXGIAdapter* adapter = nullptr;
				hr = dxgiDevice->GetAdapter(&adapter);
				if (SUCCEEDED(hr))
				{
					hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory));
					adapter->Release();
				}
				dxgiDevice->Release();
			}
		}
		if (FAILED(hr))
			return hr;

		// Create swap chain
		IDXGIFactory2* dxgiFactory2 = nullptr;
		hr = dxgiFactory->QueryInterface(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory2));
		if (dxgiFactory2)
		{
			// DirectX 11.1 or later
			hr = g_device->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(&g_device_1));
			if (SUCCEEDED(hr))
			{
				(void)g_immediate_context->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&g_immediate_context_1));
			}

			DXGI_SWAP_CHAIN_DESC1 sd;
			ZeroMemory(&sd, sizeof(sd));
			sd.Width = width;
			sd.Height = height;
			sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			sd.SampleDesc.Count = pen_window.sample_count;
			sd.SampleDesc.Quality = 0;
			sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			sd.BufferCount = 1;

			hr = dxgiFactory2->CreateSwapChainForHwnd(g_device, rp->hwnd, &sd, nullptr, nullptr, &g_swap_chain_1);
			if (SUCCEEDED(hr))
			{
				hr = g_swap_chain_1->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(&g_swap_chain));
			}

			dxgiFactory2->Release();
		}
		else
		{
			// DirectX 11.0 systems
			DXGI_SWAP_CHAIN_DESC sd;
			ZeroMemory(&sd, sizeof(sd));
			sd.BufferCount = 1;
			sd.BufferDesc.Width = width;
			sd.BufferDesc.Height = height;
			sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			sd.BufferDesc.RefreshRate.Numerator = 60;
			sd.BufferDesc.RefreshRate.Denominator = 1;
			sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			sd.OutputWindow = rp->hwnd;
			sd.SampleDesc.Count = pen_window.sample_count;
			sd.SampleDesc.Quality = 0;
			sd.Windowed = TRUE;

			hr = dxgiFactory->CreateSwapChain(g_device, &sd, &g_swap_chain);
		}

		dxgiFactory->Release();

		if (FAILED(hr))
			return hr;

        create_rtvs( bb_res, bb_depth_res, width, height );

		return S_OK;
	}

	//--------------------------------------------------------------------------------------
	// Clean up the objects we've created
	//--------------------------------------------------------------------------------------
	void renderer_destroy()
	{
		if (g_immediate_context) g_immediate_context->ClearState();
		if (g_backbuffer_rtv) g_backbuffer_rtv->Release();
		
		if (g_swap_chain) g_swap_chain->Release( );
		if (g_swap_chain_1) g_swap_chain_1->Release( );

		if (g_immediate_context) g_immediate_context->Release( );
		if (g_immediate_context_1) g_immediate_context_1->Release( );

		if (g_device) g_device->Release();
		if (g_device_1) g_device_1->Release();		
	}

    const c8* renderer_get_shader_platform()
    {
        return "hlsl";
    }
    
    bool renderer_viewport_vup( )
    {
        return false;
    }
}

