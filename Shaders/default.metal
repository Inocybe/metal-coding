//
//  shader.metal
//  learning-metal
//
//  Created by Lev Mitchell on 8/12/26.
//

#include <metal_stdlib>
#include "SharedStructures.h"

using namespace metal;




struct v2f
{
    float4 position [[position]];
    half4 color;
};

v2f vertex vertexMain(uint vertexId [[vertex_id]], device const Vertex* vertices[[buffer(BufferIndexVerticesAttributes)]], constant FrameData* frameData[[buffer(BufferIndexFrameData)]]) {
    
    float a = frameData->angle;
    float3x3 rot( sin(a), cos(a), 0.0,

                   cos(a), -sin(a), 0.0,

                   0.0, 0.0, 1.0 );

    
    
    v2f out;
    out.position = float4(rot * float3(vertices[vertexId].position.xyz), 1.0);
    out.color = vertices[vertexId].color;
    return out;
}

half4 fragment fragmentMain(v2f in [[stage_in]]) {
    return in.color;
}
