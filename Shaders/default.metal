//
//  shader.metal
//  learning-metal
//
//  Created by Lev Mitchell on 8/12/26.
//

#include <metal_stdlib>
#include "SharedStructures.h"

using namespace metal;


struct VertexData {
  device Vertex* vertices [[id(ArgumentBufferIDVertices)]];
};

struct v2f
{
    float4 position [[position]];
    half4 color;
};

v2f vertex vertexMain(uint vertexId [[vertex_id]], device const VertexData* argBuffer [[buffer(BufferIndexVerticesAttributes)]]) {
    v2f out;
    out.position = argBuffer->vertices[vertexId].position;
    out.color = argBuffer->vertices[vertexId].color;
    return out;
}

half4 fragment fragmentMain(v2f in [[stage_in]]) {
    return in.color;
}
