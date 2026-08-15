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
    BufferIndexFrameData = 1
};


// alignas makes it so sending data works for both metal and c++ compilers
struct alignas(16) FrameData {
    float angle;
};


struct Vertex {
#ifdef __METAL_VERSION__
    half4 position;
    half4 color;
#else
    NS_SIMD::packed::half4 position;
    NS_SIMD::packed::half4 color;
#endif
};


#endif
