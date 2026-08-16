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
    
    _semaphore = dispatch_semaphore_create(kMaxFramesInFlight);
    
    buildShaders();
    buildBuffers();
}


Renderer::~Renderer() {
    _pPipelineState->release();
    _pVertexBuffer->release();
    _pIndexBuffer->release();
    for (int i = 0; i < kMaxFramesInFlight; i++) {
        _pInstanceBuffer[i]->release();
    }
    
    _pCommandQueue->release();
    _pDevice->release();
}


void Renderer::draw(MTK::View* pView) {
    NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();
    
    // instance data managing
    dispatch_semaphore_wait(_semaphore, DISPATCH_TIME_FOREVER);
    _frame = (_frame + 1) % kMaxFramesInFlight;
    MTL::Buffer* pInstanceBuffer = _pInstanceBuffer[_frame];
    
    InstanceData* pInstanceData = reinterpret_cast<InstanceData*>(pInstanceBuffer->contents());
    float scl = 0.1f;
    for (size_t i = 0; i < kNumInstances; i++) {
        float t = i / float(kNumInstances);
        float xoff = (t*2.0f-1.0f) + (1.0f/kNumInstances);
        float yoff = sinf((t + _angle) * 2.0f * (float)M_PI);
        
        pInstanceData[i].instanceTransform = (simd::float4x4){
                    (simd::float4){ scl * sinf(_angle), scl * cosf(_angle), 0.f, 0.f },
                    (simd::float4){ scl * cosf(_angle), scl * -sinf(_angle), 0.f, 0.f },
                    (simd::float4){ 0.f, 0.f, scl, 0.f },
                    (simd::float4){ xoff, yoff, 0.f, 1.f }
                };
        float r = t, g = 1.0f - t, b = sinf((float)M_PI * 2.0f * t);
        pInstanceData[i].instanceColor = (simd::float4) {r, g, b, 1.0f};
    }
    pInstanceBuffer->didModifyRange(NS::Range::Make(0, pInstanceBuffer->length()));
    _angle += 1.0f;
    
    // create command buffer
    MTL::CommandBuffer* pCmd = _pCommandQueue->commandBuffer();
    
    // release the sephamore once GPU finishs frame
    Renderer* pRenderer = this;
    pCmd->addCompletedHandler(^void(MTL::CommandBuffer* pCmd) {
        dispatch_semaphore_signal(pRenderer->_semaphore);
    });
    
    MTL::RenderPassDescriptor* pRpd = pView->currentRenderPassDescriptor();
    MTL::RenderCommandEncoder* pEnc = pCmd->renderCommandEncoder(pRpd);
    
    // set render pipeline state for shaders
    pEnc->setRenderPipelineState(_pPipelineState);
    pEnc->setVertexBuffer(_pVertexBuffer, /*offset*/0, /*index*/BufferIndexVerticesAttributes);
    pEnc->setVertexBuffer(pInstanceBuffer, 0, BufferIndexInstanceAttributes);
    
    pEnc->drawIndexedPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle,
                                NS::UInteger(6),
                                MTL::IndexType::IndexTypeUInt16,
                                _pIndexBuffer,
                                NS::UInteger(0),
                                NS::UInteger(kNumInstances)
                                );
    
    
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
    const size_t numVertices = 4;
    
    VertexData vertices[numVertices] = {
        // Vertex 0: Top-Right
        {
            { 0.5f,  0.5f, 0.0f, 1.0f }, // Position
            { 1.0f,  0.0f, 0.0f, 1.0f }  // Color (Red)
        },
        // Vertex 1: Bottom-Right
        {
            { 0.5f, -0.5f, 0.0f, 1.0f }, // Position
            { 0.0f,  1.0f, 0.0f, 1.0f }  // Color (Green)
        },
        // Vertex 2: Bottom-Left
        {
            {-0.5f, -0.5f, 0.0f, 1.0f }, // Position
            { 0.0f,  0.0f, 1.0f, 1.0f }  // Color (Blue)
        },
        // Vertex 3: Top-Left
        {
            {-0.5f,  0.5f, 0.0f, 1.0f }, // Position
            { 1.0f,  1.0f, 0.0f, 1.0f }  // Color (Yellow)
        }
    };

    const size_t numIndices = 6;
    // Depending on your API (OpenGL, Vulkan, DirectX), this might be unsigned int, uint16_t, etc.
    uint16_t indices[numIndices] = {
        0, 1, 3,  // First Triangle (Top-Right, Bottom-Right, Top-Left)
        1, 2, 3   // Second Triangle (Bottom-Right, Bottom-Left, Top-Left)
    };
    
    
    // build vertex buffer
    const size_t bufferSize = sizeof(vertices);
    _pVertexBuffer = _pDevice->newBuffer(bufferSize, MTL::ResourceStorageModeManaged);
    memcpy(_pVertexBuffer->contents(), vertices, bufferSize);
    _pVertexBuffer->didModifyRange(NS::Range::Make(0, bufferSize));
    
    // build index buffer
    const size_t indexBufferSize = sizeof(indices);
    _pIndexBuffer = _pDevice->newBuffer(indexBufferSize, MTL::ResourceStorageModeManaged);
    memcpy(_pIndexBuffer->contents(), indices, indexBufferSize);
    _pIndexBuffer->didModifyRange(NS::Range::Make(0, indexBufferSize));
    
    // instance buffer stuff now (data for each instance)
    const size_t instanceBufferSize = kNumInstances * sizeof(InstanceData);
    for (int i = 0; i < kMaxFramesInFlight; i++) {
        _pInstanceBuffer[i] = _pDevice->newBuffer(instanceBufferSize, MTL::ResourceStorageModeManaged);
    }
}

