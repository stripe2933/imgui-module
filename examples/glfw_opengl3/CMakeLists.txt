# imgui::glfw_opengl3_module
add_library(imgui_glfw_opengl3_module)
target_compile_features(imgui_glfw_opengl3_module PUBLIC cxx_std_23)
target_include_directories(imgui_glfw_opengl3_module PRIVATE
    ../../cimgui/imgui
    ../../cimgui/imgui/backends
)
target_sources(imgui_glfw_opengl3_module
    PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ../../generated
        FILES
            ../../generated/imgui.cppm
            ../../generated/backends/imgui_impl_glfw.cppm
            ../../generated/backends/imgui_impl_opengl3.cppm
    PRIVATE
        ../../cimgui/imgui/imgui.cpp
        ../../cimgui/imgui/imgui_demo.cpp
        ../../cimgui/imgui/imgui_draw.cpp
        ../../cimgui/imgui/imgui_tables.cpp
        ../../cimgui/imgui/imgui_widgets.cpp
        ../../cimgui/imgui/backends/imgui_impl_glfw.cpp
        ../../cimgui/imgui/backends/imgui_impl_opengl3.cpp
)
target_compile_definitions(imgui_glfw_opengl3_module PRIVATE
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS
)
target_link_libraries(imgui_glfw_opengl3_module PUBLIC
    glfw
    OpenGL::GL
    $<$<PLATFORM_ID:Linux>:-lX11> # Starting from ImGui v1.92.3, using GLFW backend on Linux/BSD etc. requires linking with -lX11.
)

add_library(imgui::glfw_opengl3_module ALIAS imgui_glfw_opengl3_module)

# ----- Project CMake targets -----

add_executable(glfw_opengl3 main.cpp)
target_compile_features(glfw_opengl3 PUBLIC cxx_std_23)
target_link_libraries(glfw_opengl3 PRIVATE
    imgui::glfw_opengl3_module
)

set_target_properties(glfw_opengl3 PROPERTIES CXX_MODULE_STD 1)