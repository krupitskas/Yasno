#include "shader_structs.h"

RWTexture2D<float2> brdf_texture : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 threadId : SV_DispatchThreadID)
{
	//if(threadId.x > parameters.display_width || threadId.y > parameters.display_height)
	//{
	//	return;
	//}

	brdf_texture[threadId.xy] = float2(13, 64); 
}
