#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_hpp_macros.hpp>

import std;
import imgui_impl_glfw;
import imgui_impl_vulkan;
import vulkan_hpp;

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

class Swapchain {
public:
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
    ) : swapchain { device, vk::SwapchainCreateInfoKHR {
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
        std::uint32_t minImageCount = surfaceCapabilities.minImageCount + 1;
        if (surfaceCapabilities.maxImageCount != 0) {
            minImageCount = std::min(minImageCount, surfaceCapabilities.maxImageCount);
        }
        return minImageCount;
    }
};

int main() {
    // Initialize ImGui.
    ImGui::CheckVersion();
    ImGui::CreateContext();

    // Create GLFW window.
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(800, 480, "ImGui Example", nullptr, nullptr);

    // Create Vulkan context.
    VULKAN_HPP_DEFAULT_DISPATCHER.init();

    vk::raii::Context context;

    // Create Vulkan instance.
    constexpr vk::ApplicationInfo applicationInfo {
        "ImGui Example", 0,
        nullptr, 0,
        vk::makeApiVersion(0, 1, 0, 0),
    };

    std::vector<const char*> instanceExtensions;

    // This application supports the Vulkan portability subset.
    for (const vk::ExtensionProperties &props : vk::enumerateInstanceExtensionProperties()) {
        if (static_cast<std::string_view>(props.extensionName) == vk::KHRPortabilityEnumerationExtensionName) {
            instanceExtensions.push_back(vk::KHRGetPhysicalDeviceProperties2ExtensionName);
            instanceExtensions.push_back(vk::KHRPortabilityEnumerationExtensionName);
            break;
        }
    }

    // Add required Vulkan instance extensions for GLFW.
    std::uint32_t glfwExtensionCount;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    instanceExtensions.append_range(std::views::counted(glfwExtensions, glfwExtensionCount));

    const vk::raii::Instance instance { context, vk::InstanceCreateInfo {
        vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR,
        &applicationInfo,
        {},
        instanceExtensions,
    } };

    VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

    // Create Vulkan surface.
    const vk::raii::SurfaceKHR surface { instance, [&] {
        if (VkSurfaceKHR result; glfwCreateWindowSurface(*instance, window, nullptr, &result) == VK_SUCCESS) {
            return result;
        }

        std::cerr << "Failed to create Vulkan surface.\n";
        std::exit(1);
    }() };

    // Find suitable GPU. For the sake of simplicity, we will just use the first available physical device.
    const vk::raii::PhysicalDevice physicalDevice = instance.enumeratePhysicalDevices().at(0);

    // Find queue family that is both capable of graphics operations and presentation.
    std::uint32_t graphicsQueueFamily = 0;
    for (const vk::QueueFamilyProperties &props : physicalDevice.getQueueFamilyProperties()) {
        if ((props.queueFlags & vk::QueueFlagBits::eGraphics) && physicalDevice.getSurfaceSupportKHR(graphicsQueueFamily, *surface)) {
            break;
        }
        ++graphicsQueueFamily;
    }

    // Create Vulkan device.
    constexpr float priority = 1.f;
    const vk::DeviceQueueCreateInfo queueCreateInfo { {}, graphicsQueueFamily, vk::ArrayProxyNoTemporaries<const float> { priority } };

    std::vector deviceExtensions {
        vk::KHRSwapchainExtensionName,
    };
    for (const vk::ExtensionProperties &props : physicalDevice.enumerateDeviceExtensionProperties()) {
        if (static_cast<std::string_view>(props.extensionName) == vk::KHRPortabilitySubsetExtensionName) {
            deviceExtensions.push_back(vk::KHRPortabilitySubsetExtensionName);
            break;
        }
    }

    vk::raii::Device device { physicalDevice, vk::DeviceCreateInfo {
        {},
        queueCreateInfo,
        {},
        deviceExtensions,
    } };

    VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);

    vk::Queue queue = (*device).getQueue(graphicsQueueFamily, 0);

    // Create Vulkan render pass.
    constexpr vk::AttachmentDescription attachmentDescription {
        {},
        vk::Format::eB8G8R8A8Unorm,
        vk::SampleCountFlagBits::e1,
        vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
        {}, {},
        {}, vk::ImageLayout::ePresentSrcKHR
    };

    constexpr vk::AttachmentReference attachmentReference { 0, vk::ImageLayout::eColorAttachmentOptimal };
    const vk::SubpassDescription subpassDescription {
        {},
        vk::PipelineBindPoint::eGraphics,
        {},
        attachmentReference,
    };

    constexpr vk::SubpassDependency subpassDependency {
        vk::SubpassExternal, 0,
        vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput,
        {}, vk::AccessFlagBits::eColorAttachmentWrite,
    };

    const vk::raii::RenderPass renderPass { device, vk::RenderPassCreateInfo {
        {},
        attachmentDescription,
        subpassDescription,
        subpassDependency,
    } };

    // Create Vulkan swapchain and related resources.
    const auto getFramebufferExtent = [&] -> vk::Extent2D {
        int framebufferWidth, framebufferHeight;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        return {
            static_cast<std::uint32_t>(framebufferWidth),
            static_cast<std::uint32_t>(framebufferHeight)
        };
    };

    vk::Extent2D swapchainExtent = getFramebufferExtent();
    Swapchain swapchain { device, *surface, swapchainExtent, physicalDevice.getSurfaceCapabilitiesKHR(*surface) };

    // Create framebuffers.
    std::vector<vk::raii::Framebuffer> framebuffers
        = swapchain.imageViews
        | std::views::transform([&](const vk::raii::ImageView &imageView) {
            return vk::raii::Framebuffer { device, vk::FramebufferCreateInfo {
                {},
                *renderPass,
                *imageView,
                swapchainExtent.width, swapchainExtent.height, 1,
            } };
        })
        | std::ranges::to<std::vector>();

    struct UserData {
        const vk::raii::PhysicalDevice &physicalDevice;
        const vk::raii::Device &device;
        vk::SurfaceKHR surface;
        vk::RenderPass renderPass;
        vk::Extent2D &swapchainExtent;
        Swapchain &swapchain;
        std::vector<vk::raii::Framebuffer> &framebuffers;
    };

    UserData userData { physicalDevice, device, *surface, *renderPass, swapchainExtent, swapchain, framebuffers };
    glfwSetWindowUserPointer(window, &userData);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow *window, int width, int height) {
        for (; width == 0 || height == 0; glfwGetFramebufferSize(window, &width, &height)) {
            glfwWaitEvents();
        }

        auto &[physicalDevice, device, surface, renderPass, swapchainExtent, swapchain, framebuffers] = *static_cast<UserData*>(glfwGetWindowUserPointer(window));
        device.waitIdle();

        swapchainExtent = vk::Extent2D { static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height) };
        swapchain = { device, surface, swapchainExtent, physicalDevice.getSurfaceCapabilitiesKHR(surface), *swapchain.swapchain };
        framebuffers
            = swapchain.imageViews
            | std::views::transform([&](const vk::raii::ImageView &imageView) {
                return vk::raii::Framebuffer { device, vk::FramebufferCreateInfo {
                    {},
                    renderPass,
                    *imageView,
                    swapchainExtent.width, swapchainExtent.height, 1,
                } };
            })
            | std::ranges::to<std::vector>();
    });

    // Create Vulkan command pool and allocate a command buffer.
    vk::raii::CommandPool commandPool { device, vk::CommandPoolCreateInfo { {}, graphicsQueueFamily } };
    vk::CommandBuffer frameCommandBuffer = (*device).allocateCommandBuffers({ *commandPool, vk::CommandBufferLevel::ePrimary, 1 })[0];

    // Frame synchronization stuffs.
    vk::raii::Semaphore imageAvailableSemaphore { device, vk::SemaphoreCreateInfo{} };
    vk::raii::Fence frameReadyFence { device, vk::FenceCreateInfo { vk::FenceCreateFlagBits::eSignaled } };

    // Initialize ImGui with GLFW and Vulkan.
    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo initInfo {
        .ApiVersion = vk::makeApiVersion(0, 1, 0, 0),
        .Instance = *instance,
        .PhysicalDevice = *physicalDevice,
        .Device = *device,
        .QueueFamily = graphicsQueueFamily,
        .Queue = queue,
        .RenderPass = *renderPass,
        .MinImageCount = 2,
        .ImageCount = 2,
        .Subpass = 0,
        .DescriptorPoolSize = 128,
    };
    ImGui_ImplVulkan_Init(&initInfo);

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

        constexpr vk::ClearValue clearValue{};
        frameCommandBuffer.beginRenderPass({
            *renderPass,
            *framebuffers[swapchainImageIndex],
            vk::Rect2D { {}, swapchainExtent },
            clearValue,
        }, vk::SubpassContents::eInline);

        // Draw ImGui.
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), frameCommandBuffer);

        frameCommandBuffer.endRenderPass();

        frameCommandBuffer.end();

        // Submit frame command buffer.
        constexpr vk::PipelineStageFlags waitDstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        device.resetFences(*frameReadyFence);
        queue.submit(vk::SubmitInfo {
            *imageAvailableSemaphore,
            waitDstStageMask,
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

    // Cleanup ImGui resources.
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Cleanup GLFW window.
    glfwDestroyWindow(window);
    glfwTerminate();
}