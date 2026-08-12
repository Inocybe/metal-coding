//
//  Renderer.cpp
//  learning-metal
//
//  Created by Lev Mitchell on 8/11/26.
//
#include "Renderer.hpp"
#include <iostream>
#include <simd/simd.h>

static const char* shaderSrc = R"(
    #include <metal_stdlib>
    using namespace metal;
    struct v2f
    {
        float4 position [[position]];
        half3 color;
    };
    v2f vertex vertexMain( uint vertexId [[vertex_id]],
                           device const float3* positions [[buffer(0)]],
                           device const float3* colors [[buffer(1)]] )
    {
        v2f o;
        o.position = float4( positions[ vertexId ], 1.0 );
        o.color = half3( colors[ vertexId ] );
        return o;
    }
    half4 fragment fragmentMain( v2f in [[stage_in]] )
    {
        return half4( in.color, 1.0 );
    }
)";



Renderer::Renderer(MTL::Device* pDevice) : _pDevice(pDevice->retain()) {
    _pCommandQueue = _pDevice->newCommandQueue();
    buildShaders();
    buildBuffers();
}


Renderer::~Renderer() {
    _pPipelineState->release();
    _pVertexPositionsBuffer->release();
    _pVertexColorsBuffer->release();
    
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
    pEnc->setVertexBuffer(_pVertexPositionsBuffer, /*offset*/0, /*index*/0);
    pEnc->setVertexBuffer(_pVertexColorsBuffer, 0, 1);
    pEnc->drawPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
    
    pEnc->endEncoding();
    pCmd->presentDrawable(pView->currentDrawable());
    pCmd->commit();
    
    pPool->release();
}

void Renderer::buildShaders() {
    NS::Error* pError = nullptr;
    
    MTL::Library* pLibrary = _pDevice->newLibrary(NS::String::string(shaderSrc, NS::UTF8StringEncoding), nullptr, &pError);
    
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
    using simd::float3;
    
    const size_t numVertices = 3;
    float3 positions[numVertices] = {
        {0.0,  0.5, 0.0},
        {-0.5, -0.5, 0.0},
        {0.5, -0.5, 0.0}
    };
    float3 colors[numVertices] = {
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, 0.0, 1.0}
    };
    
    const size_t positionsSize = sizeof(positions);
    const size_t colorsSize = sizeof(colors);
    
    _pVertexPositionsBuffer = _pDevice->newBuffer(positionsSize, MTL::ResourceStorageModeManaged);
    _pVertexColorsBuffer = _pDevice->newBuffer(colorsSize, MTL::ResourceStorageModeManaged);
    
    memcpy(_pVertexPositionsBuffer->contents(), positions, positionsSize);
    memcpy(_pVertexColorsBuffer->contents(), colors, colorsSize);
    
    _pVertexPositionsBuffer->didModifyRange(NS::Range::Make(0, positionsSize));
    _pVertexColorsBuffer->didModifyRange(NS::Range::Make(0, colorsSize));
}

