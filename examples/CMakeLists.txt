cmake_minimum_required(VERSION 3.30)

# Enable C++ standard library module.
if (CMAKE_VERSION VERSION_GREATER_EQUAL "4.0.3")
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
elseif (CMAKE_VERSION VERSION_GREATER_EQUAL "4.0.1")
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "a9e1cf81-9932-4810-974b-6eccaf14e457")
else()
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "0e5b6991-d74f-4b3d-a41c-cf096e0b2508")
endif()

project(imgui_module_examples LANGUAGES CXX)

# ----- External dependencies -----

find_package(glfw3 CONFIG)
find_package(OpenGL)
find_package(Vulkan)

# ----- Module CMake targets for the dependencies -----

if (${Vulkan_FOUND})
    add_library(Vulkan_module)
    target_compile_features(Vulkan_module PUBLIC cxx_std_23)
    target_include_directories(Vulkan_module PUBLIC ${Vulkan_INCLUDE_DIRS})
    target_sources(Vulkan_module PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${Vulkan_INCLUDE_DIRS}
        FILES
            ${Vulkan_INCLUDE_DIRS}/vulkan/vulkan.cppm
    )
    target_compile_definitions(Vulkan_module
        PUBLIC
            VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
            VK_ENABLE_BETA_EXTENSIONS # VK_KHR_portability_subset
        PRIVATE
            VULKAN_HPP_NO_SETTERS
            VULKAN_HPP_NO_SMART_HANDLE
            VULKAN_HPP_NO_TO_STRING
    )
    target_link_libraries(Vulkan_module PUBLIC Vulkan::Vulkan)
    set_target_properties(Vulkan_module PROPERTIES CXX_MODULE_STD 1)

    add_library(Vulkan::module ALIAS Vulkan_module)
endif()

# ----- Project CMake targets -----

if (${glfw3_FOUND})
    if (${OpenGL_FOUND})
        add_subdirectory(glfw_opengl3)
    endif()
    if (${Vulkan_FOUND})
        add_subdirectory(glfw_vulkan)
    endif()
endif()