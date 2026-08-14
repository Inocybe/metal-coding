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
    // shader stuff
    _pPipelineState->release();
    _pShaderLibrary->release();
    _pVertexBuffer->release();
    _pArgBuffer->release();
    // other stuff
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
    pEnc->setVertexBuffer(_pArgBuffer, /*offset*/0, /*index*/BufferIndexVerticesAttributes);
    pEnc->useResource(_pVertexBuffer, MTL::ResourceUsageRead, MTL::RenderStageVertex);
    pEnc->drawPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
    
    pEnc->endEncoding();
    pCmd->presentDrawable(pView->currentDrawable());
    pCmd->commit();
    
    pPool->release();
}

void Renderer::buildShaders() {
    NS::Error* pError = nullptr;
    
    _pShaderLibrary = _pDevice->newDefaultLibrary();
            
    if (!_pShaderLibrary) {
        std::cerr << pError->localizedDescription()->utf8String() << "\n";
        assert(false);
    }
    
    _pVertexFn = _pShaderLibrary->newFunction(NS::String::string("vertexMain", NS::UTF8StringEncoding));
    MTL::Function* pFragmentFn = _pShaderLibrary->newFunction(NS::String::string("fragmentMain", NS::UTF8StringEncoding));
    MTL::RenderPipelineDescriptor* pDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    pDesc->setVertexFunction(_pVertexFn);
    pDesc->setFragmentFunction(pFragmentFn);
    pDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB);
    
    _pPipelineState = _pDevice->newRenderPipelineState(pDesc, &pError);
    if(!_pPipelineState) {
        std::cerr << pError->localizedDescription()->utf8String() << "\n";
        assert(false);
    }
    
    pFragmentFn->release();
    pDesc->release();
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
    
    // argument buffer time
    MTL::ArgumentEncoder* pArgEncoder = _pVertexFn->newArgumentEncoder(BufferIndexVerticesAttributes);
    
    _pArgBuffer = _pDevice->newBuffer(pArgEncoder->encodedLength(), MTL::ResourceStorageModeManaged);
    
    pArgEncoder->setArgumentBuffer(_pArgBuffer, /*offset*/0);
    pArgEncoder->setBuffer(_pVertexBuffer, /*offset*/0, ArgumentBufferIDVertices);
    
    _pArgBuffer->didModifyRange(NS::Range::Make(0, _pArgBuffer->length()));
    
    pArgEncoder->release();
    _pVertexFn->release();
}

