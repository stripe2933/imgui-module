#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_hpp_macros.hpp>

import std;
import imgui_impl_glfw;
import imgui_impl_vulkan;
import vulkan_hpp;

#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif

// Use at your own risk.
template <typename T>
[[nodiscard]] const T &lvalue(const T &&rvalue) noexcept {
    return rvalue;
}

class Swapchain {
public:
    vk::Extent2D extent;
    vk::raii::SwapchainKHR swapchain;
    std::vector<vk::Image> images;
    std::vector<vk::raii::ImageView> imageViews;
    std::vector<vk::raii::Semaphore> imageReadySemaphores;

    Swapchain(
        const vk::raii::Device &device,
        vk::SurfaceKHR surface,
        const vk::Extent2D &extent,
        const vk::SurfaceCapabilitiesKHR &surfaceCapabilities,
        vk::SwapchainKHR oldSwapchain = {}
    ) : extent { extent },
        swapchain { device, vk::SwapchainCreateInfoKHR {
            {},
            surface,
            getMinImageCount(surfaceCapabilities),
            vk::Format::eB8G8R8A8Unorm,
            vk::ColorSpaceKHR::eSrgbNonlinear,
            extent,
            1,
            vk::ImageUsageFlagBits::eColorAttachment,
            vk::SharingMode::eExclusive,
            {},
            surfaceCapabilities.currentTransform,
            vk::CompositeAlphaFlagBitsKHR::eOpaque,
            vk::PresentModeKHR::eFifo,
            false,
            oldSwapchain
        } },
        images { swapchain.getImages() },
        imageViews { std::from_range, images | std::views::transform([&](vk::Image image) {
            return vk::raii::ImageView { device, vk::ImageViewCreateInfo {
                {},
                image,
                vk::ImageViewType::e2D,
                vk::Format::eB8G8R8A8Unorm,
                {},
                vk::ImageSubresourceRange { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 },
            } };
        }) },
        imageReadySemaphores { std::from_range, images | std::views::transform([&](auto) {
            return vk::raii::Semaphore { device, vk::SemaphoreCreateInfo{} };
        }) } { }

private:
    [[nodiscard]] static std::uint32_t getMinImageCount(const vk::SurfaceCapabilitiesKHR &surfaceCapabilities) noexcept {
        std::uint32_t result = surfaceCapabilities.minImageCount + 1;
        if (surfaceCapabilities.maxImageCount != 0) {
            result = std::min(result, surfaceCapabilities.maxImageCount);
        }
        return result;
    }
};

