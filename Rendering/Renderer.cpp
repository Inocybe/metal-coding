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
    
    _semaphore = dispatch_semaphore_create(Renderer::kMaxFramesInFlight);
    
    buildShaders();
    buildBuffers();
    
    buildFrameData();
}


Renderer::~Renderer() {
    _pPipelineState->release();
    _pVertexBuffer->release();
    
    // release the tripple buffer
    for (int i = 0; i < kMaxFramesInFlight; i++) {
        _pFrameData[i]->release();
    }
    dispatch_release(_semaphore);
    
    _pCommandQueue->release();
    _pDevice->release();
}


void Renderer::draw(MTK::View* pView) {
    NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();
    
    // 1) wait for GPU to finish rendering slot
    dispatch_semaphore_wait(_semaphore, DISPATCH_TIME_FOREVER);
    // 2) cycle next frame buffer index
    _frame = (_frame + 1) % kMaxFramesInFlight;
    MTL::Buffer* pFrameDataBuffer = _pFrameData[_frame];
    // 3) update data on CPU
    FrameData* pData = reinterpret_cast<FrameData*>(pFrameDataBuffer->contents());
    pData->angle = (_angle += 0.01f);
    pFrameDataBuffer->didModifyRange(NS::Range::Make(0, sizeof(FrameData)));
    
    MTL::CommandBuffer* pCmd = _pCommandQueue->commandBuffer();
    
    // 4) register release side of semaphor
    Renderer* pRenderer = this;
    pCmd->addCompletedHandler(^void(MTL::CommandBuffer* pCmd) {
        dispatch_semaphore_signal(pRenderer->_semaphore);
    });
    
    
    MTL::RenderPassDescriptor* pRpd = pView->currentRenderPassDescriptor();
    MTL::RenderCommandEncoder* pEnc = pCmd->renderCommandEncoder(pRpd);
    
    
    
    
    // set render pipeline state for shaders
    pEnc->setRenderPipelineState(_pPipelineState);
    pEnc->setVertexBuffer(_pVertexBuffer, /*offset*/0, /*index*/BufferIndexVerticesAttributes);
    pEnc->setVertexBuffer(pFrameDataBuffer, 0, BufferIndexFrameData);
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

void Renderer::buildFrameData() {
    for (int i = 0; i < kMaxFramesInFlight; i++) {
        _pFrameData[i] = _pDevice->newBuffer(sizeof(FrameData), MTL::ResourceStorageModeManaged);
    }
}
