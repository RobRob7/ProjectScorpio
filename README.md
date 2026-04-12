# Project Atlas
C++17 voxel-based rendering engine built for Windows featuring a modular, interchangeable backend architecture supporting Vulkan 1.4 (Dynamic Rendering) and OpenGL 4.6 Core. 

<h3>
Features:
</h3>

- Directional Shadow Mapping with Percentage-Closer Filtering (PCF)
- Physically-Inspired Surface Water Rendering
- Post-Processing Fog
- Screen-Space Ambient Occlusion (SSAO)
- Fast Approximate Anti-Aliasing (FXAA)
- Camera View Frustum Culling
- Greedy Meshing for Chunks
- Memory-Efficient Vertex Storage
- Procedural Terrain Generation using LibNoise
- Block Placement and Destruction
- World Data Persistence:
    - Auto-Saving World State
    - Manual Save System

## Controls
- WASD – Move
- Mouse – Free-look camera (when in camera mode)
- Left Click – Break block
- Right Click – Place block

#### Input Modes
> **Note:** The application starts in camera control mode by default.
- **`=`** — Enable camera control mode
  - Allows free-look camera movement (mouse + keyboard).
- **`-`** — Enable cursor mode
  - Releases the mouse cursor for interacting with the ImGui control panel.

<h2>
Preview
</h2>

![Shadow Mapping](./milestones/demo/shadow_maps.gif)

![Project Demo](./milestones/demo/demo4.gif)

<!--  -->
<h2>
Debugging & Validation
</h2>

- Used RenderDoc to capture and analyze GPU frames across both Vulkan and OpenGL backends.
- Utilized Vulkan shader debugging tools to inspect shader execution and validate intermediate outputs.
- Verified correctness of multi-pass rendering by analyzing the RenderDoc frame stack and inspecting intermediate render targets.
- Developed a custom in-engine debug system to visualize pass outputs (G-buffer, shadow maps, SSAO), enabling real-time validation outside of frame captures.

<h2>
Rendering & Engine Techniques
</h2>


> Multi-frame-in-flight architecture with per-frame resources (descriptor sets, UBOs) to avoid CPU-GPU synchronization hazards.

This project focuses on implementing real-time rendering techniques that are commonly used in modern game engines. Each technique was implemented from scratch with explicit control over GPU resources and pipeline state.

> **Note:** FPS comparisons were recorded at the same camera position. Relative gains are hardware-agnostic; absolute FPS varies by GPU.

### Rendering Pipeline Overview
> Vulkan backend utilizes dynamic rendering (no render passes/framebuffers) for flexible multi-pass composition.

1. G-buffer pass (normals + depth)
2. Shadow Map pass (shadow map depth)
3. Debug pass (visualize - gNormal, gDepth, shadow map depth)
4. SSAO pass + blur
5. Water reflection & refraction passes
6. Forward render
7. Post-Processing Fog
8. FXAA
9. UI Elements

<!--  -->
---
<h4>
Physically-Inspired Surface Water Rendering
</h4>

- Water is rendered using a dedicated pass that captures the scene above and below the water plane into reflection and refraction textures.
- Utilizes Fresnel-based reflection/refraction blending.
- DuDv mapping for wave distortion effects coupled with time-based animation.
- Refraction depth texture used to make shallow water appear lighter in color.

**Importance:**  
Previous versions of this engine showcased flat and boring water blocks. Water is now more visually appealing and takes into account other objects in the scene.

<!--  -->
---
<h4>
Post-Processing Fog
</h4>

- Screen-space, depth-based fog applied as a post-processing pass.
- Configurable fog color and start/end distances.
- Integrates seamlessly with SSAO, FXAA, and lighting passes.

**Importance:**  
Demonstrates the ability to implement additional post-processing effects that integrate cleanly into an existing post-processing pipeline.

<!--  -->
---
<h4>
Screen-Space Ambient Occlusion (SSAO)
</h4>

- Utilizes a G-buffer that stores view space normals and depth.
- Generates a random sample kernel in view space and projects samples into screen space.
- Calculates occlusion by comparing sampled depth values against the current fragment depth.
- A blur pass is applied to reduce high frequency noise while preserving edge detail.

**Importance:**  
SSAO adds depth perception and contact shadows without the cost of full global illumination, significantly improving visual realism in dense voxel environments.

