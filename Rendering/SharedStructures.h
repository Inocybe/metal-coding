//
//  SharedStructures.h
//  learning-metal
//
//  Created by Lev Mitchell on 8/13/26.
//
#ifndef SharedStructures_h
#define SharedStructures_h


#ifdef __METAL_VERSION__
// when compiled using metal shader compiler
#define NS_SIMD metal
using namespace metal;
#else
// when compiled using c++ compiler (idk what xcode uses)
#include <simd/simd.h>
#define NS_SIMD simd
#endif


enum BufferIndices {
    BufferIndexVerticesAttributes = 0,
    BufferIndexUniforms = 1
};


enum ArgumentBufferID {
    ArgumentBufferIDVertices = 0
};

struct Vertex {
    NS_SIMD::float4 position;
#ifdef __METAL_VERSION__
    half4 color;
#else
    simd::packed::half4 color;
#endif
};


#endif
