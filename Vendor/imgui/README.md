# ImGui-v
An ImGui fork without index buffer, making it easier to do vertex pulling for bindless renderers. This project made to be used on [Lorr](https://github.com/exdal/Lorr) renderer.

Instead of calling deindex function and building a new vertex buffer for each frame, the draw backend code is modified to scrap out index buffer and write into vertex buffer directly.