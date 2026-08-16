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
    BufferIndexInstanceAttributes = 1
};

struct VertexData {
    simd::float4 position;
    simd::float4 color;
// --- OLD ---
/*
    NS_SIMD::float4 position;
#ifdef __METAL_VERSION__
    half4 color;
#else
    simd::packed::half4 color;
#endif
 */
};

struct InstanceData {
    simd::float4x4 instanceTransform;
    simd::float4 instanceColor;
};



#endif
