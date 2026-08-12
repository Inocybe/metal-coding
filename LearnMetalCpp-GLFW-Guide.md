# Learning Metal with metal-cpp + GLFW

A personal, GLFW-based rebuild of Apple's `LearnMetalCpp` sample series. Same 11 stages, same
incremental "build on the last one" structure Apple uses — but every windowing/event-loop line
is GLFW instead of `NS::Application`/`MTK::View`, so what you learn here transfers directly to
your voxel engine and to Linux/Windows later if you ever go Vulkan.

**Read this before you start typing:**

- I have not compiled this on real hardware — I don't have macOS/Metal available where I'm
  writing this. Treat it as a strong, carefully-reasoned starting point, not a guarantee. You'll
  hit typos and the odd wrong parameter; that's normal, bring them back and I'll help debug.
- Apple's original samples are Apache 2.0 licensed, so adapting them like this is exactly what
  the license is for.
- Where I deviate from Apple's real approach, I say so explicitly and why. The three deviations
  that matter most, up front:
  1. **No `MTKView` means no free depth buffer.** Starting at Lesson 5 (3D), you have to create
     and manage your own depth texture. Apple's version never mentions this because `MTKView`
     does it silently.
  2. **Argument buffers (Lesson 2) are shown once, standalone.** Apple's own lesson text for
     Lessons 3+ shows plain `setVertexBuffer` calls, not argument-buffer indirection — so I
     don't thread that pattern through every later lesson either. You can layer it back in
     anywhere once you're comfortable with it.
  3. **Instancing (Lesson 4) is dropped starting at Lesson 6.** Lighting and texturing are
     easier to learn against one object. Re-adding instancing on top once you've got both is a
     good exercise.

---

## Project layout

Set it up as one folder per lesson, each a self-contained buildable program — same idea as
Apple's per-target Xcode project, and it means you can `diff -ru 04-instancing 05-perspective`
to see exactly what changed.

```
LearnMetalCpp-GLFW/
  metal-cpp/                  <- Apple's headers, download separately
  common/
    MetalGlue.mm               <- written once in Lesson 0, reused unchanged every lesson
  00-window/
    main.cpp  Renderer.hpp  Renderer.cpp
  01-primitive/
    ...
  02-argbuffers/
  03-animation/
  04-instancing/
  05-perspective/
    Math.hpp                   <- introduced this lesson
  06-lighting/
  07-texturing/
  08-compute/
  09-compute-to-render/
  10-frame-debugging/
    Info.plist
  Makefile
```

For each new lesson folder: copy the previous lesson's folder, then apply that lesson's section
below. `Renderer.hpp`/`Renderer.cpp`/the shader string are where nearly everything happens;
`main.cpp` barely changes after Lesson 0.

**Root Makefile** (builds every lesson into `build/<lesson-name>`):

```makefile
METALCPP = ./metal-cpp
FRAMEWORKS = -framework Metal -framework QuartzCore -framework Cocoa -framework Foundation
CXXFLAGS = -std=c++17 -fno-objc-arc -I $(METALCPP)
LDFLAGS = $(FRAMEWORKS) -lglfw -lobjc

LESSONS = 00-window 01-primitive 02-argbuffers 03-animation 04-instancing \
          05-perspective 06-lighting 07-texturing 08-compute \
          09-compute-to-render 10-frame-debugging

all: $(LESSONS)

$(LESSONS):
	mkdir -p build
	clang++ $(CXXFLAGS) $@/*.cpp $@/*.mm common/MetalGlue.mm -o build/$@ $(LDFLAGS)

clean:
	rm -rf build

.PHONY: all clean $(LESSONS)
```

(Lesson 10 needs one extra linker flag for its `Info.plist` — noted in that section.)

---

## Lesson 0 — Create a Window for Metal Rendering

You already built this one — it's the GLFW + `CAMetalLayer` rewrite from earlier: `main.cpp`
creates a GLFW window, `MetalGlue.mm` attaches a `CAMetalLayer` to it, `Renderer::draw()` clears
the layer to solid red each frame. Copy that into `00-window/` as your starting point; everything
below builds on it.

Quick recap of the four files, since every later lesson references them by name:

- **`common/MetalGlue.mm`** — the one Objective-C++ file. Bridges a `MTL::Device*` to a real
  `id<MTLDevice>` and attaches a `CAMetalLayer` to the GLFW window. Never changes again.
- **`main.cpp`** — `glfwCreateWindow`, call `attachMetalLayer`, construct `Renderer`, loop
  `glfwPollEvents()` + `renderer.draw(pLayer)`.
- **`Renderer.hpp`/`.cpp`** — owns the `MTL::Device`/`MTL::CommandQueue`, and every lesson from
  here adds new members and expands `draw()`.

---

## Lesson 1 — Render a Triangle

### New concepts
A **render pipeline** (compiled vertex + fragment shaders) and **buffers** (raw GPU memory you
fill from the CPU and bind to shader arguments).

### Files touched
`Renderer.hpp`, `Renderer.cpp`

Add to `Renderer.hpp`:

```cpp
public:
    void buildShaders();
    void buildBuffers();

private:
    MTL::RenderPipelineState* _pPSO;
    MTL::Buffer* _pVertexPositionsBuffer;
    MTL::Buffer* _pVertexColorsBuffer;
```

Call `buildShaders()` then `buildBuffers()` at the end of the `Renderer` constructor — shaders
first, since nothing else depends on the buffers yet but the pipeline needs to exist before you
ever try to bind anything to it.

**The shader source.** Metal Shading Language (MSL) is a C++14 dialect. Real projects put this in
a `.metal` file that Xcode compiles separately; to keep the Makefile simple, embed it as a raw
string at the top of `Renderer.cpp`:

```cpp
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
```

**Why two shaders:** the vertex shader runs once per vertex and decides *where* it goes
(`position`); the fragment shader runs once per covered pixel and decides *what color* it is.
`[[buffer(0)]]`/`[[buffer(1)]]` are the slot numbers you'll bind CPU-side buffers to — that
number is the actual contract between your C++ and your MSL, nothing else connects them.

