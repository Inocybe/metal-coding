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
    Renderer(MTL::Device* pDevice, MTK::View* pView);
    ~Renderer();
    
    void draw(MTK::View* pView);
    void buildShaders();
    void buildBuffers();
    void buildDepthStencilStates();
    void buildDepthTexture(int width, int height);
    
    static const int kMaxFramesInFlight = 3;
    static const int kNumInstances = 32;
private:
    MTL::Device* _pDevice;
    MTL::CommandQueue* _pCommandQueue;
    MTL::DepthStencilState* _pDepthStencilState;
    MTL::Texture* _pDepthTexture;
    
    // shader variables
    MTL::RenderPipelineState* _pPipelineState;
    MTL::Buffer* _pVertexBuffer;
    MTL::Buffer* _pIndexBuffer;
    MTL::Buffer* _pInstanceBuffer[kMaxFramesInFlight];
    MTL::Buffer* _pCameraBuffer[kMaxFramesInFlight];
    
    dispatch_semaphore_t _semaphore;
    float _angle = 0.0;
    int _frame = 0;
};
