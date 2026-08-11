//
//  Renderer.hpp
//  learning-metal
//
//  Created by Lev Mitchell on 8/11/26.
//


#pragma once

#include <Metal/Metal.hpp>
#include <MetalKit/MetalKit.hpp>

class Renderer {
public:
    Renderer(MTL::Device* pDevice);
    ~Renderer();
    void draw(MTK::View* pView);
private:
    MTL::Device* _pDevice;
    MTL::CommandQueue* _pCommandQueue;
};
