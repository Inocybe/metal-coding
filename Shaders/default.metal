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

v2f vertex vertexMain(
                    device const VertexData*   vertexData   [[buffer(BufferIndexVerticesAttributes)]],
                    device const InstanceData* instanceData [[buffer(BufferIndexInstanceAttributes)]],
                    uint vertexId   [[vertex_id]],
                    uint instanceId [[instance_id]]) {
    
    v2f out;
    float4 pos = vertexData[vertexId].position;               // already float4 in your struct
    out.position = instanceData[instanceId].instanceTransform * pos;
    out.color = (half4)instanceData[instanceId].instanceColor;
    return out;
}

half4 fragment fragmentMain(v2f in [[stage_in]]) {
    return in.color;
}
