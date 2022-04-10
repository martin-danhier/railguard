# Think board

## descriptor sets

Possible levels:

- Global (renderer)
- Global, one per frame (renderer.frames)
- Swapchain-specific (swapchain)
- Swapchain-specific, one per frame
- Camera-specific (should be same: should not have more than one cam per swapchain)
- Model specific (change each draw command)
- Model specifc, one per frame
- Material-specific (change each render batch)
- Material-specific, one per frame

Camera: one per swapchain, dynamic over frames
Scene data: global in renderer, dynamic over frames (so we can update values)
Vertex buffer: one big vertex buffer with all meshes
Index buffer: idem
Material textures: local in material, one per pass (not one per frame, it will mostly be static and won't change from frame to frame)
Render node data (render matrix): big SSBO in renderer dynamic over frames


Actual levels

- Global
  - Scene data (fog, distance, ambient, etc)
  - Mesh data
  - Model matrices (sent all at once, use index to find the right one), avoids a bind at each draw call
- Swapchain specific
  - Camera buffers (no multiple cams)
- Material specific
  - Material data


To allocate them

-> one pool, and if not enough capacity, create new pool. Reset all of them at frame begin
-> DynamicDescriptorPool