```cpp
void Renderer::buildShaders()
{
    NS::Error* pError = nullptr;
    MTL::Library* pLibrary = _pDevice->newLibrary(
        NS::String::string( shaderSrc, NS::UTF8StringEncoding ), nullptr, &pError );
    if ( !pLibrary )
    {
        __builtin_printf( "%s", pError->localizedDescription()->utf8String() );
        assert( false );
    }

    MTL::Function* pVertexFn = pLibrary->newFunction(
        NS::String::string( "vertexMain", NS::UTF8StringEncoding ) );
    MTL::Function* pFragFn = pLibrary->newFunction(
        NS::String::string( "fragmentMain", NS::UTF8StringEncoding ) );

    MTL::RenderPipelineDescriptor* pDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    pDesc->setVertexFunction( pVertexFn );
    pDesc->setFragmentFunction( pFragFn );
    pDesc->colorAttachments()->object(0)->setPixelFormat( MTL::PixelFormat::PixelFormatBGRA8Unorm );

    _pPSO = _pDevice->newRenderPipelineState( pDesc, &pError );
    if ( !_pPSO )
    {
        __builtin_printf( "%s", pError->localizedDescription()->utf8String() );
        assert( false );
    }

    pVertexFn->release();
    pFragFn->release();
    pDesc->release();
    pLibrary->release();
}
```

**Why the pixel format must match:** `PixelFormatBGRA8Unorm` here has to be the exact same
format you set on the `CAMetalLayer` in `MetalGlue.mm`. If they don't match, pipeline creation
either fails outright or you get garbage color output — this is a very common first bug.

`MTL::RenderPipelineState` is expensive to build (it invokes an actual shader compiler). Build it
once at startup, never per-frame — that's why it lives in a constructor call, not in `draw()`.

```cpp
void Renderer::buildBuffers()
{
    using simd::float3;
    const size_t NumVertices = 3;

    float3 positions[NumVertices] =
    {
        { -0.8f,  0.8f, 0.0f },
        {  0.0f, -0.8f, 0.0f },
        { +0.8f,  0.8f, 0.0f }
    };
    float3 colors[NumVertices] =
    {
        { 1.0f, 0.3f, 0.2f },
        { 0.8f, 1.0f, 0.0f },
        { 0.8f, 0.0f, 1.0f }
    };

    const size_t positionsDataSize = NumVertices * sizeof( float3 );
    const size_t colorDataSize = NumVertices * sizeof( float3 );

    _pVertexPositionsBuffer = _pDevice->newBuffer( positionsDataSize, MTL::ResourceStorageModeManaged );
    _pVertexColorsBuffer = _pDevice->newBuffer( colorDataSize, MTL::ResourceStorageModeManaged );

    memcpy( _pVertexPositionsBuffer->contents(), positions, positionsDataSize );
    memcpy( _pVertexColorsBuffer->contents(), colors, colorDataSize );

    _pVertexPositionsBuffer->didModifyRange( NS::Range::Make( 0, _pVertexPositionsBuffer->length() ) );
    _pVertexColorsBuffer->didModifyRange( NS::Range::Make( 0, _pVertexColorsBuffer->length() ) );
}
```

**Why `didModifyRange()`:** `ResourceStorageModeManaged` means Metal keeps a CPU-visible copy and
a GPU-visible copy in sync itself, but only if you tell it when the CPU side changed. Skip this
call and the GPU may render stale (often zeroed) data — silent bug, not a crash.

Now wire it into `draw()` — insert between creating the encoder and `endEncoding()`:

```cpp
pEnc->setRenderPipelineState( _pPSO );
pEnc->setVertexBuffer( _pVertexPositionsBuffer, /*offset*/ 0, /*index*/ 0 );
pEnc->setVertexBuffer( _pVertexColorsBuffer, /*offset*/ 0, /*index*/ 1 );
pEnc->drawPrimitives( MTL::PrimitiveType::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3) );
```

The `index` argument here is exactly the `[[buffer(N)]]` slot from the shader — `setVertexBuffer`
at index 1 is what makes `colors` show up in `[[buffer(1)]]` on the GPU side. `drawPrimitives`
with `PrimitiveTypeTriangle` and 3 vertices is the actual "draw" command; everything before it
was just setup.

Add matching `release()` calls for `_pPSO`, `_pVertexPositionsBuffer`, `_pVertexColorsBuffer` in
the destructor — same pattern as `_pCommandQueue`/`_pDevice` from Lesson 0.

---

## Lesson 2 — Store Shader Arguments in a Buffer

### New concepts
**Argument buffers**: instead of binding several buffers directly, you bind one buffer that
itself *contains references* to other buffers/textures. Useful once you have dozens of resources
per draw call and don't want dozens of `setVertexBuffer` calls.

This lesson stands alone — apply it on top of Lesson 1, but don't carry it forward into Lesson 3;
we go back to direct binding there for clarity (see the note at the top of this guide).

### Files touched
`Renderer.hpp`, `Renderer.cpp`

Shader changes — replace the two separate `positions`/`colors` parameters with one indirect
struct:

```cpp
struct VertexData
{
    device float3* positions [[id(0)]];
    device float3* colors [[id(1)]];
};

v2f vertex vertexMain( device const VertexData* vertexData [[buffer(0)]],
                       uint vertexId [[vertex_id]] )
{
    v2f o;
    o.position = float4( vertexData->positions[ vertexId ], 1.0 );
    o.color = half3( vertexData->colors[ vertexId ] );
    return o;
}
```

`[[id(0)]]`/`[[id(1)]]` inside the struct are a *different* numbering system from `[[buffer(N)]]`
— they're slots within the argument buffer itself, not top-level binding points. Easy to confuse
with `[[buffer(N)]]` at a glance; they aren't the same thing.

`Renderer.hpp` needs to keep the library and add the new buffer:

```cpp
private:
    MTL::Library* _pShaderLibrary;   // now kept as a member — buildBuffers() needs it
    MTL::Buffer* _pArgBuffer;
```

Change `buildShaders()` to store `pLibrary` into `_pShaderLibrary` instead of releasing it
locally (release it in the destructor instead). Then, in `buildBuffers()`, after creating
`_pVertexPositionsBuffer`/`_pVertexColorsBuffer` as before, add:

```cpp
MTL::Function* pVertexFn = _pShaderLibrary->newFunction(
    NS::String::string( "vertexMain", NS::UTF8StringEncoding ) );
MTL::ArgumentEncoder* pArgEncoder = pVertexFn->newArgumentEncoder( 0 );

_pArgBuffer = _pDevice->newBuffer( pArgEncoder->encodedLength(), MTL::ResourceStorageModeManaged );

pArgEncoder->setArgumentBuffer( _pArgBuffer, 0 );
pArgEncoder->setBuffer( _pVertexPositionsBuffer, 0, 0 );
pArgEncoder->setBuffer( _pVertexColorsBuffer, 0, 1 );

_pArgBuffer->didModifyRange( NS::Range::Make( 0, _pArgBuffer->length() ) );

pVertexFn->release();
pArgEncoder->release();
```

