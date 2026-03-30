#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <ranges>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#define GLFW_INCLUDE_VULKAN        // REQUIRED only for GLFW CreateWindowSurface.
#include <GLFW/glfw3.h>

const uint32_t WIDTH  = 800;
const uint32_t HEIGHT = 600;
const float queuePriority = 1.0;

const std::vector<char const*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUF
	constexpr bool enableValidationLayers = false;
#else
	constexpr bool enableValidationLayers = true;
#endif

class HelloTriangleApplication
{
  public:
	void run()
	{
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

  private:
	int a = 0;
	GLFWwindow *window = nullptr;
	vk::raii::Context context;
    vk::raii::Instance instance = nullptr;

	vk::raii::SurfaceKHR surface = nullptr;

    vk::raii::PhysicalDevice physicalDevice = nullptr;
	vk::raii::Device device = nullptr;

	vk::raii::Queue graphicsQueue = nullptr;

    vk::raii::SwapchainKHR swapChain = nullptr;
    std::vector<vk::Image> swapChainImages;
    vk::SurfaceFormatKHR   swapChainSurfaceFormat;
    vk::Extent2D           swapChainExtent;

	std::vector<const char *> requiredDeviceExtension = {
	    vk::KHRSwapchainExtensionName};

	void initWindow()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
	}

    void createInstance()
    {
        constexpr vk::ApplicationInfo appInfo{
            .pApplicationName = "Hello Triangle",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "No Engine",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = vk::ApiVersion14
        };

		uint32_t glfwExtensionCount = 0;
		auto     glfwExtensions     = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		vk::InstanceCreateInfo createInfo{
			.pApplicationInfo = &appInfo,
			.enabledLayerCount = static_cast<uint32_t>(validationLayers.size()),
			.ppEnabledLayerNames = validationLayers.data(),
			.enabledExtensionCount = glfwExtensionCount,
			.ppEnabledExtensionNames = glfwExtensions
		};

		instance = vk::raii::Instance(context, createInfo);
    }

	void initVulkan()
	{
        createInstance();
		createSurface();
        pickPhysicalDevice();
		createLogicalDevice();
        createSwapChain();

	}

