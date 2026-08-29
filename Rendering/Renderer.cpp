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
#include "Math.hpp"

Renderer::Renderer(MTL::Device* pDevice) : _pDevice(pDevice->retain()) {
    _pCommandQueue = _pDevice->newCommandQueue();
    
    _semaphore = dispatch_semaphore_create(kMaxFramesInFlight);
    
    buildShaders();
    buildBuffers();
    buildDepthStencilStates();
    buildDepthTexture(800, 600);
}


Renderer::~Renderer() {
    _pPipelineState->release();
    _pVertexBuffer->release();
    _pIndexBuffer->release();
    for (int i = 0; i < kMaxFramesInFlight; i++) {
        _pInstanceBuffer[i]->release();
        _pCameraBuffer[i]->release();
    }
    
    _pDepthStencilState->release();
    _pDepthTexture->release();
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
    
    // update camera data
    MTL::Buffer* pCameraBuffer = _pCameraBuffer[_frame];
    CameraData* pCameraData = reinterpret_cast<CameraData*>(pCameraBuffer->contents());
    pCameraData->perspectiveTransform = math::makePerspective( 45.f * M_PI / 180.f, 1.f, 0.03f, 500.f );
    pCameraData->worldTransform = math::makeIdentity();
    pCameraBuffer->didModifyRange(NS::Range::Make(0, pCameraBuffer->length()));
    
    // create command buffer
    MTL::CommandBuffer* pCmd = _pCommandQueue->commandBuffer();
    
    // release the sephamore once GPU finishs frame
    Renderer* pRenderer = this;
    pCmd->addCompletedHandler(^void(MTL::CommandBuffer* pCmd) {
        dispatch_semaphore_signal(pRenderer->_semaphore);
    });
    
    
    MTL::RenderPassDescriptor* pRpd = pView->currentRenderPassDescriptor();

    
    // generate texutre for render pass at this frame
    MTL::RenderPassDepthAttachmentDescriptor* pDepthAttachment = pRpd->depthAttachment();
    pDepthAttachment->setTexture(_pDepthTexture);
    pDepthAttachment->setClearDepth(1.0f);
    pDepthAttachment->setLoadAction(MTL::LoadActionClear);
    pDepthAttachment->setStoreAction(MTL::StoreActionDontCare);
    
    
    MTL::RenderCommandEncoder* pEnc = pCmd->renderCommandEncoder(pRpd);

    
    pEnc->setDepthStencilState(_pDepthStencilState);
    pEnc->setCullMode(MTL::CullModeBack);
    pEnc->setFrontFacingWinding(MTL::Winding::WindingCounterClockwise);
    
    
    // set render pipeline state for shaders
    pEnc->setRenderPipelineState(_pPipelineState);
    pEnc->setVertexBuffer(_pVertexBuffer, /*offset*/0, /*index*/BufferIndexVerticesAttributes);
    pEnc->setVertexBuffer(pInstanceBuffer, 0, BufferIndexInstanceAttributes);
    pEnc->setVertexBuffer(pCameraBuffer, 0, BufferIndexCameraAttributes);
    
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
    pDesc->setDepthAttachmentPixelFormat(MTL::PixelFormat::PixelFormatDepth32Float);
    
    
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
    
    float s = 0.5f;
    VertexData verts[] = {
        { { -s, -s, +s, 1.0f} }, { { +s, -s, +s, 1.0f } }, { { +s, +s, +s, 1.0f } }, { { -s, +s, +s, 1.0f } },
        { { -s, -s, -s, 1.0f } }, { { +s, -s, -s, 1.0f } }, { { +s, +s, -s, 1.0f } }, { { -s, +s, -s, 1.0f } }
    };

    //const size_t numIndices = 6;
    uint16_t indices[] = {
        0,1,2, 2,3,0,   // front
        1,5,6, 6,2,1,   // right
        5,4,7, 7,6,5,   // back
        4,0,3, 3,7,4,   // left
        3,2,6, 6,7,3,   // top
        4,5,1, 1,0,4    // bottom
    };
    
    
    // build vertex buffer
    const size_t bufferSize = sizeof(verts);
    _pVertexBuffer = _pDevice->newBuffer(bufferSize, MTL::ResourceStorageModeManaged);
    memcpy(_pVertexBuffer->contents(), verts, bufferSize);
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
    // allocate the camera buffer
    for (int i = 0; i < kMaxFramesInFlight; i++) {
        _pCameraBuffer[i] = _pDevice->newBuffer(sizeof(CameraData), MTL::ResourceStorageModeManaged);
    }
}


void Renderer::buildDepthStencilStates() {
    MTL::DepthStencilDescriptor* pDsDesc = MTL::DepthStencilDescriptor::alloc()->init();
    pDsDesc->setDepthCompareFunction(MTL::CompareFunction::CompareFunctionLess);
    pDsDesc->setDepthWriteEnabled(true);
    _pDepthStencilState = _pDevice->newDepthStencilState(pDsDesc);
    pDsDesc->release();
}


void Renderer::buildDepthTexture(int width, int height) {
    MTL::TextureDescriptor* pDesc = MTL::TextureDescriptor::alloc()->init();
    pDesc->setPixelFormat(MTL::PixelFormat::PixelFormatDepth32Float);
    pDesc->setWidth(width);
    pDesc->setHeight(height);
    pDesc->setStorageMode(MTL::StorageMode::StorageModePrivate);
    pDesc->setUsage(MTL::TextureUsageRenderTarget);
    if ( _pDepthTexture ) _pDepthTexture->release();
    _pDepthTexture = _pDevice->newTexture(pDesc);
    pDesc->release();
}