**Why `newArgumentEncoder(0)`:** the encoder's job is to know the exact memory layout Metal
expects for argument index 0 of `vertexMain` (i.e. the `VertexData` struct) — you never hand-lay
that memory out yourself, you always ask Metal for an encoder that knows the shader's actual
compiled layout, then call `setBuffer`/`setTexture` on it like you're filling out a form.

In `draw()`, replace the two direct `setVertexBuffer` calls with:

```cpp
pEnc->setVertexBuffer( _pArgBuffer, 0, 0 );
pEnc->useResource( _pVertexPositionsBuffer, MTL::ResourceUsageRead );
pEnc->useResource( _pVertexColorsBuffer, MTL::ResourceUsageRead );
```

**Why `useResource()` is mandatory here:** normally Metal can see a buffer is in use because
you literally handed it to `setVertexBuffer`. Once a buffer is only referenced *indirectly*
(through the argument buffer), Metal has no way to know it needs to be resident — you have to
say so explicitly, or you'll get a GPU-side crash or garbage reads.

---

## Lesson 3 — Animate Rendering

### New concepts
Passing a small **uniform value** (same for every vertex) to a shader, and the **triple-buffering
+ semaphore** pattern you'll reuse for every animated value in every lesson after this one.

### Files touched
`Renderer.hpp`, `Renderer.cpp`

We're back on Lesson 1's direct-buffer layout (no argument buffer). Shader adds a `FrameData`
uniform and uses it to rotate:

```cpp
struct FrameData
{
    float angle;
};

v2f vertex vertexMain( uint vertexId [[vertex_id]],
                       device const float3* positions [[buffer(0)]],
                       device const float3* colors [[buffer(1)]],
                       constant FrameData* frameData [[buffer(2)]] )
{
    float a = frameData->angle;
    float3x3 rot( sin(a), cos(a), 0.0,
                  cos(a), -sin(a), 0.0,
                  0.0, 0.0, 1.0 );
    v2f o;
    o.position = float4( rot * positions[ vertexId ], 1.0 );
    o.color = half3( colors[ vertexId ] );
    return o;
}
```

**Declare `FrameData` twice, by hand — in C++ and in the MSL string.** Because we compile the
shader from an embedded string, there's no shared header the two languages both see; you're
responsible for keeping their layouts identical. (If you ever move shaders into real `.metal`
files compiled separately, you can share one header — worth doing once your project grows.)

`Renderer.hpp` additions:

```cpp
public:
    static const int kMaxFramesInFlight = 3;

private:
    void buildFrameData();

    struct FrameData { float angle; };   // C++-side mirror of the MSL struct above

    MTL::Buffer* _pFrameData[ kMaxFramesInFlight ];
    dispatch_semaphore_t _semaphore;
    float _angle = 0.f;
    int _frame = 0;
```

**Why three buffers instead of one:** the CPU writes next frame's angle while the GPU may *still
be reading* the previous frame's buffer (GPU work is asynchronous — `commit()` doesn't block).
Writing into the same buffer the GPU is currently reading is a race condition. Cycling through
three buffers gives the GPU a two-frame head start to finish before the CPU comes back around to
reuse that slot.

```cpp
void Renderer::buildFrameData()
{
    for ( int i = 0; i < kMaxFramesInFlight; ++i )
        _pFrameData[i] = _pDevice->newBuffer( sizeof(FrameData), MTL::ResourceStorageModeManaged );
}
```

Constructor: call `buildFrameData()`, and create the semaphore —

```cpp
_semaphore = dispatch_semaphore_create( Renderer::kMaxFramesInFlight );
```

**Why the semaphore, on top of triple-buffering:** triple-buffering alone assumes the GPU never
falls more than two frames behind. The semaphore makes that assumption *enforced* rather than
hoped-for — if the GPU is somehow still behind, the CPU actually blocks and waits instead of
silently corrupting a buffer it shouldn't be touching yet.

In `draw()`, right at the top, before building anything:

```cpp
dispatch_semaphore_wait( _semaphore, DISPATCH_TIME_FOREVER );

_frame = (_frame + 1) % kMaxFramesInFlight;
MTL::Buffer* pFrameDataBuffer = _pFrameData[ _frame ];

reinterpret_cast<FrameData*>( pFrameDataBuffer->contents() )->angle = (_angle += 0.01f);
pFrameDataBuffer->didModifyRange( NS::Range::Make( 0, sizeof(FrameData) ) );
```

Right after creating the command buffer, register the release side of the semaphore:

```cpp
MTL::CommandBuffer* pCmd = _pCommandQueue->commandBuffer();

Renderer* pRenderer = this;
pCmd->addCompletedHandler( ^void( MTL::CommandBuffer* ){
    dispatch_semaphore_signal( pRenderer->_semaphore );
});
```

**Why this specific pairing (`wait` at the top of `draw()`, `signal` in a completion handler):**
the wait/signal aren't adjacent lines — that's the point. `wait` blocks the *next* frame's CPU
work until *this* frame's GPU work genuinely finishes, which is exactly the guarantee you need
and exactly why it has to be a callback rather than something called synchronously.

Finally, bind the buffer and add its `release()`s in the destructor:

```cpp
pEnc->setVertexBuffer( pFrameDataBuffer, 0, 2 );
```

```cpp
// destructor
for ( int i = 0; i < kMaxFramesInFlight; ++i ) _pFrameData[i]->release();
dispatch_release( _semaphore );   // dispatch objects need their own manual release under -fno-objc-arc
```

---

## Lesson 4 — Draw Multiple Instances of an Object

### New concepts
Drawing the same geometry many times in **one draw call** instead of one call per object —
each instance gets its own transform/color from a per-instance buffer.

This lesson also **retires the `FrameData` buffer from Lesson 3**: from here on, animation gets
computed CPU-side directly into each instance's transform matrix, which is simpler once you have
per-object data anyway. The semaphore + triple-buffering *pattern* isn't going anywhere — you're
about to reuse it for instance data below.

### Files touched
`Renderer.hpp`, `Renderer.cpp`

