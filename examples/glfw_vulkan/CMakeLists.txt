# imgui::glfw_vulkan_module
add_library(imgui_glfw_vulkan_module)
target_compile_features(imgui_glfw_vulkan_module PUBLIC cxx_std_23)
target_include_directories(imgui_glfw_vulkan_module PRIVATE
    ../../cimgui/imgui
    ../../cimgui/imgui/backends
)
target_sources(imgui_glfw_vulkan_module
    PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ../../generated
        FILES
            ../../generated/imgui.cppm
            ../../generated/backends/imgui_impl_glfw.cppm
            ../../generated/backends/imgui_impl_vulkan.cppm
    PRIVATE
        ../../cimgui/imgui/imgui.cpp
        ../../cimgui/imgui/imgui_demo.cpp
        ../../cimgui/imgui/imgui_draw.cpp
        ../../cimgui/imgui/imgui_tables.cpp
        ../../cimgui/imgui/imgui_widgets.cpp
        ../../cimgui/imgui/backends/imgui_impl_glfw.cpp
        ../../cimgui/imgui/backends/imgui_impl_vulkan.cpp
)
target_compile_definitions(imgui_glfw_vulkan_module PRIVATE
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS
)
target_link_libraries(imgui_glfw_vulkan_module PUBLIC
    glfw
    Vulkan::module
)

add_library(imgui::glfw_vulkan_module ALIAS imgui_glfw_vulkan_module)

# ----- Project CMake targets -----

add_executable(glfw_vulkan main.cpp)
target_compile_features(glfw_vulkan PUBLIC cxx_std_23)
target_link_libraries(glfw_vulkan PRIVATE
    imgui::glfw_vulkan_module
)

set_target_properties(glfw_vulkan PROPERTIES CXX_MODULE_STD 1)
