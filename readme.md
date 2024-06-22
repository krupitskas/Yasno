<div align="center">

# `🌩️ Yasno`

**Personal DirectX 12 research oriented renderer for quick iterations and easy usage**

</div>

<p align="center">
    <img src="docs/images/raster.png" style="width: 49%; height: 49%">
    <img src="docs/images/pathtracing.png" style="width: 49%; height: 49%">
</p>

## Current research goal idea

Yasno has two modes - raster and pathtracing for reference image.  
Reference pathtracing use RTX for tracing rays from the camera to the world. Raster right now not use any of RTX features (but will in the future like RTXGI, RTXAO)    
* Main goal to achieve non blurry, not ghosty nice sharp looking picture which is close to the reference pathtracing image. I don't really want to use any TAA or upscaling for this research, so I want to revisit older technques like SMAA which expect image converge via single frame, however - will see..
* Second goal is to have very efficiient culling (up to each triangle)  
* Third goal is to load UE5 matrix city demo and run it with this renderer (yeah, not soon)

### Current features

* as raster renderer mode
* RTX pathtracing renderer mode (materials WIP)
* GLTF format loading
* HDR Tonemapping (naive one, would be added more modes in the future like ACES and filmic)
* Bindless textures
* Single material, vertex, indices buffer
* Indirect draw (almost finished)

### Planned close next features

* RTX Pathtracing with multiframe accumulation
* Indirect frustum and occlusion cull via HZB
* RTXGI
* BRDF, BTDF and BSDF for different type materials to match pathtracing mode
* Utilize physical light units
* V Buffer
* Yasno architecture refactoring (right now it *very* messy)
* hare hot reloading (tracking for filesystem changes is there, but I want to make it right with include support)
* Finish naive techniques like
    * CSM
        * PCF
        * PCSS
    * OIT
    * Volumetric fog
    * SSLR, SSAO (with RTX fallback)
    * DOF
    * Color grading

### Planned long term features

* Apex and triangle culling
* Morph, skeletal animations
* Scene saving and loading (research OpenUSD)
* Texture streaming
* Async scene loading