We switch from a triangle to a small quad (four corners, two triangles) so 32 instances read as
a scattered field of shapes rather than an unreadable mess of overlapping triangles.

```cpp
struct VertexData { simd::float3 position; };
struct InstanceData { simd::float4x4 instanceTransform; simd::float4 instanceColor; };
```

Shader:

```cpp
struct VertexData { float3 position; };
struct InstanceData { float4x4 instanceTransform; float4 instanceColor; };

v2f vertex vertexMain( device const VertexData* vertexData [[buffer(0)]],
                       device const InstanceData* instanceData [[buffer(1)]],
                       uint vertexId [[vertex_id]],
                       uint instanceId [[instance_id]] )
{
    v2f o;
    float4 pos = float4( vertexData[ vertexId ].position, 1.0 );
    o.position = instanceData[ instanceId ].instanceTransform * pos;
    o.color = half3( instanceData[ instanceId ].instanceColor.rgb );
    return o;
}
```

**`[[instance_id]]`** is Metal counting which copy of the draw you're currently on — that's the
whole mechanism. You index into `instanceData` with it the same way `[[vertex_id]]` indexes into
per-vertex arrays.

Quad + index buffer, in `buildBuffers()`:

```cpp
const float s = 0.5f;
VertexData verts[] = {
    { { -s, -s, 0.f } }, { { +s, -s, 0.f } },
    { { +s, +s, 0.f } }, { { -s, +s, 0.f } }
};
uint16_t indices[] = { 0, 1, 2,  2, 3, 0 };

_pVertexDataBuffer = _pDevice->newBuffer( sizeof(verts), MTL::ResourceStorageModeManaged );
_pIndexBuffer = _pDevice->newBuffer( sizeof(indices), MTL::ResourceStorageModeManaged );

memcpy( _pVertexDataBuffer->contents(), verts, sizeof(verts) );
memcpy( _pIndexBuffer->contents(), indices, sizeof(indices) );
_pVertexDataBuffer->didModifyRange( NS::Range::Make(0, _pVertexDataBuffer->length()) );
_pIndexBuffer->didModifyRange( NS::Range::Make(0, _pIndexBuffer->length()) );
```

**Why an index buffer now, when Lesson 1's triangle didn't need one:** a quad's two triangles
share two corners. Without indices you'd duplicate those shared vertices in the vertex buffer;
with indices, four unique vertices plus six index values (`0,1,2,2,3,0`) describe both triangles.
Not required at this scale, but it's the pattern you'll want the moment geometry gets non-trivial
(the cube in Lesson 5 needs it for real).

Reuse the Lesson 3 triple-buffer pattern, this time for instances:

```cpp
static const int kNumInstances = 32;
MTL::Buffer* _pInstanceDataBuffer[ kMaxFramesInFlight ];   // built the same way _pFrameData was
```

In `draw()`, after the semaphore wait / frame-index cycle:

```cpp
MTL::Buffer* pInstanceDataBuffer = _pInstanceDataBuffer[ _frame ];
InstanceData* pInstanceData = reinterpret_cast<InstanceData*>( pInstanceDataBuffer->contents() );

float scl = 0.1f;
for ( size_t i = 0; i < kNumInstances; ++i )
{
    float t = i / (float)kNumInstances;
    float xoff = (t * 2.0f - 1.0f) + (1.f / kNumInstances);
    float yoff = sinf( (t + _angle) * 2.0f * M_PI );

    pInstanceData[i].instanceTransform = (simd::float4x4){
        (simd::float4){ scl * sinf(_angle), scl * cosf(_angle), 0.f, 0.f },
        (simd::float4){ scl * cosf(_angle), scl * -sinf(_angle), 0.f, 0.f },
        (simd::float4){ 0.f, 0.f, scl, 0.f },
        (simd::float4){ xoff, yoff, 0.f, 1.f }
    };

    float r = t, g = 1.0f - t, b = sinf( M_PI * 2.0f * t );
    pInstanceData[i].instanceColor = (simd::float4){ r, g, b, 1.0f };
}
pInstanceDataBuffer->didModifyRange( NS::Range::Make( 0, pInstanceDataBuffer->length() ) );
```

Bind and issue an *indexed, instanced* draw:

```cpp
pEnc->setVertexBuffer( _pVertexDataBuffer, 0, 0 );
pEnc->setVertexBuffer( pInstanceDataBuffer, 0, 1 );

pEnc->drawIndexedPrimitives( MTL::PrimitiveType::PrimitiveTypeTriangle,
                             6, MTL::IndexType::IndexTypeUInt16,
                             _pIndexBuffer, 0,
                             kNumInstances );
```

**Why this is one draw call, not 32:** `drawIndexedPrimitives`'s last argument is the instance
count. Metal runs the whole vertex/fragment pipeline 32 times internally, but you pay the
CPU-side draw-call overhead exactly once — that overhead, not the GPU work itself, is usually
the actual bottleneck when a scene has thousands of small objects.

---

## Lesson 5 — Render 3D with Perspective Projection

### New concepts
A **perspective matrix** (fakes a camera lens), and — because we ditched `MTKView` — **your own
depth texture**, which Apple's version gets silently from `MTKView.depthStencilPixelFormat`.

### Files touched
`Renderer.hpp`, `Renderer.cpp`, new `Math.hpp`

`Math.hpp` — small, self-contained, used from here on:

```cpp
#pragma once
#include <simd/simd.h>
#include <math.h>

namespace math
{
    inline simd::float4x4 makeIdentity()
    {
        using simd::float4;
        return simd::float4x4( (float4){1,0,0,0}, (float4){0,1,0,0},
                                (float4){0,0,1,0}, (float4){0,0,0,1} );
    }

    inline simd::float4x4 makePerspective( float fovRadians, float aspect, float znear, float zfar )
    {
        using simd::float4;
        float ys = 1.f / tanf( fovRadians * 0.5f );
        float xs = ys / aspect;
        float zs = zfar / ( znear - zfar );
        return simd::float4x4( (float4){ xs, 0, 0, 0 },
                                (float4){ 0, ys, 0, 0 },
                                (float4){ 0, 0, zs, -1 },
                                (float4){ 0, 0, znear * zs, 0 } );
    }
}
```

Geometry switches from the flat quad to a cube (8 shared vertices, 36 indices — no normals yet,
those come in Lesson 6):