	void mainLoop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
		}
	}

	void cleanup()
	{
		glfwDestroyWindow(window);

		glfwTerminate();
	}

    bool isDeviceSuitable(vk::raii::PhysicalDevice const& physicalDevice)
    {
        // API check
        bool supportsVulkan1_4 = physicalDevice.getProperties().apiVersion >= vk::ApiVersion14;

        // Queue family check
        auto queueFamilyPropertiesVector = physicalDevice.getQueueFamilyProperties();

        bool supportsGraphics = false;
		for(auto [index, queueFamilyProperties] : queueFamilyPropertiesVector | std::views::enumerate) {
			if(!!(queueFamilyProperties.queueFlags & vk::QueueFlagBits::eGraphics) && physicalDevice.getSurfaceSupportKHR(
				static_cast<uint32_t>(index),
				surface
			)) {
				supportsGraphics = true;
				break;
			}
		}
		if(!supportsGraphics) {
			throw std::runtime_error("physical device does not support graphics queue!");
		}

        // Extensions check
        auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
        bool supportsAllRequiredExtensions = std::ranges::all_of(
            requiredDeviceExtension, [&availableDeviceExtensions](auto const& requiredDeviceExtension){
                return std::ranges::any_of(
                    availableDeviceExtensions, [&requiredDeviceExtension](auto const& availableDeviceExtension){
                        return strcmp( availableDeviceExtension.extensionName, requiredDeviceExtension ) == 0;
                    });
            });
        // features check
        auto features = physicalDevice.template getFeatures2<vk::PhysicalDeviceFeatures2,
                                                            vk::PhysicalDeviceVulkan13Features,
                                                            vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
        bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
                                  features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

        return supportsVulkan1_4 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
    }

	void createSurface()
	{
		VkSurfaceKHR c_surface;
		if (glfwCreateWindowSurface(*instance, window, nullptr, &c_surface) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create window surface!");
		}
		surface = vk::raii::SurfaceKHR(instance, c_surface);
	}

    void pickPhysicalDevice()
    {
        auto physicalDevices = instance.enumeratePhysicalDevices();
        auto const iter = std::ranges::find_if(physicalDevices, [&](auto const& physicalDevice){
            return isDeviceSuitable(physicalDevice);
        });
        if(iter == physicalDevices.end()) {
            throw std::runtime_error("has no availiable physical device!");
        }

        physicalDevice = *iter;
    }

	void createLogicalDevice()
	{
		// queue
		auto queueFamilyPropertiesVector = physicalDevice.getQueueFamilyProperties();

		uint32_t graphicsQueueIndex = ~0;
		for(auto [index, queueFamilyProperties] : queueFamilyPropertiesVector | std::views::enumerate) {
			if(!!(queueFamilyProperties.queueFlags & vk::QueueFlagBits::eGraphics) && physicalDevice.getSurfaceSupportKHR(
				static_cast<uint32_t>(index),
				surface
			)) {
				graphicsQueueIndex = static_cast<uint32_t>(index);
				break;
			}
		}
		if(graphicsQueueIndex == ~0) {
			throw std::runtime_error("physical device does not support graphics queue!");
		}
		std::cout<< graphicsQueueIndex<< "===="<<std::endl;

		vk::DeviceQueueCreateInfo deviceQueueCreateInfo = {
			.queueFamilyIndex = graphicsQueueIndex,
			.queueCount = 1,
			.pQueuePriorities = &queuePriority,
		};

		// feature
		vk::StructureChain
		<
			vk::PhysicalDeviceFeatures2,
			vk::PhysicalDeviceVulkan13Features,
			vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
		> featureChain = {
			{},
			{.dynamicRendering = true},
			{.extendedDynamicState = true},
		};

		// create
		vk::DeviceCreateInfo deviceCreateInfo = {
			.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
			.queueCreateInfoCount = 1,
			.pQueueCreateInfos = &deviceQueueCreateInfo,
			.enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtension.size()),
			.ppEnabledExtensionNames = requiredDeviceExtension.data(),
		};
		device = vk::raii::Device(physicalDevice, deviceCreateInfo);
		graphicsQueue = vk::raii::Queue(device, graphicsQueueIndex, 0);
	}

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
    {
        const auto iter = std::ranges::find_if(availableFormats, [](vk::SurfaceFormatKHR const& surfaceFormat){
            return surfaceFormat.format == vk::Format::eB8G8R8A8Srgb && surfaceFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
        });
        if(iter == availableFormats.end()) {
            throw std::runtime_error("has no available surface format!");
        }
        return *iter;
    }

    vk::PresentModeKHR chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes)
    {
        return std::ranges::any_of(availablePresentModes, [](const auto& presentMode){
            return presentMode == vk::PresentModeKHR::eMailbox;
        }) ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities)
    {
        if(capabilities.currentExtent.height != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }

        int width, height;
        vk::Extent2D extent;
        glfwGetFramebufferSize(window, &width, &height);

        extent.height = static_cast<uint32_t>(height);
        extent.width = static_cast<uint32_t>(width);

        return extent;
    }

    uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &capabilities)
    {
        auto minImageCount = std::max(3u, capabilities.minImageCount);
        if ((0 < capabilities.maxImageCount) && (capabilities.maxImageCount < minImageCount))
        {
            minImageCount = capabilities.maxImageCount;
        }
        return minImageCount;
    }

    void createSwapChain()
    {
        swapChainSurfaceFormat = chooseSwapSurfaceFormat(physicalDevice.getSurfaceFormatsKHR(surface));
        swapChainExtent = chooseSwapExtent(physicalDevice.getSurfaceCapabilitiesKHR(surface));
        vk::SwapchainCreateInfoKHR createInfo{
            .surface = surface,
            .minImageCount = chooseSwapMinImageCount(physicalDevice.getSurfaceCapabilitiesKHR(surface)),
            .imageFormat = swapChainSurfaceFormat.format,
            .imageColorSpace = swapChainSurfaceFormat.colorSpace,
            .imageExtent = swapChainExtent,
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .preTransform = physicalDevice.getSurfaceCapabilitiesKHR(surface).currentTransform,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = chooseSwapPresentMode(physicalDevice.getSurfacePresentModesKHR(surface)),
            .clipped = VK_TRUE,
        };
        swapChain = vk::raii::SwapchainKHR(device, createInfo);
        swapChainImages = swapChain.getImages();
    }
};

int main()
{
	std::cout<<"hello world"<<std::endl;
	try
	{
		HelloTriangleApplication app;
		app.run();

	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
