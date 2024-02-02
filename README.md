# SDF Rendering in D3D12 with DirectX Raytracing

![Project screenshot](https://u.cubeupload.com/jambuttenshaw/43screenshot.png)

## DirectX Raytracing

My project uses DXR to render signed distance field objects. Geometry is built from procedural AABBs, and associated distance data is stored in a 3D volume and sphere-traced within an intersection shader.

## Memory Efficient Volumes

Volumes are compacted so that no empty space is wasted, and an indirection table is used to associate raytracing AABBs with distance data.

## Real-time Reconstruction

Async compute is used to rebuild geometry in real-time without stalling the rendering queue.