```cpp
const float s = 0.5f;
VertexData verts[] = {
    { { -s, -s, +s } }, { { +s, -s, +s } }, { { +s, +s, +s } }, { { -s, +s, +s } },
    { { -s, -s, -s } }, { { +s, -s, -s } }, { { +s, +s, -s } }, { { -s, +s, -s } }
};
uint16_t indices[] = {
    0,1,2, 2,3,0,   // front
    1,5,6, 6,2,1,   // right
    5,4,7, 7,6,5,   // back
    4,0,3, 3,7,4,   // left
    3,2,6, 6,7,3,   // top
    4,5,1, 1,0,4    // bottom
};
```

**Depth texture, the part `MTKView` was hiding from us:**

```cpp
// Renderer.hpp
void buildDepthTexture( int width, int height );
MTL::DepthStencilState* _pDepthStencilState;
MTL::Texture* _pDepthTexture;
```

```cpp
void Renderer::buildDepthStencilStates()
{
    MTL::DepthStencilDescriptor* pDsDesc = MTL::DepthStencilDescriptor::alloc()->init();
    pDsDesc->setDepthCompareFunction( MTL::CompareFunction::CompareFunctionLess );
    pDsDesc->setDepthWriteEnabled( true );
    _pDepthStencilState = _pDevice->newDepthStencilState( pDsDesc );
    pDsDesc->release();
}

void Renderer::buildDepthTexture( int width, int height )
{
    MTL::TextureDescriptor* pDesc = MTL::TextureDescriptor::alloc()->init();
    pDesc->setPixelFormat( MTL::PixelFormat::PixelFormatDepth32Float );
    pDesc->setWidth( width );
    pDesc->setHeight( height );
    pDesc->setStorageMode( MTL::StorageModePrivate );
    pDesc->setUsage( MTL::TextureUsageRenderTarget );

    if ( _pDepthTexture ) _pDepthTexture->release();
    _pDepthTexture = _pDevice->newTexture( pDesc );
    pDesc->release();
}
```

Call `buildDepthStencilStates()` and `buildDepthTexture(512, 512)` from the constructor (matching
your window's initial size — if you ever add resize handling, call `buildDepthTexture` again with
the new size, same as the "what this doesn't handle yet" note from Lesson 0).

**Why depth testing needs its own texture at all:** without one, Metal has nowhere to record "how
close is the nearest thing drawn to this pixel so far," so triangles just paint over each other
in draw order — the back face of a cube can end up on top of the front face. The depth texture is
what lets `CompareFunctionLess` actually reject fragments that are behind something already drawn.

Wire the depth format into the pipeline (`buildShaders()`):

```cpp
pDesc->setDepthAttachmentPixelFormat( MTL::PixelFormat::PixelFormatDepth32Float );
```

And into the render pass descriptor and encoder state in `draw()`:

```cpp
MTL::RenderPassDepthAttachmentDescriptor* pDepthAttachment = pRpd->depthAttachment();
pDepthAttachment->setTexture( _pDepthTexture );
pDepthAttachment->setClearDepth( 1.0 );
pDepthAttachment->setLoadAction( MTL::LoadActionClear );
pDepthAttachment->setStoreAction( MTL::StoreActionDontCare );

// after creating pEnc:
pEnc->setDepthStencilState( _pDepthStencilState );
pEnc->setCullMode( MTL::CullModeBack );
pEnc->setFrontFacingWinding( MTL::Winding::WindingCounterClockwise );
```

`setCullMode(Back)` skips rasterizing triangles facing away from the camera entirely — a cube has
6 faces but you can only ever see 3 at once, so this alone roughly halves the fragment work.

Camera data, triple-buffered exactly like instance data was:

```cpp
struct CameraData { simd::float4x4 perspectiveTransform; simd::float4x4 worldTransform; };
MTL::Buffer* _pCameraDataBuffer[ kMaxFramesInFlight ];
```

```cpp
MTL::Buffer* pCameraDataBuffer = _pCameraDataBuffer[ _frame ];
CameraData* pCameraData = reinterpret_cast<CameraData*>( pCameraDataBuffer->contents() );
pCameraData->perspectiveTransform = math::makePerspective( 45.f * M_PI / 180.f, 1.f, 0.03f, 500.f );
pCameraData->worldTransform = math::makeIdentity();
pCameraDataBuffer->didModifyRange( NS::Range::Make(0, sizeof(CameraData)) );

pEnc->setVertexBuffer( _pVertexDataBuffer, 0, 0 );
pEnc->setVertexBuffer( pInstanceDataBuffer, 0, 1 );
pEnc->setVertexBuffer( pCameraDataBuffer, 0, 2 );
```

Shader:

```cpp
struct CameraData { float4x4 perspectiveTransform; float4x4 worldTransform; };

v2f vertex vertexMain( device const VertexData* vertexData [[buffer(0)]],
                       device const InstanceData* instanceData [[buffer(1)]],
                       device const CameraData& cameraData [[buffer(2)]],
                       uint vertexId [[vertex_id]],
                       uint instanceId [[instance_id]] )
{
    float4 pos = float4( vertexData[ vertexId ].position, 1.0 );
    pos = instanceData[ instanceId ].instanceTransform * pos;
    pos = cameraData.perspectiveTransform * cameraData.worldTransform * pos;

    v2f o;
    o.position = pos;
    o.color = half3( instanceData[ instanceId ].instanceColor.rgb );
    return o;
}
```

---

## Lesson 6 — Light Geometry

### New concepts
A **normal** per vertex (which way a surface faces), and a basic Lambert lighting formula in the
fragment shader.

To keep this lesson's new idea from tangling with Lesson 4's instancing math, we simplify back to
**one cube**, driven only by `cameraData.worldTransform`. Re-adding instancing on top later — by
feeding each instance's rotation into the normal as well — is a solid follow-up exercise once
this makes sense on its own.

### Files touched
`Renderer.hpp`, `Renderer.cpp`, `Math.hpp`

Add a Y-axis rotation helper to `Math.hpp`, so the cube visibly turns and the lighting has
something to play across:

```cpp
inline simd::float4x4 makeYRotate( float angleRadians )
{
    using simd::float4;
    float a = angleRadians;
    return simd::float4x4( (float4){ cosf(a), 0, sinf(a), 0 },
                            (float4){ 0, 1, 0, 0 },
                            (float4){ -sinf(a), 0, cosf(a), 0 },
                            (float4){ 0, 0, 0, 1 } );
}
```

