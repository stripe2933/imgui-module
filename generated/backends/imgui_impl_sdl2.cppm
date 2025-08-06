module;

#include <imgui_impl_sdl2.h>

export module imgui_impl_sdl2;

export import imgui;

export {
    // ----- Types -----

    using ::ImGui_ImplSDL2_GamepadMode;
    using ::SDL_Event;
    using ::SDL_Renderer;
    using ::SDL_Window;
    using ::_SDL_GameController;

    // ----- Functions -----

    using ::ImGui_ImplSDL2_GetContentScaleForDisplay;
    using ::ImGui_ImplSDL2_GetContentScaleForWindow;
    using ::ImGui_ImplSDL2_InitForD3D;
    using ::ImGui_ImplSDL2_InitForMetal;
    using ::ImGui_ImplSDL2_InitForOpenGL;
    using ::ImGui_ImplSDL2_InitForOther;
    using ::ImGui_ImplSDL2_InitForSDLRenderer;
    using ::ImGui_ImplSDL2_InitForVulkan;
    using ::ImGui_ImplSDL2_NewFrame;
    using ::ImGui_ImplSDL2_ProcessEvent;
    using ::ImGui_ImplSDL2_SetGamepadMode;
    using ::ImGui_ImplSDL2_Shutdown;
}
