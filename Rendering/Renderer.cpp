//
//  Renderer.cpp
//  learning-metal
//
//  Created by Lev Mitchell on 8/11/26.
//
#include "Renderer.hpp"
#include <iostream>
#include <simd/simd.h>
#include <cstring>
#include "SharedStructures.h"

Renderer::Renderer(MTL::Device* pDevice) : _pDevice(pDevice->retain()) {
    _pCommandQueue = _pDevice->newCommandQueue();
    buildShaders();
    buildBuffers();
}


Renderer::~Renderer() {
    _pPipelineState->release();
    _pVertexBuffer->release();
    
    _pCommandQueue->release();
    _pDevice->release();
}


void Renderer::draw(MTK::View* pView) {
    NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();
    
    MTL::CommandBuffer* pCmd = _pCommandQueue->commandBuffer();
    MTL::RenderPassDescriptor* pRpd = pView->currentRenderPassDescriptor();
    MTL::RenderCommandEncoder* pEnc = pCmd->renderCommandEncoder(pRpd);
    
    // set render pipeline state for shaders
    pEnc->setRenderPipelineState(_pPipelineState);
    pEnc->setVertexBuffer(_pVertexBuffer, /*offset*/0, /*index*/BufferIndexVerticesAttributes);
    pEnc->drawPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
    
    pEnc->endEncoding();
    pCmd->presentDrawable(pView->currentDrawable());
    pCmd->commit();
    
    pPool->release();
}

void Renderer::buildShaders() {
    NS::Error* pError = nullptr;
    
    MTL::Library* pLibrary = _pDevice->newDefaultLibrary();
    
    if (!pLibrary) {
        std::cerr << pError->localizedDescription()->utf8String() << "\n";
        assert(false);
    }
    
    MTL::Function* pVertexFn = pLibrary->newFunction(NS::String::string("vertexMain", NS::UTF8StringEncoding));
    MTL::Function* pFragmentFn = pLibrary->newFunction(NS::String::string("fragmentMain", NS::UTF8StringEncoding));
    MTL::RenderPipelineDescriptor* pDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    pDesc->setVertexFunction(pVertexFn);
    pDesc->setFragmentFunction(pFragmentFn);
    pDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB);
    
    _pPipelineState = _pDevice->newRenderPipelineState(pDesc, &pError);
    if(!_pPipelineState) {
        std::cerr << pError->localizedDescription()->utf8String() << "\n";
        assert(false);
    }
    
    pVertexFn->release();
    pFragmentFn->release();
    pDesc->release();
    pLibrary->release();
}

void Renderer::buildBuffers() {
    using simd::float4;
    const size_t numVertices = 3;
    
    Vertex vertices[numVertices] = {
        {
            { 0.0f,  0.5f, 0.0f, 1.0f },
            { 1.0f, 0.0f, 0.0f, 1.0f }
        },
        {
            { -0.5f, -0.5f, 0.0f, 1.0f },
            { 0.0f, 1.0f, 0.0f, 1.0f }
        },
        {
            {  0.5f, -0.5f, 0.0f, 1.0f },
            { 0.0f, 0.0f, 1.0f, 1.0f }
        }
    };
    
    const size_t bufferSize = sizeof(vertices);
    
    _pVertexBuffer = _pDevice->newBuffer(bufferSize, MTL::ResourceStorageModeManaged);
    
    memcpy(_pVertexBuffer->contents(), vertices, bufferSize);
    
    _pVertexBuffer->didModifyRange(NS::Range::Make(0, bufferSize));
}

