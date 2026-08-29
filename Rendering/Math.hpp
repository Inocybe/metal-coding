//
//  math.hpp
//  learning-metal
//
//  Created by Lev Mitchell on 8/24/26.
//
#pragma once

#include <simd/simd.h>
#include <math.h>

namespace math {

inline simd::float4x4 makeIdentity() {
    using simd::float4;
    
    return simd::float4x4(
                          (float4){ 1.f, 0.f, 0.f, 0.f },
                          (float4){ 0.f, 1.f, 0.f, 0.f },
                          (float4){ 0.f, 0.f, 1.f, 0.f },
                          (float4){ 0.f, 0.f, 0.f, 1.f }
                          );
}

inline simd::float4x4 makePerspective(float fovRadians, float aspect, float znear, float zfar) {
    using simd::float4;
    
    float ys = 1.f / tanf(fovRadians * 0.5f);
    float xs = ys / aspect;
    float zs = zfar / (znear - zfar);
    
    return simd::float4x4(
                          (float4){xs, 0.f, 0.f, 0.f},
                          (float4){0.f, ys, 0.f, 0.f},
                          (float4){0.f, 0.f, zs, -1.f},
                          (float4){0.f, 0.f, znear*zs, 0.f}
                          );
}

}
