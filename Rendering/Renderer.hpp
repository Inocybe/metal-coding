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
private:
    MTL::Device* _pDevice;
    MTL::CommandQueue* _pCommandQueue;
    
    // shader variables
    MTL::RenderPipelineState* _pPipelineState;
    MTL::Buffer* _pVertexBuffer;
};