<!--  -->
---
<h4>
Fast Approximate Anti-Aliasing (FXAA)
</h4>

- Implements FXAA 3.11 by Timothy Lottes  
  (based on https://gist.github.com/kosua20/0c506b81b3812ac900048059d2383126)
- A post-processing pass that operates on the final scene color buffer.
- Identifies and smooths jagged edges.

**Importance:**  
FXAA is a cost-efficient method for anti-aliasing with minimal performance cost that is ideal for voxel engines.

<!--  -->
---
<h4>
View Frustum Culling
</h4>

![Frustum](milestones/VisualCameraFrustum.png)

- Each chunk is tested against the camera's view frustum using Axis-Aligned Bounding Box (AABB) vs frustum plane checks. 
- Only the chunks visible from inside the frustum are rendered.
- Integrated directly into the chunk manager (CPU side) to avoid extra GPU load through draw calls.
- Noticeable performance increase from 679 FPS to 1057 FPS (~56% improvement) measured on an RTX 5090 at the same camera position.

**Importance:**  
Frustum culling drastically reduces GPU workload by efficiently rendering only the chunks visible in camera view, reducing overhead and increasing performance as the world grows in size.

<!--  -->
---
<h4>
Greedy Meshing
</h4>

- The chunk mesh is generated using greedy meshing, which scans each chunk in three passes (one per axis).
- For each pass, adjacent voxels are compared to detect visible faces. Adjacent faces that match in attributes are merged into larger quads.

| GPU            | Optimizations Off (FPS) | Optimizations On (FPS) | FPS Change | % Increase |
|----------------|---------------|--------------|------------|----------|
| Intel Arc A370M      | 39.9          | 73.8          | +33.9       | +85.0%   |
| AMD Radeon 780M       | 39.3           | 90.7          | +51.4       | +130.8%   |
| Nvidia RTX 4060m       | 103.0           | 210.1          | +107.1       | +104.0%   |
| Nvidia RTX 5090       | 342.2           | 662.0          | +319.8        | +93.5%   |

**Importance:**  
Greedy meshing reduces the number of draw calls and triangles sent to the GPU, improving overall rendering performance.

<!--  -->
---
<h4>
Memory-Efficient Vertex Storage
</h4>

![unoptimized](milestones/10_OPT_OFF.png)
![optimized](milestones/10_OPT_ON.png)

- Significantly reduced the per-vertex memory footprint for opaque geometry.
- Previously, each vertex stored position (vec3), normal (vec3), and UV (vec2) totaling 32 bytes. The optimized format packs this data into a single uint32_t (4 bytes).
- ~88% reduction in RAM usage: 1579 MB -> 185 MB.
- ~14% reduction in VRAM usage: 5282 MB -> 4526 MB.

> **Note:** VRAM reduction is smaller than RAM reduction due to textures and framebuffers dominating total GPU memory usage.

**Importance:**  
Smaller vertices reduce CPU memory pressure, improve cache efficiency, and allow significantly larger worlds and higher chunk counts without exhausting system memory.

<!--  -->
---
<h4>
Procedural Terrain Generation
</h4>

- Utilizes the LibNoise library to generate a terrain heightmap.
- This allows for varied terrain features such as hills, oceans, and trees.

**Importance:**  
Procedural generation allows for large, varied worlds without having to worry about doing so by hand, while maintaining a deterministic state.

<!--  -->
---
<h4>
World State Persistence System
</h4>

- Chunk data is serialized to disk using a custom save format.
- Supports both manual and automatic saving.
- As the player modifies (place/destroy blocks) the world, these changes persist through application shutdown and restart.

**Importance:**  
Persistent world state demonstrates data-oriented design beyond real-time rendering.

---
<!--  -->
<h4>
Directional Shadow Mapping
</h4>

- Implemented real-time directional shadow mapping using a light-space depth pass.
- Every frame the shadow map is re-calculated based on the camera's visible region.
- Applied Percentage-Closer Filtering (PCF) to produce softer shadows by sampling the surrounding texels of the depth map and averaging the results.

**Importance:**  
Adds depth cues and spatial realism while demonstrating understanding of multi-pass rendering, and real-time lighting techniques.

---
<!--  -->
<!--  -->
<h2>
Milestones
</h2>

| Terrain Generation + Skybox |
|---------|
| *Initial terrain generation using a simple heightmap.* |
| ![Alt Text 1](milestones/1_terraingen_cubemap.png)|

| Terrain Generation w/LibNoise |
|---------|
| *Terrain generation using LibNoise for more interesting views, trees are WIP.* |
| ![Alt Text 1](milestones/2_betterterrain_blocksplace.png)|

| Proper Tree Generation |
|---------|
| *Updated tree generation to randomly construct canopy.* |
| ![Alt Text 1](milestones/3_propertrees.png)|

| G-buffer Normal | G-buffer Depth |
|----------------------------|--------------------------------|
| *Working on implementing SSAO. Implemented G-buffer with debug view for surface normals.* | *Working on implementing SSAO. Implemented G-buffer with debug view for depth.* |
| ![](milestones/4a_normals.png) | ![](milestones/4b_depth.png) |

| SSAO (Off) | SSAO (On) |
|----------------------------|--------------------------------|
| *Previous version of engine before implementation of SSAO.* | *SSAO significantly improves scene depth by enhancing contact shadows at the intersections where blocks meet. This helps improve the detail of the geometry.* |
| ![](milestones/5a_SSAO_OFF.png) | ![](milestones/5b_SSAO_ON.png) |

| Frustum Culling (Off) | Frustum Culling (On) |
|----------------------------|--------------------------------|
| *FPS: 679* | *FPS: 1057* = ~56% Increase in performance|
| ![](milestones/6a1_FC_OFF.png) | ![](milestones/6b1_FC_ON.png) | 
| ![](milestones/6a2_FC_OFF.png) | ![](milestones/6b2_FC_ON.png) | 

| FXAA (Off) | FXAA (On) |
|----------------------------|--------------------------------|
| *FXAA is turned off. The edges of the white cube are jagged.* | *FXAA helps to smooth out the jagged edges of objects in view. The white cube displays edges that have been noticeably smoothed.* |
| ![](milestones/7a_FXAA_OFF.png) | ![](milestones/7a_FXAA_ON.png) | 
![](milestones/7a_FXAA_OFF_Enh.png) | ![](milestones/7a_FXAA_ON_Enh.png) |

| Flat Water | Beautiful Water |
|----------------------------|--------------------------------|
| *Previous version of engine using static water textures.* | *Enhanced water using reflection/refraction textures, and DuDv distortion.* |
| ![](milestones/8b_RTXWATER_OFF.png) | ![](milestones/8a_RTXWATER_ON.png) |

| Fog (Off) | Fog (On) |
|----------------------------|--------------------------------|
| *World rendered without fog enabled.* | *Post-processing fog used to obscure objects further away.* |
| ![](milestones/9a_Fog_OFF.png) | ![](milestones/9b_Fog_ON.png) |

| Optimizations (Off) | Optimizations (On) |
|----------------------------|--------------------------------|
| *World rendered without frustum culling, vertex memory reduction, and greedy meshing.* | *World rendered WITH frustum culling, vertex memory reduction, and greedy meshing.* |
| *FPS: 342* | *FPS: 662 = ~94%  Increase in Performance* |
| |*~88% reduction in RAM usage: 1579 MB -> 185 MB.* |
| ![](milestones/10a1_OPT_OFF.png) | ![](milestones/10b1_OPT_ON.png) |
| ![](milestones/10a2_OPT_OFF.png) | ![](milestones/10b2_OPT_ON.png) |

| OpenGL Render | Vulkan Render |
|----------------------------|--------------------------------|
| *Scene rendered in OpenGL.* | *Scene rendered in Vulkan.* |
| ![](milestones/11a_opengl.png) | ![](milestones/11b_vulkan.png) |

| Shadow Mapping (Off) | Shadow Mapping (On) |
|----------------------------|--------------------------------|
| ![](milestones/12a_Shadows_OFF.gif) | ![](milestones/12b_Shadows_ON.gif) |

<h2>
Requirements
</h2>

> - [Download](https://git-scm.com/install/) and install Git.
> - [Download](https://vulkan.lunarg.com/sdk/home) and install latest Vulkan SDK.
> - [Download](https://visualstudio.microsoft.com/vs/community/) Visual Studio 2022 Community Edition or newer.
> -- Install workloads: *Desktop development with C++*.
> - [Download](https://cmake.org/download/) and install CMake (>= v3.31.0).

<h2>
Build
</h2>

- Clone repo:
```
git clone https://github.com/RobRob7/ProjectAtlas.git
```
- Then run commands:
```
cd ProjectAtlas
mkdir build
cd build
cmake ..
cmake --build . --config Release
```
<h2>
Run
</h2>

- For Command Prompt:

```
cd Release
Atlas.exe
```
- For Git Bash:
```
cd Release
./Atlas.exe
```


<h2>
Dependencies
</h2>

Libraries already provided, the following are used:
|Library|Usage|Version|
|-------|-------|-----|
|[Glad](https://glad.dav1d.de/)|OpenGL loader generator|N/A|
|[GLFW](https://www.glfw.org/download.html)|API for creating windows, contexts and surfaces, receiving input and events|v3.4|
|[GLM](https://github.com/g-truc/glm/releases/tag/1.0.1)|Header only C++ mathematics library|v1.01|
|[ImGui](https://github.com/ocornut/imgui/releases/tag/v1.92.5)|Bloat-free Graphical User interface for C++|v1.92.5|
|[LibNoise](https://libnoise.sourceforge.net/)|A portable, open-source, coherent noise-generating library for C++|v1.0.0|

<h2>
Project Structure
</h2>

Project layout:
- **include/**
  - internal header files
- **src/**
    - main.cpp → main driver
    - **chunk/**
        - **opengl/**
            - chunk_mesh_gpu_gl.cpp → chunk mesh opengl
        - **vulkan/**
            - chunk_mesh_gpu_vk.cpp → chunk mesh vulkan
        - chunk_data.cpp → chunk data
        - chunk_manager.cpp → management of chunk meshes
        - chunk_mesh.cpp → chunk mesh
    - **core/**
        - application.cpp → main application
        - scene.cpp → object setup + renderer call opengl
        - scene_vk.cpp → object setup + renderer call vulkan
    - **light/**
        - light.cpp → light cube object opengl
        - light_vk.cpp → light cube object vulkan
    - **main_opengl/**
        - opengl_main.cpp → opengl main instance
    - **main_vulkan/**
        - buffer_vk.cpp → buffer helper class
        - descriptor_set_vk.cpp → descriptor set helper class
        - graphics_pipeline_vk.cpp → pipeline helper class
        - vulkan_main.cpp → vulkan main instance
    - **player/**
        - crosshair.cpp → crosshair UI icon opengl
        - crosshair_vk.cpp → crosshair UI icon vulkan
    - **renderer/**
        - **opengl/**
            - chunk_pass_gl.cpp → opaque chunk render
            - debug_pass.cpp → G-buffer normal + depth view
            - fog_pass.cpp → fog pass
            - fxaa_pass.cpp → FXAA pass
            - gbuffer_pass.cpp → G-buffer pass
            - present_pass.cpp → final image pass
            - renderer_gl.cpp → render pipeline
            - shadow_map_pass_gl.cpp → shadow map pass
            - ssao_pass.cpp → SSAO pass
            - water_pass.cpp → water pass
        - **vulkan/**
            - chunk_pass_vk.cpp → opaque chunk render
            - debug_pass_vk.cpp → G-buffer normal + depth view
            - fog_pass_vk.cpp → fog pass
            - fxaa_pass_vk.cpp → FXAA pass
            - gbuffer_pass_vk.cpp → G-buffer pass
            - present_pass_vk.cpp → final image pass
            - renderer_vk.cpp → render pipeline
            - shadow_map_pass_vk.cpp → shadow map pass
            - ssao_pass_vk.cpp → SSAO pass
            - water_pass_vk.cpp → water pass
    - **save/**
        - save.cpp → world state saving
    - **system/**
        - camera.cpp → camera system
    - **ui/**
        - ui.cpp → UI system
    - **utility/**
        - **opengl/**
            - cubemap_gl.cpp → setup + render cubemap
            - shader.cpp → shader helper class
            - texture.cpp → setup texture
            - ubo_gl.cpp → UBO upload
        - **vulkan/**
            - cubemap_vk.cpp → setup + render cubemap
            - image_vk.cpp → texture base
            - shader_vk.cpp → shader helper
            - texture_2d_vk.cpp → load texture from file
            - texture_cubemap_vk.cpp → load multiple textures from file
            - utils_vk.cpp → transition image helpers
- **res/**
  - **shader/** → Shaders
  - **texture/** → Textures
- **deps/** → Dependency files
- **papers/** → Papers implemented
