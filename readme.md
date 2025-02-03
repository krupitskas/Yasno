<div align="center">

# `🌩️ Yasno / Ясно 🌥️`

**DirectX 12 personal research renderer**

</div>

<p align="center">
    <img src="docs/images/raster.png" style="width: 49%; height: 49%">
    <img src="docs/images/pathtracing.png" style="width: 49%; height: 49%">
</p>

## Research Ideas

Yasno has two modes - raster and rtx pathtracing to have a reference image.  
 
Reference pathtracing fully utilize RTX. Raster right now not use any of RTX features (but will in the future)    
* Main goal to achieve believably raster image which match pathtracing close much as possible
* Second goal is to have very efficient raster pipeline, that mean culling, meshlets, indirect workflow
* Third goal (bonus!) is to load UE5 matrix city demo and run it with this renderer (yeah, ambitious)

### Current Features

* GLTF format loading
* Forward raster direct and indirect
* RTX pathtracing with temporal accumulation
* Tonemapping
* Bindless textures
* Single packed material, vertex, indices buffer
* Shaders hot reloading
* CSM with PCF
* Imgui and Imguizmo

### WIP Things

* Improve BRDF
* ReSTIR
* Improve dx12 arcitecture
* Culling