Cube data expands to 24 vertices — one copy of each corner *per face* — because each face needs
its own flat normal, and a shared corner can't hold four different normal values at once:

```cpp
struct VertexData { simd::float3 position; simd::float3 normal; };

const float s = 0.5f;
VertexData verts[] = {
    //     position                normal
    { { -s, -s, +s }, {  0,  0,  1 } }, { { +s, -s, +s }, {  0,  0,  1 } },
    { { +s, +s, +s }, {  0,  0,  1 } }, { { -s, +s, +s }, {  0,  0,  1 } },

    { { +s, -s, +s }, {  1,  0,  0 } }, { { +s, -s, -s }, {  1,  0,  0 } },
    { { +s, +s, -s }, {  1,  0,  0 } }, { { +s, +s, +s }, {  1,  0,  0 } },

    { { +s, -s, -s }, {  0,  0, -1 } }, { { -s, -s, -s }, {  0,  0, -1 } },
    { { -s, +s, -s }, {  0,  0, -1 } }, { { +s, +s, -s }, {  0,  0, -1 } },

    { { -s, -s, -s }, { -1,  0,  0 } }, { { -s, -s, +s }, { -1,  0,  0 } },
    { { -s, +s, +s }, { -1,  0,  0 } }, { { -s, +s, -s }, { -1,  0,  0 } },

    { { -s, +s, +s }, {  0,  1,  0 } }, { { +s, +s, +s }, {  0,  1,  0 } },
    { { +s, +s, -s }, {  0,  1,  0 } }, { { -s, +s, -s }, {  0,  1,  0 } },

    { { -s, -s, -s }, {  0, -1,  0 } }, { { +s, -s, -s }, {  0, -1,  0 } },
    { { +s, -s, +s }, {  0, -1,  0 } }, { { -s, -s, +s }, {  0, -1,  0 } }
};

uint16_t indices[] = {
     0, 1, 2,  2, 3, 0,      4, 5, 6,  6, 7, 4,
     8, 9,10, 10,11, 8,     12,13,14, 14,15,12,
    16,17,18, 18,19,16,     20,21,22, 22,23,20
};
```

Shader — `v2f` gains a `normal`, the vertex shader passes it through untransformed (fine for a
uniformly-scaled cube; a non-uniform scale would need the inverse-transpose of the model matrix,
worth knowing but not needed here), and the fragment shader does the actual lighting math:

```cpp
struct v2f { float4 position [[position]]; float3 normal; half3 color; };

v2f vertex vertexMain( device const VertexData* vertexData [[buffer(0)]],
                       device const CameraData& cameraData [[buffer(1)]],
                       uint vertexId [[vertex_id]] )
{
    float4 pos = float4( vertexData[ vertexId ].position, 1.0 );
    v2f o;
    o.position = cameraData.perspectiveTransform * cameraData.worldTransform * pos;
    o.normal = vertexData[ vertexId ].normal;
    o.color = half3( 0.9, 0.4, 0.2 );   // placeholder base color — swap for whatever you like
    return o;
}

half4 fragment fragmentMain( v2f in [[stage_in]] )
{
    float3 l = normalize( float3( 1.0, 1.0, 0.8 ) );   // fixed light direction: front-top-right
    float3 n = normalize( in.normal );
    float ndotl = saturate( dot( n, l ) );
    return half4( in.color * 0.1 + in.color * ndotl, 1.0 );
}
```

**Reading the lighting formula:** `dot(n, l)` is largest (1.0) when a face points straight at the
light and 0 when it's perpendicular to it — `saturate()` just clamps out the negative values you'd
get from faces pointing away. `in.color * 0.1` is a flat ambient term so faces pointing away from
the light aren't pure black; `in.color * ndotl` is the actual directional lighting on top.

`draw()` now animates `worldTransform` instead of feeding an angle into the shader directly —
this retires the last piece of Lesson 3's GPU-side rotation approach, which is genuinely no
longer needed now that you have a proper world matrix to put rotation into:

```cpp
pCameraData->worldTransform = math::makeYRotate( _angle );
```

(`_angle` is still incremented the same way it has been since Lesson 3 — only *where* it gets
used has moved.)

---

## Lesson 7 — Texture Surfaces

### New concepts
Loading image data onto the GPU as a **texture**, and sampling it in the fragment shader.

### Files touched
`Renderer.hpp`, `Renderer.cpp`

`VertexData` gains a texture coordinate; extend the Lesson 6 array (one `{u, v}` pair per line,
same 24 entries):

```cpp
struct VertexData { simd::float3 position; simd::float3 normal; simd::float2 texcoord; };

VertexData verts[] = {
    { { -s, -s, +s }, {  0,  0,  1 }, { 0, 1 } }, { { +s, -s, +s }, {  0,  0,  1 }, { 1, 1 } },
    { { +s, +s, +s }, {  0,  0,  1 }, { 1, 0 } }, { { -s, +s, +s }, {  0,  0,  1 }, { 0, 0 } },

    { { +s, -s, +s }, {  1,  0,  0 }, { 0, 1 } }, { { +s, -s, -s }, {  1,  0,  0 }, { 1, 1 } },
    { { +s, +s, -s }, {  1,  0,  0 }, { 1, 0 } }, { { +s, +s, +s }, {  1,  0,  0 }, { 0, 0 } },

    { { +s, -s, -s }, {  0,  0, -1 }, { 0, 1 } }, { { -s, -s, -s }, {  0,  0, -1 }, { 1, 1 } },
    { { -s, +s, -s }, {  0,  0, -1 }, { 1, 0 } }, { { +s, +s, -s }, {  0,  0, -1 }, { 0, 0 } },

    { { -s, -s, -s }, { -1,  0,  0 }, { 0, 1 } }, { { -s, -s, +s }, { -1,  0,  0 }, { 1, 1 } },
    { { -s, +s, +s }, { -1,  0,  0 }, { 1, 0 } }, { { -s, +s, -s }, { -1,  0,  0 }, { 0, 0 } },

    { { -s, +s, +s }, {  0,  1,  0 }, { 0, 1 } }, { { +s, +s, +s }, {  0,  1,  0 }, { 1, 1 } },
    { { +s, +s, -s }, {  0,  1,  0 }, { 1, 0 } }, { { -s, +s, -s }, {  0,  1,  0 }, { 0, 0 } },

    { { -s, -s, -s }, {  0, -1,  0 }, { 0, 1 } }, { { +s, -s, -s }, {  0, -1,  0 }, { 1, 1 } },
    { { +s, -s, +s }, {  0, -1,  0 }, { 1, 0 } }, { { -s, -s, +s }, {  0, -1,  0 }, { 0, 0 } }
};
```

