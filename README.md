# SDF Rendering and Construction in D3D12 with DirectX Raytracing

![Project screenshot](https://u.cubeupload.com/jambuttenshaw/43screenshot.png)

## DirectX Raytracing

My project uses DXR to render signed distance field objects. Geometry is built from procedural AABBs, and associated distance data is stored in a 3D volume and sphere-traced within an intersection shader.

## Memory Efficient Volumes

Volumes are compacted so that no empty space is wasted, and an indirection table is used to associate raytracing AABBs with distance data.

## Async Compute

Async compute is used to rebuild geometry in real-time without stalling the rendering queue. Resources are double-buffered and their states managed to prevent simultaneous reading and writing.

## Hierarchical Reconstruction

![Hierarchical reconstruction](https://u.cubeupload.com/jambuttenshaw/hierarchicalhorizont.png)

Objects are built hierarchically, starting with very large, coarse bricks, each of which can be refined into up to 64 sub-bricks with each iteration.

## Edit Culling

![Edit Culling](https://u.cubeupload.com/jambuttenshaw/editculling.png)

Each brick evaluates which edits (the primitive shapes that make up an object) are relevant, and stores a list of indices of these edits. When the brick is later evaluated, only the relevant edits need to be evaluated which speeds up construction massively.