class App {
public:
    explicit App()
        : window { createWindow() }
        , instance { createInstance() }
        , surface { createSurface() }
        , physicalDevice { instance.enumeratePhysicalDevices().at(0) }
        , queueFamily { getQueueFamily() }
        , device { createDevice() }
        , queue { (*device).getQueue(queueFamily, 0) }
        , renderPass { createRenderPass() }
        , swapchain { device, *surface, getFramebufferExtent(), physicalDevice.getSurfaceCapabilitiesKHR(*surface) }
        , framebuffers { createFramebuffers() } {
        // Register the callback that recreates the swapchain and related resources when the window is resized.
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, [](GLFWwindow *window, int width, int height) {
            while (width == 0 || height == 0) {
                glfwWaitEvents();
                glfwGetFramebufferSize(window, &width, &height);
            }

            auto app = static_cast<App*>(glfwGetWindowUserPointer(window));
            app->device.waitIdle();

            app->swapchain = {
                app->device,
                app->surface,
                vk::Extent2D { static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height) },
                app->physicalDevice.getSurfaceCapabilitiesKHR(app->surface),
                *app->swapchain.swapchain
            };
            app->framebuffers = app->createFramebuffers();
        });

        // Initialize ImGui.
        ImGui::CheckVersion();
        ImGui::CreateContext();

        ImGui_ImplGlfw_InitForVulkan(window, true);

        ImGui_ImplVulkan_InitInfo initInfo {
            .ApiVersion = vk::makeApiVersion(0, 1, 0, 0),
            .Instance = *instance,
            .PhysicalDevice = *physicalDevice,
            .Device = *device,
            .QueueFamily = queueFamily,
            .Queue = queue,
            .DescriptorPoolSize = 128,
            .MinImageCount = 2,
            .ImageCount = 2,
            .PipelineInfoMain = {
                .RenderPass = *renderPass,
                .Subpass = 0,
            },
        };
        ImGui_ImplVulkan_Init(&initInfo);
    }

    ~App() {
        // Cleanup ImGui resources.
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        // Cleanup GLFW window.
        glfwDestroyWindow(window);
    }

    void run() {
        // Create Vulkan command pool and allocate a command buffer.
        vk::raii::CommandPool commandPool { device, vk::CommandPoolCreateInfo { {}, queueFamily } };
        vk::CommandBuffer frameCommandBuffer = (*device).allocateCommandBuffers({ *commandPool, vk::CommandBufferLevel::ePrimary, 1 })[0];

        // Frame synchronization stuffs.
        vk::raii::Semaphore imageAvailableSemaphore { device, vk::SemaphoreCreateInfo{} };
        vk::raii::Fence frameReadyFence { device, vk::FenceCreateInfo { vk::FenceCreateFlagBits::eSignaled } };

        while (!glfwWindowShouldClose(window)) {
            // Wait for the previous frame's execution.
            std::ignore = device.waitForFences(*frameReadyFence, true, ~0ULL);

            // Handle window events.
            glfwPollEvents();

            // ImGui.
            ImGui_ImplGlfw_NewFrame();
            ImGui_ImplVulkan_NewFrame();
            ImGui::NewFrame();

            ImGui::ShowDemoWindow();

            ImGui::Render();

            // Acquire swapchain image.
            std::uint32_t swapchainImageIndex;
            try {
                swapchainImageIndex = (*device).acquireNextImageKHR(*swapchain.swapchain, ~0ULL, *imageAvailableSemaphore).value;
            }
            catch (const vk::OutOfDateKHRError&) {
                continue;
            }

            // Record frame command buffer.
            commandPool.reset();
            frameCommandBuffer.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

            frameCommandBuffer.beginRenderPass({
                *renderPass,
                *framebuffers[swapchainImageIndex],
                vk::Rect2D { {}, swapchain.extent },
                lvalue<vk::ClearValue>(vk::ClearColorValue { 0.f, 0.f, 0.f, 0.f }),
            }, vk::SubpassContents::eInline);

            // Draw ImGui.
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), frameCommandBuffer);

            frameCommandBuffer.endRenderPass();

            frameCommandBuffer.end();

            // Submit frame command buffer.
            device.resetFences(*frameReadyFence);
            queue.submit(vk::SubmitInfo {
                *imageAvailableSemaphore,
                lvalue(vk::Flags { vk::PipelineStageFlagBits::eColorAttachmentOutput }),
                frameCommandBuffer,
                *swapchain.imageReadySemaphores[swapchainImageIndex],
            }, *frameReadyFence);

            // Present the acquired swapchain image.
            try {
                std::ignore = queue.presentKHR({ *swapchain.imageReadySemaphores[swapchainImageIndex], *swapchain.swapchain, swapchainImageIndex });
            }
            catch (const vk::OutOfDateKHRError&) { }
        }

        device.waitIdle();
    }

