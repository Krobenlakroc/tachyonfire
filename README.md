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
