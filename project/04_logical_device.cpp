#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>

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
	GLFWwindow *window = nullptr;
	vk::raii::Context context;
    vk::raii::Instance instance = nullptr;

    vk::raii::PhysicalDevice physicalDevice = nullptr;
	vk::raii::Device device = nullptr;

	vk::raii::Queue graphicsQueue = nullptr;

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
        pickPhysicalDevice();
		createLogicalDevice();
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
        bool supportsGraphics = std::ranges::any_of(queueFamilyPropertiesVector, [&](auto const& queueFamilyProperties){
            return !!(queueFamilyProperties.queueFlags & vk::QueueFlagBits::eGraphics);
        });

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
		auto queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
		auto graphicsQueueIter = std::ranges::find_if(queueFamilyProperties, [&](auto const& queueFamily){
			return !!(queueFamily.queueFlags & vk::QueueFlagBits::eGraphics);
		});
		auto graphicsQueueIndex = static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueIter));
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
