struct vs_input
{
	float4 position : POSITION;
	float2 tex_coord: TEXCOORD0;
};

struct vs_output
{
	float4 position : SV_POSITION;
	float2 tex_coord: TEXCOORD0;
};

vs_output main( vs_input input )
{
	vs_output output;
	
	output.position = input.position;
	output.tex_coord = input.tex_coord;
	
    return output;
}