private:
    GLFWwindow *window;

    vk::raii::Context context;
    vk::raii::Instance instance;
    vk::raii::SurfaceKHR surface;
    vk::raii::PhysicalDevice physicalDevice;
    std::uint32_t queueFamily;
    vk::raii::Device device;
    vk::Queue queue;

    vk::raii::RenderPass renderPass;

    Swapchain swapchain;
    std::vector<vk::raii::Framebuffer> framebuffers;

    [[nodiscard]] static GLFWwindow *createWindow() {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        return glfwCreateWindow(800, 480, "ImGui Example", nullptr, nullptr);
    }

    [[nodiscard]] vk::raii::Instance createInstance() const {
    #if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
        // Initialize Vulkan function pointers.
        VULKAN_HPP_DEFAULT_DISPATCHER.init();
    #endif

        std::vector<const char*> extensions;

        for (const vk::ExtensionProperties &props : vk::enumerateInstanceExtensionProperties()) {
            if (static_cast<std::string_view>(props.extensionName) == vk::KHRPortabilityEnumerationExtensionName) {
                // This application supports the Vulkan portability subset.
                extensions.push_back(vk::KHRGetPhysicalDeviceProperties2ExtensionName);
                extensions.push_back(vk::KHRPortabilityEnumerationExtensionName);
                break;
            }
        }

        // Add required Vulkan instance extensions for GLFW.
        std::uint32_t glfwExtensionCount;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::copy_n(glfwExtensions, glfwExtensionCount, back_inserter(extensions));

        vk::raii::Instance result { context, vk::InstanceCreateInfo {
            vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR,
            &lvalue(vk::ApplicationInfo {
                "ImGui Example", 0,
                nullptr, 0,
                vk::makeApiVersion(0, 1, 0, 0),
            }),
            {},
            extensions,
        } };

    #if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
        // Initialize per-instance Vulkan function pointers.
        VULKAN_HPP_DEFAULT_DISPATCHER.init(*result);
    #endif

        return result;
    }

    [[nodiscard]] vk::raii::SurfaceKHR createSurface() const {
        if (VkSurfaceKHR rawSurface; glfwCreateWindowSurface(*instance, window, nullptr, &rawSurface) == VK_SUCCESS) {
            return { instance, rawSurface };
        }

        throw std::runtime_error { "Failed to create Vulkan surface" };
    }

    [[nodiscard]] std::uint32_t getQueueFamily() const {
        // Find queue family that is both capable of graphics operations and presentation.
        std::uint32_t result = 0;
        for (const vk::QueueFamilyProperties &props : physicalDevice.getQueueFamilyProperties()) {
            if ((props.queueFlags & vk::QueueFlagBits::eGraphics) && physicalDevice.getSurfaceSupportKHR(result, *surface)) {
                break;
            }
            ++result;
        }
        return result;
    }

    [[nodiscard]] vk::raii::Device createDevice() const {
        std::vector extensions {
            vk::KHRSwapchainExtensionName,
        };

        for (const vk::ExtensionProperties &props : physicalDevice.enumerateDeviceExtensionProperties()) {
            if (static_cast<std::string_view>(props.extensionName) == vk::KHRPortabilitySubsetExtensionName) {
                // This application supports the Vulkan portability subset.
                extensions.push_back(vk::KHRPortabilitySubsetExtensionName);
                break;
            }
        }

        vk::raii::Device result { physicalDevice, vk::DeviceCreateInfo {
            {},
            lvalue(vk::DeviceQueueCreateInfo {
                {},
                queueFamily,
                vk::ArrayProxyNoTemporaries<const float> { lvalue(1.f) }
            }),
            {},
            extensions,
        } };

    #if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
        // Initialize per-device Vulkan function pointers.
        VULKAN_HPP_DEFAULT_DISPATCHER.init(*result);
    #endif

        return result;
    }

    [[nodiscard]] vk::raii::RenderPass createRenderPass() const {
        return { device, vk::RenderPassCreateInfo {
            {},
            lvalue(vk::AttachmentDescription {
                {},
                vk::Format::eB8G8R8A8Unorm,
                vk::SampleCountFlagBits::e1,
                vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
                {}, {},
                {}, vk::ImageLayout::ePresentSrcKHR
            }),
            lvalue(vk::SubpassDescription {
                {},
                vk::PipelineBindPoint::eGraphics,
                {},
                lvalue(vk::AttachmentReference { 0, vk::ImageLayout::eColorAttachmentOptimal }),
            }),
            lvalue(vk::SubpassDependency {
                vk::SubpassExternal, 0,
                vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput,
                {}, vk::AccessFlagBits::eColorAttachmentWrite,
            }),
        } };
    }

    [[nodiscard]] std::vector<vk::raii::Framebuffer> createFramebuffers() const {
        return swapchain.imageViews
            | std::views::transform([this](const vk::raii::ImageView &imageView) {
                return vk::raii::Framebuffer { device, vk::FramebufferCreateInfo {
                    {},
                    *renderPass,
                    *imageView,
                    swapchain.extent.width, swapchain.extent.height, 1,
                } };
            })
            | std::ranges::to<std::vector>();
    }

    [[nodiscard]] vk::Extent2D getFramebufferExtent() const {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        return { static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height) };
    }
};

int main() {
    glfwInit();
    try {
        App{}.run();
    }
    catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    glfwTerminate();
}