#include "shader_structs.h"
#include "shared.hlsl"

RWTexture2D<float2> brdf_texture : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 threadId : SV_DispatchThreadID)
{
	//if(threadId.x > parameters.display_width || threadId.y > parameters.display_height)
	//{
	//	return;
	//}

    float2 texcoords = (float2(threadId.xy) + float2(0.5, 0.5)) / float2(512.0, 512.0);
    float2 integratedBRDF = IntegrateBRDF(texcoords.x, texcoords.y);

	brdf_texture[threadId.xy] = integratedBRDF; 
}