Building the texture — a generated checkerboard, since Metal has no built-in image loader
(that's normally MetalKit's or Image I/O's job; a real one from a file is future work you can add
once this is working):

```cpp
// Renderer.hpp
void buildTextures();
MTL::Texture* _pTexture;
```

```cpp
void Renderer::buildTextures()
{
    const uint32_t tw = 128, th = 128;

    MTL::TextureDescriptor* pDesc = MTL::TextureDescriptor::alloc()->init();
    pDesc->setWidth( tw );
    pDesc->setHeight( th );
    pDesc->setPixelFormat( MTL::PixelFormatRGBA8Unorm );
    pDesc->setTextureType( MTL::TextureType2D );
    pDesc->setStorageMode( MTL::StorageModeManaged );
    pDesc->setUsage( MTL::ResourceUsageSample | MTL::ResourceUsageRead );

    _pTexture = _pDevice->newTexture( pDesc );

    uint8_t* pTextureData = (uint8_t*)alloca( tw * th * 4 );
    for ( uint32_t y = 0; y < th; ++y )
    for ( uint32_t x = 0; x < tw; ++x )
    {
        bool isWhite = (x ^ y) & 0b1000;
        uint8_t c = isWhite ? 0xFF : 0xA0;
        size_t i = (y * tw + x) * 4;
        pTextureData[i+0] = c; pTextureData[i+1] = c; pTextureData[i+2] = c; pTextureData[i+3] = 0xFF;
    }
    _pTexture->replaceRegion( MTL::Region( 0, 0, 0, tw, th, 1 ), 0, pTextureData, tw * 4 );

    pDesc->release();
}
```

Call `buildTextures()` from the constructor. Bind it in `draw()`:

```cpp
pEnc->setFragmentTexture( _pTexture, /*index*/ 0 );
```

Shader — `v2f` and `VertexData` both gain `texcoord`, and the fragment shader samples the texture
and mixes it into the existing lighting math:

```cpp
struct v2f { float4 position [[position]]; float3 normal; half3 color; float2 texcoord; };

// in vertexMain: o.texcoord = vertexData[ vertexId ].texcoord;

half4 fragment fragmentMain( v2f in [[stage_in]], texture2d<half, access::sample> tex [[texture(0)]] )
{
    constexpr sampler s( address::repeat, filter::linear );
    half3 texel = tex.sample( s, in.texcoord ).rgb;

    float3 l = normalize( float3( 1.0, 1.0, 0.8 ) );
    float3 n = normalize( in.normal );
    float ndotl = saturate( dot( n, l ) );

    half3 litColor = in.color * texel;
    return half4( litColor * 0.1 + litColor * ndotl, 1.0 );
}
```

`texture2d<half, access::sample>` and `[[texture(0)]]` are the fragment-shader equivalent of
`device const T* [[buffer(N)]]` — a separate binding namespace from buffers, which is why texture
index 0 and buffer index 0 don't collide.

---

## Lesson 8 — Use the GPU for General Purpose Computation

### New concepts
A **compute kernel** — a shader that isn't tied to triangles or pixels at all, just runs once per
"thread" over an arbitrary grid, here used to generate the checkerboard texture's replacement on
the GPU instead of the CPU.

### Files touched
`Renderer.hpp`, `Renderer.cpp`

Add a compute kernel to the shader string. Apple's own lesson text doesn't give the actual
fractal math, just the function signature — this is a reasonable, workable implementation, not
verbatim Apple source:

```cpp
kernel void mandelbrot_set( texture2d<half, access::write> tex [[texture(0)]],
                            uint2 index [[thread_position_in_grid]],
                            uint2 gridSize [[threads_per_grid]] )
{
    float2 uv = float2(index) / float2(gridSize);
    float2 c = (uv - 0.5) * 3.0 - float2( 0.4, 0.0 );
    float2 z = float2( 0.0 );

    float t = 0.0;
    for ( int i = 0; i < 256; ++i )
    {
        z = float2( z.x*z.x - z.y*z.y, 2.0*z.x*z.y ) + c;
        if ( dot(z, z) > 4.0 ) { t = float(i) / 256.0; break; }
    }
    tex.write( half4( t, t, t, 1.0 ), index, 0 );
}
```

**Why compute kernels don't write via a render pass:** there's no rasterizer, no fragments, no
render target here — a compute kernel just gets a grid position (`[[thread_position_in_grid]]`)
and writes wherever it wants with `tex.write()`. That's the whole distinction from a fragment
shader.

Building the compute pipeline is simpler than a render pipeline — one function, no attachments,
no vertex descriptor:

```cpp
// Renderer.hpp
void buildComputePipeline();
void generateMandelbrotTexture( MTL::CommandBuffer* pCmd );
MTL::ComputePipelineState* _pComputePSO;
```

```cpp
void Renderer::buildComputePipeline()
{
    NS::Error* pError = nullptr;
    MTL::Function* pFn = _pShaderLibrary->newFunction(
        NS::String::string( "mandelbrot_set", NS::UTF8StringEncoding ) );
    _pComputePSO = _pDevice->newComputePipelineState( pFn, &pError );
    if ( !_pComputePSO )
    {
        __builtin_printf( "%s", pError->localizedDescription()->utf8String() );
        assert( false );
    }
    pFn->release();
}
```

Note this needs `_pShaderLibrary` kept around as a member (same as Lesson 2 needed it) — store it
in `buildShaders()` instead of releasing it locally, if you haven't already.

**One texture-descriptor change:** the compute kernel *writes* the texture, so `buildTextures()`
needs `MTL::ResourceUsageWrite` added to its usage flags, alongside the existing
`Sample | Read`. Miss this and the compute dispatch will fail validation.

```cpp
void Renderer::generateMandelbrotTexture( MTL::CommandBuffer* pCmd )
{
    MTL::ComputeCommandEncoder* pEnc = pCmd->computeCommandEncoder();
    pEnc->setComputePipelineState( _pComputePSO );
    pEnc->setTexture( _pTexture, 0 );

    MTL::Size gridSize( 128, 128, 1 );
    NS::UInteger tgSize = _pComputePSO->maxTotalThreadsPerThreadgroup();
    pEnc->dispatchThreads( gridSize, MTL::Size( tgSize, 1, 1 ) );

    pEnc->endEncoding();
}
```

