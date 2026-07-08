# Fado Game Engine

##### Ongoing project

Fado is a custom 3D game engine written in **C++** using **Direct3D 11**.
The engine is heavily data oriented; we have a bunch of structs, and create functions that operate on those structs.

![Description](media/showcase.gif)

### Latest Features
- Improved levels load save and update.
- 2D sprite animation system.
- A unified material shader, with color/tint, texture, lit/unlit and transparency.
- Custom compressed file format for textures, models, fonts and audios.
- 2D and 3D custom sound system. (XAusio2 for Windows).
- Seamless Cubemap Skybox.
- Real-time entities transform editing with ImGui.
- Added functioning buttons.
- Implemented a UI commands bucket with custom UI system and ImGui for debugging.
- Game hot reloading.
- AABB and OBB collision detection.
- Entities in an Entity Component System. Transforms, meshes and textures are set in arrays through handles. 
- Memory Arena system that allocates memeory only at startup and uses it throughout the session.
- Fully resizable and full-screen toggle-supported window.
- Keyboard and multiple controllers input support with XInput.

### Current Structure

##### Game Layer
- Handles the game/engine input.
- This is still in the early stages, eventually, it'll be the gateway to all the interactive objects in the engine and/or game.

##### Rendering Layer
- Includes all of the DirectX11 rendering requirements and types.
- Currently consists of direct3D structs, initialization and rendering functions. Only init and Render are public and exposed to other files.

##### Platform Layer
- This is where the program starts and the game loop begins (main).
- Currently, the engine is supported only for Windows with a Win32 platform layer.
- The main function uses VirtualAlloc once and allocates memeory that is used throughout the code, so that no dynamic allocations happen in the game (DX11 init is an expection as some elements are managed and freed by DX).

