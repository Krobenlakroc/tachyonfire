<div align="center"> <img width="1997" height="720" alt="logosteam" src="https://github.com/user-attachments/assets/3d87415b-357a-492e-8a30-5c839c6b5506" />

# Tachyon Fire Game Engine
 
**Special purpose 3D game engine for making a singleplayer Arena FPS**

[gameplay.webm](https://github.com/user-attachments/assets/0d77ca9a-d3ba-485d-9ab7-48e0d615c754)


</div>

## About

Tachyon Fire is a cross-platform arena FPS engine (Windows/Linux) written in C, using SDL2 and OpenGL 4.6. It combines a modern clustered deferred rendering pipeline with arcade-style collision detection and player movement code. The engine code is licensed under GPLv3, but the game's assets are properitary.  

## Libraries
* Misc Code from Exengine (https://github.com/solenum/exengine/)
  - IQM based animation library (modified to support instancing and ragdolls)
  - Trisoup Collision code (modified painstakingly for improved stability)
* Freetype
* Quake III's movement code (modified to support sliding)
* Openalsoft
* Spng
* Miniz
* Glad
* SDL2

## Features

* Hybrid clustered deferred/forward rendering pipeline.
* Pre-baked 2nd order spherical harmonic global illumination
* Raytraced reflections using distance imposters (also pre-baked)
* Continous trisoup collision detection
* Skeletal Animation
* Spline based enemy movement
* Custom UI Framework
* Multithreaded boids simulation using a spatial hash
* Physically based rendering
* Inverse Kinematics
* Ragdoll physics
* Deferred Decals
* Sphere proxy ambient occlusion using tiled shading
* Multithreaded SIMD frustum culling
* Priority/distance based audio system

  <img width="1920" height="1080" alt="screen4" src="https://github.com/user-attachments/assets/30626aaa-72be-4518-9479-eb276659fd50" />


## Building

The Linux build is done inside a container, so the executable and its dlls can be linked against glibc 2.28. I reccomend using podman.
The windows build is done inside a window$ 10 vm with an msys2 ucrt64 toolchain. You can use the msys2 package manager to install everything except for freetype which needs to be compiled to use no dependencies and linked statically.  

The builds don't work out of the box, you'll have to copy the src,include, and ftinclude directories into the proper builds directory.
When I started programming in C I had never heard of cmake, so I wrote build.lua to generate a Makefile and never got around to using a real build system (not that they are much better).
If you want to make a pull request to add cmake or something similar, I'll merge it.

## Implementation notes

The engine uses a +Y down, +Z forward, +X left coordinate system (the regular OpenGL right handed coordinate system multiplied by <-1,-1,-1>). 
No other 3D software (to my knowledge) uses this convention so everything imported will be backwards and upside down (normal maps, models, etc). I compensate for flipped normals in the shader code (expecting OpenGL normal maps as input). The blender level export script compensates for it at export time. IQM models will be flipped, but I never corrected it because the only one used in the game is symetrical. 

The raytraced reflection solution uses distance imposters (cubemaps with depth information) as the world representation. This works really well for reflecting rooms that can be represented as peicewise combinations of distance functions taken at different points, but will require a lot of memory for scenes with more complex geometry that blocks a lot of the scene. Hallways, corridoors or large outdoor areas make placing the cubemaps relatively simple (just place them in areas where they can "see" a lot of the scene), but a level like a dense forest would be almost impossible with all the nooks and crannies. 

I used a variable timestep instead of a fixed one for simplicity. Consequently, some jumps will only work at certain framerates.

## AI Use

**The game's assets (levels, textures, sound, music, capsule art) are 100% human made.**

I used LLMs (mainly claude web interface) for some tasks during the latter portion of engine development (2025-2026).

The files in the script directory are vibe coded because they don't affect the actual execution of the game.
There are only 2 files in the engine codebase that are fully AI generated (with some tweaks), th_splinegen and th_liquidgen. 
For the rest of the engine, I used AI sometimes to do one off helper functions or as a rubber duck to research problems with.

The portions of the engine I did prior to 2024 (https://www.youtube.com/watch?v=FNSB2U2YxPo) such as the collision system and the distance imposter raytracing had no AI involvement.
If you are like me and are skeptical of projects that seem "vibe coded" out of nowhere by someone with no appreciation for software quality, I promise you this is not the case. The code is pretty messy and lacks standardization/extensibility but its systems were loosely coupled enough to make something that works.