**Why `maxTotalThreadsPerThreadgroup()` instead of a hardcoded threadgroup size:** that number is
hardware-dependent — different GPUs support different maximum threadgroup sizes, so asking the
pipeline state for its own limit is the portable way to pick one, rather than guessing a constant
that might be invalid on some device.

For this lesson, generate the texture once at startup instead of every frame — from the
constructor, after `buildTextures()` and `buildComputePipeline()`:

```cpp
MTL::CommandBuffer* pCmd = _pCommandQueue->commandBuffer();
generateMandelbrotTexture( pCmd );
pCmd->commit();
pCmd->waitUntilCompleted();
```

`waitUntilCompleted()` here is fine — it's one-time startup work, not something you'd ever want
in the per-frame `draw()` path (that's exactly the block-the-CPU problem the Lesson 3 semaphore
exists to avoid).

---

## Lesson 9 — Mix Compute with Rendering

### New concepts
Running the same compute kernel **every frame**, on the same command buffer as the render pass
that uses its output — and relying on Metal's automatic hazard tracking instead of hand-written
synchronization.

### Files touched
`Renderer.cpp`

Move the call from the constructor into `draw()`, before the render pass is created, on the same
command buffer:

```cpp
MTL::CommandBuffer* pCmd = _pCommandQueue->commandBuffer();
// ... semaphore completion handler, as in Lesson 3 ...

generateMandelbrotTexture( pCmd );

MTL::RenderPassDescriptor* pRpd = MTL::RenderPassDescriptor::alloc()->init();
// ... rest of the render pass exactly as before ...
```

**Why nothing else needs to change:** Metal tracks which resources each encoder reads and writes
within a single command buffer. It sees the compute pass *write* `_pTexture` and the render pass
*read* it right after, and automatically inserts whatever GPU-side wait is needed between them —
you get correct ordering for free just by encoding compute-then-render into the same buffer, in
that order. This is genuinely one of the few places Metal does synchronization for you rather
than making you say so explicitly (contrast with `useResource()` back in Lesson 2, which is the
opposite situation).

As a small bonus tying back to `_angle`, feed a zoom factor into the kernel so the fractal
visibly evolves — add a one-float buffer argument to `mandelbrot_set` (`constant float& zoom
[[buffer(0)]]`, bound the same way `pEnc->setBuffer(...)` binds anything else on a compute
encoder) and use it to scale `c` in the loop above. Left as an exercise — you already have every
piece needed (a small uniform buffer, per-frame update, binding by index) from Lesson 3 onward.

---

## Lesson 10 — Capture GPU Commands for Debugging

### New concepts
**GPU frame capture** — a recording of every Metal command in a frame that you can open and step
through visually. Apple triggers this from a menu item; since we have no AppKit menu, we trigger
it from a keypress instead — a straightforward GLFW substitution for the same idea.

### Files touched
`Renderer.hpp`, `Renderer.cpp`, `main.cpp`, new `Info.plist`

```cpp
// Renderer.hpp
void triggerCapture();
private:
    bool _capturing = false;
    int _captureFrameCount = 0;
```

```cpp
void Renderer::triggerCapture()
{
    MTL::CaptureManager* pCaptureManager = MTL::CaptureManager::sharedCaptureManager();
    if ( !pCaptureManager->supportsDestination( MTL::CaptureDestinationGPUTraceDocument ) )
    {
        __builtin_printf( "GPU capture not supported\n" );
        return;
    }

    MTL::CaptureDescriptor* pDesc = MTL::CaptureDescriptor::alloc()->init();
    pDesc->setDestination( MTL::CaptureDestinationGPUTraceDocument );
    pDesc->setCaptureObject( _pDevice );
    pDesc->setOutputURL( NS::URL::fileURLWithPath(
        NS::String::string( "./capture.gputrace", NS::UTF8StringEncoding ) ) );

    NS::Error* pError = nullptr;
    if ( !pCaptureManager->startCapture( pDesc, &pError ) )
        __builtin_printf( "%s\n", pError->localizedDescription()->utf8String() );
    else
    {
        _capturing = true;
        _captureFrameCount = 0;
    }
    pDesc->release();
}
```

At the end of `draw()`, stop after a few frames so the trace stays small enough to actually open:

```cpp
if ( _capturing && ++_captureFrameCount >= 3 )
{
    MTL::CaptureManager::sharedCaptureManager()->stopCapture();
    _capturing = false;
}
```

Wire a key press to it in `main.cpp`:

```cpp
glfwSetWindowUserPointer( pWindow, &renderer );
glfwSetKeyCallback( pWindow, []( GLFWwindow* pWindow, int key, int, int action, int ){
    if ( key == GLFW_KEY_C && action == GLFW_PRESS )
        reinterpret_cast<Renderer*>( glfwGetWindowUserPointer(pWindow) )->triggerCapture();
});
```

`glfwGetWindowUserPointer`/`glfwSetWindowUserPointer` is GLFW's standard way to smuggle a `this`
pointer into a plain C callback — you'll use this same pattern for input handling in the voxel
engine.

**Enabling capture at all requires an `Info.plist`,** because a device only permits GPU capture
if it finds this key set. Apps built as a real `.app` bundle get this from Xcode automatically;
a bare command-line binary (what our Makefile produces) has to link it in manually.

`10-frame-debugging/Info.plist`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>MetalCaptureEnabled</key>
    <true/>
</dict>
</plist>
```

Linked in via one extra clang flag for this lesson only — add to its build rule:

```
-Xlinker -sectcreate -Xlinker __TEXT -Xlinker __info_plist -Xlinker 10-frame-debugging/Info.plist
```

Once captured, `capture.gputrace` opens directly in Xcode and persists on disk — you can inspect
it any time after the app has already quit, not just live.

---

## Where this leaves you

Eleven stages, each one buildable and diffable on its own, ending in: a textured, lit, animated
cube generated partly by a GPU compute kernel, debuggable with real Xcode GPU capture — all
driven by GLFW instead of AppKit. From here, natural next steps are loading real image files into
Lesson 7's texture (stb_image is a common, simple choice), adding your own input handling for a
camera, or pulling the shader strings out into real `.metal` files once the embedded-string
approach starts feeling cramped.
