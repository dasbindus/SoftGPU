# imgui - Dear ImGui

This is a placeholder for the Dear ImGui library.

## Installation

imgui should be initialized as a git submodule:

```bash
git submodule update --init --recursive
```

Or clone manually:

```bash
git clone --depth 1 https://github.com/ocornut/imgui.git extern/imgui
```

## Note

ImGui backends (imgui_impl_glfw.cpp, imgui_impl_opengl3.cpp) are included in the imgui repository.
CMake will automatically build them when adding imgui as a subdirectory.
