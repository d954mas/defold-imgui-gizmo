# ImGui Gizmo Demo (Defold)

Small demo project for testing ImGui Gizmo in Defold: object picking, transforms via gizmo, and a mini camera view manipulator.

## Features
- Translate, rotate, and scale via ImGui Gizmo.
- Uniform scale enforced for the sphere (prevents distortion).
- Object selection by clicking its collider.
- Visual grid and mini view manipulator.

## Run
1. Open the project in Defold.
2. Run `main/main.collection`.

## Controls
- Click an object to select it.
- The left panel lets you change gizmo mode and edit TRS values manually.
- Enable Snap for stepped changes.

## Example
Lua header usage (API + constants) and a minimal per-frame call:

```lua
local imgui = require "imgui"
local imgui_gizmo = require "imgui_gizmo.imgui_gizmo_header"

function update(self, dt)
    -- At least one ImGui call is required each frame to trigger NewFrame().
    imgui.get_frame_height()

    imgui_gizmo.set_context()
    imgui_gizmo.set_rect(0, 0, self.display_width, self.display_height)
    imgui_gizmo.set_drawlist_foreground()
    local manipulated = imgui_gizmo.manipulate(
        self.view,
        self.projection,
        imgui_gizmo.OPERATION_SCALEU,
        imgui_gizmo.MODE_LOCAL,
        self.matrix
    )
    if manipulated then
        -- handle updated matrix here
    end
end
```

## Notes
- The Lua header in `imgui_gizmo/imgui_gizmo_header.lua` is the API entry point (constants and bindings).
- You must call at least one ImGui function every frame so the extension can trigger `NewFrame()`. A no-op like `imgui.get_frame_height()` is sufficient.

## Key Files
- `main/gizmo_demo.script` — demo logic and ImGui Gizmo usage.
- `imgui_gizmo/imgui_gizmo_header.lua` — ImGui Gizmo API (constants + bindings).

## Background
Based on the Defold "basic 3D" template and uses 3D physics.
