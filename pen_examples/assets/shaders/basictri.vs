struct vs_input
{
	float4 position : POSITION;
};

struct vs_output
{
	float4 position : SV_POSITION;
};

vs_output main( vs_input input )
{
	vs_output output;
	
	output.position = input.position;
	
    return output;
}