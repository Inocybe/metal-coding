//
//  Renderer.hpp
//  learning-metal
//
//  Created by Lev Mitchell on 8/11/26.
//


#pragma once

#include <Metal/Metal.hpp>
#include <MetalKit/MetalKit.hpp>
#include "SharedStructures.h"

class Renderer {
public:
    Renderer(MTL::Device* pDevice);
    ~Renderer();
    
    void draw(MTK::View* pView);
    void buildShaders();
    void buildBuffers();
    
    static const int kMaxFramesInFlight = 3;
    static const int kNumInstances = 32;
private:
    MTL::Device* _pDevice;
    MTL::CommandQueue* _pCommandQueue;
    
    // shader variables
    MTL::RenderPipelineState* _pPipelineState;
    MTL::Buffer* _pVertexBuffer;
    MTL::Buffer* _pIndexBuffer;
    MTL::Buffer* _pInstanceBuffer[kMaxFramesInFlight];
    
    dispatch_semaphore_t _semaphore;
    float _angle = 0.0;
    int _frame = 0;
};
