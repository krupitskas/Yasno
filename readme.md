<div align="center">

# `🌩️ Yasno / Ясно 🌥️`

**DirectX 12 research renderer**

</div>

<p align="center">
    <img src="docs/images/raster.png" style="width: 49%; height: 49%">
    <img src="docs/images/pathtracing.png" style="width: 49%; height: 49%">
</p>

## Research ideas

Yasno has two modes - raster and pathtracing to have reference image.  
 
Reference pathtracing heavily use RTX for tracing rays from the camera to the world. Raster right now not use any of RTX features (but will in the future like RTXGI, RTXAO)    
* Main goal to achieve believably raster image which match pathtracing as much as possible
* Second goal is to have very efficient raster pipeline, that mean culling, meshlets, indirect workflow
* Third goal (bonus!) is to load UE5 matrix city demo and run it with this renderer (yeah, not soon)

### Current features

* Raster direct and indirect
* RTX pathtracing renderer with multiframe accumulation
* GLTF format loading
* HDR Tonemapping (naive one, would be added more modes in the future like ACES and filmic)
* Bindless textures
* Single packed material, vertex, indices buffer

### Planned next features

* Indirect frustum and occlusion cull via HZB
* Yasno architecture refactoring (right now it *very* messy)
* BRDF, BTDF and BSDF for different type materials to match pathtracing mode
* Utilize physical light units
* V Buffer
* Shaders and C++ hot reloading
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
* Scene saving and loading
* Texture streaming
* Async scene loading

