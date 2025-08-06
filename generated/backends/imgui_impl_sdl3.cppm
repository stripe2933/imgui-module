module;

#include <imgui_impl_sdl3.h>

export module imgui_impl_sdl3;

export import imgui;

export {
    // ----- Types -----

    using ::ImGui_ImplSDL3_GamepadMode;
    using ::SDL_Event;
    using ::SDL_Gamepad;
    using ::SDL_Renderer;
    using ::SDL_Window;

    // ----- Functions -----

    using ::ImGui_ImplSDL3_InitForD3D;
    using ::ImGui_ImplSDL3_InitForMetal;
    using ::ImGui_ImplSDL3_InitForOpenGL;
    using ::ImGui_ImplSDL3_InitForOther;
    using ::ImGui_ImplSDL3_InitForSDLGPU;
    using ::ImGui_ImplSDL3_InitForSDLRenderer;
    using ::ImGui_ImplSDL3_InitForVulkan;
    using ::ImGui_ImplSDL3_NewFrame;
    using ::ImGui_ImplSDL3_ProcessEvent;
    using ::ImGui_ImplSDL3_SetGamepadMode;
    using ::ImGui_ImplSDL3_Shutdown;
}
