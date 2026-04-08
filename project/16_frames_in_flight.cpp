#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <ranges>
#include <fstream>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#define GLFW_INCLUDE_VULKAN        // REQUIRED only for GLFW CreateWindowSurface.
#include <GLFW/glfw3.h>

constexpr uint32_t WIDTH  = 800;
constexpr uint32_t HEIGHT = 600;
constexpr float queuePriority = 1.0;
constexpr int MAX_FRAMES_IN_FLIGHT = 2;

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
	uint32_t graphicsQueueIndex = 0;

    vk::raii::SwapchainKHR swapChain = nullptr;
    std::vector<vk::Image> swapChainImages;
	std::vector<vk::raii::ImageView> swapChainImageViews;
    vk::SurfaceFormatKHR   swapChainSurfaceFormat;
    vk::Extent2D           swapChainExtent;

	vk::raii::PipelineLayout pipelineLayout = nullptr;
    vk::raii::Pipeline graphicsPipeline = nullptr;
    vk::raii::CommandPool commandPool = nullptr;
	std::vector<vk::raii::CommandBuffer> commandBuffers;

	std::vector<vk::raii::Semaphore> imageAvailableSemaphores;
	std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
	std::vector<vk::raii::Fence> preFrameFences;

	uint32_t frameIndex = 0;


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
		createImageViews();
        createGraphicsPipeline();
        createCommandPool();
		createCommandBuffers();
		createSyncObjects();
	}

	void mainLoop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
			drawFrame();
		}
		device.waitIdle();
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
															vk::PhysicalDeviceVulkan11Features,
                                                            vk::PhysicalDeviceVulkan13Features,
                                                            vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
        bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
										features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
										features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
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

		graphicsQueueIndex = ~0;
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

		vk::DeviceQueueCreateInfo deviceQueueCreateInfo = {
			.queueFamilyIndex = graphicsQueueIndex,
			.queueCount = 1,
			.pQueuePriorities = &queuePriority,
		};

		// feature
		vk::StructureChain
		<
			vk::PhysicalDeviceFeatures2,
			vk::PhysicalDeviceVulkan11Features,
			vk::PhysicalDeviceVulkan13Features,
			vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
		> featureChain = {
			{},
			{.shaderDrawParameters = true},
			{.synchronization2 = true, .dynamicRendering = true},
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

    auto readShader(const char* path) -> std::vector<std::byte> {
		std::ifstream shaderFile(path, std::ios::ate | std::ios::binary);
		if(!shaderFile) {
			throw std::runtime_error("cannot open shaderFile!");
		}

		std::vector<std::byte> buf(shaderFile.tellg());
		shaderFile.seekg(0);

		shaderFile.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));

		return buf;
    }

    [[nodiscard]] auto createShaderModule(const std::vector<std::byte>& code) -> vk::raii::ShaderModule
    {
        vk::ShaderModuleCreateInfo createInfo = {
            .codeSize = code.size() * sizeof(std::byte),
            .pCode = reinterpret_cast<const uint32_t*>(code.data())
        };
        vk::raii::ShaderModule shaderModule = vk::raii::ShaderModule(device, createInfo);

        return shaderModule;

    }
    void createGraphicsPipeline()
    {
		// shader module
        #ifdef _WIN32
		    auto shaderModule = createShaderModule(readShader("../shaders/slang.spv"));
        #elif __linux__
            auto shaderModule = createShaderModule(readShader("shaders/slang.spv"));
        #endif

		vk::PipelineShaderStageCreateInfo vertCreateInfo = {
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = shaderModule,
            .pName = "vertMain"
        };
        vk::PipelineShaderStageCreateInfo fragCreateInfo = {
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = shaderModule,
            .pName = "fragMain"
        };

		vk::PipelineShaderStageCreateInfo shaderStages[] = {vertCreateInfo, fragCreateInfo};

		// vertex input
		vk::PipelineVertexInputStateCreateInfo vertexInput = {
			.pVertexBindingDescriptions = nullptr,
			.pVertexAttributeDescriptions = nullptr
		};

		// input assemble
		vk::PipelineInputAssemblyStateCreateInfo inputAssembly = {
			.topology = vk::PrimitiveTopology::eTriangleList,
			.primitiveRestartEnable = vk::False
		};

		// viewport and scissor
		vk::Viewport viewport{0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f};
		vk::Rect2D scissor{vk::Offset2D{0, 0}, swapChainExtent};
		vk::PipelineViewportStateCreateInfo viewportState = {
			.viewportCount = 1,
			.pViewports = &viewport,
			.scissorCount = 1,
			.pScissors = &scissor
		};

		// rasterizer
		vk::PipelineRasterizationStateCreateInfo rasterizer = {
			.depthClampEnable = vk::False,
			.rasterizerDiscardEnable = vk::False,
			.polygonMode = vk::PolygonMode::eFill,
			.cullMode = vk::CullModeFlagBits::eBack,
			.frontFace = vk::FrontFace::eClockwise,
			.depthBiasEnable = vk::False,
			.lineWidth = 1.0f,
		};

		// multisampling
		vk::PipelineMultisampleStateCreateInfo multisampling = {
			.rasterizationSamples = vk::SampleCountFlagBits::e1,
			.sampleShadingEnable = vk::False,
		};

		// depth and stencil
		vk::PipelineDepthStencilStateCreateInfo depthStencil = {
			.depthTestEnable = vk::False,
			.depthWriteEnable = vk::False,
			.depthCompareOp = vk::CompareOp::eAlways,
			.depthBoundsTestEnable = vk::False,
			.stencilTestEnable = vk::False,
		};

		// color blend & logic op
		vk::PipelineColorBlendAttachmentState colorBlendAttachment = {
			.blendEnable = vk::False,
			.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
		};
		vk::PipelineColorBlendStateCreateInfo colorBlendOp = {
			.logicOpEnable = vk::False,
			.logicOp = vk::LogicOp::eCopy,
			.attachmentCount = 1,
			.pAttachments = & colorBlendAttachment
		};

		// pipeline layout
		vk::PipelineLayoutCreateInfo pipelineLayoutInfo{.setLayoutCount = 0, .pushConstantRangeCount = 0};
		pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

        // dynamic rendering
        vk::PipelineRenderingCreateInfo renderingInfo = {
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &swapChainSurfaceFormat.format,
        };

        vk::GraphicsPipelineCreateInfo pipelineInfo = {
            .pNext = &renderingInfo,
            .stageCount = 2,
            .pStages = shaderStages,
            .pVertexInputState = &vertexInput,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = &depthStencil,
            .pColorBlendState = &colorBlendOp,
            .layout = pipelineLayout,
            .renderPass = nullptr, // dynamic rendering does not use render pass}
        };

        graphicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineInfo);
    }

	void createCommandPool()
    {
		vk::CommandPoolCreateInfo poolInfo = {
			.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
			.queueFamilyIndex = graphicsQueueIndex
		};
		commandPool = vk::raii::CommandPool(device, poolInfo);
    }

	void createCommandBuffers()
	{
		commandBuffers.reserve(MAX_FRAMES_IN_FLIGHT);
		vk::CommandBufferAllocateInfo bufferInfo = {
			.commandPool = commandPool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = MAX_FRAMES_IN_FLIGHT
		};
		commandBuffers = vk::raii::CommandBuffers(device, bufferInfo);
	}

	void createSyncObjects()
	{
        assert(imageAvailableSemaphores.empty() && renderFinishedSemaphores.empty() && preFrameFences.empty());

        // imageAvailableSemaphores & preFrameFences
		vk::FenceCreateInfo fenceCreateInfo = {
			.flags = vk::FenceCreateFlagBits::eSignaled
		};
		imageAvailableSemaphores.reserve(MAX_FRAMES_IN_FLIGHT);
		preFrameFences.reserve(MAX_FRAMES_IN_FLIGHT);
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			imageAvailableSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
			preFrameFences.emplace_back(device, fenceCreateInfo);
		}

        // renderFinishedSemaphores
		renderFinishedSemaphores.reserve(swapChainImages.size());
		for (size_t i = 0; i < swapChainImages.size(); ++i)
		{
			renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
		}
	}

	void transition_image_layout(
	    uint32_t                imageIndex,
	    vk::ImageLayout         old_layout,
	    vk::ImageLayout         new_layout,
	    vk::AccessFlags2        src_access_mask,
	    vk::AccessFlags2        dst_access_mask,
	    vk::PipelineStageFlags2 src_stage_mask,
	    vk::PipelineStageFlags2 dst_stage_mask)
	{
			vk::ImageMemoryBarrier2 barrier = {
				.srcStageMask        = src_stage_mask,
				.srcAccessMask       = src_access_mask,
				.dstStageMask        = dst_stage_mask,
				.dstAccessMask       = dst_access_mask,
				.oldLayout           = old_layout,
				.newLayout           = new_layout,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image               = swapChainImages[imageIndex],
				.subresourceRange    = {
					.aspectMask     = vk::ImageAspectFlagBits::eColor,
					.baseMipLevel   = 0,
					.levelCount     = 1,
					.baseArrayLayer = 0,
					.layerCount     = 1}};
			vk::DependencyInfo dependencyInfo = {
				.dependencyFlags         = {},
				.imageMemoryBarrierCount = 1,
				.pImageMemoryBarriers    = &barrier};
		commandBuffers[frameIndex].pipelineBarrier2(dependencyInfo);
	}

	void recordCommandBuffer(uint32_t imageIndex)
	{
		// set attachment & rendering
		vk::RenderingAttachmentInfo attachmentInfo = {
			.imageView = swapChainImageViews[imageIndex],
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f)
		};

		vk::RenderingInfo renderingInfo = {
			.renderArea = {
				.offset = vk::Offset2D{0, 0},
				.extent = swapChainExtent
			},
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &attachmentInfo,
		};

		// begin
		commandBuffers[frameIndex].begin({});

		// layout transition
		transition_image_layout(imageIndex, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
			{}, vk::AccessFlagBits2::eColorAttachmentWrite,
			vk::PipelineStageFlagBits2::eTopOfPipe, vk::PipelineStageFlagBits2::eColorAttachmentOutput);

		// render pass
		commandBuffers[frameIndex].beginRendering(renderingInfo);
		commandBuffers[frameIndex].bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
		commandBuffers[frameIndex].draw(3, 1, 0, 0);
		commandBuffers[frameIndex].endRendering();

		transition_image_layout(imageIndex, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
			vk::AccessFlagBits2::eColorAttachmentWrite, {},
			vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eBottomOfPipe);

		commandBuffers[frameIndex].end();
	}

	void drawFrame()
	{
		// wait for pre frame
		auto result = device.waitForFences(*preFrameFences[frameIndex], vk::True, std::numeric_limits<uint64_t>::max());
		if(result != vk::Result::eSuccess) {
			std::runtime_error("device cant wait fence");
		}
		device.resetFences(*preFrameFences[frameIndex]);

		//acquire swap chain image
		auto [acquireResult, imageIndex] = swapChain.acquireNextImage(
			std::numeric_limits<uint64_t>::max(),
			*imageAvailableSemaphores[frameIndex],
			nullptr);
		if(acquireResult != vk::Result::eSuccess) {
			std::runtime_error("cant get next image from swap chain");
		}
		auto& renderFinishedSemaphore = renderFinishedSemaphores[imageIndex];

		// record cmd buffer
		recordCommandBuffer(imageIndex);

		// submit cmd buffer
		vk::PipelineStageFlags waitStage(vk::PipelineStageFlagBits::eColorAttachmentOutput);
		vk::SubmitInfo submitInfo = {
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &*imageAvailableSemaphores[frameIndex],
			.pWaitDstStageMask = &waitStage,
			.commandBufferCount = 1,
			.pCommandBuffers = &*commandBuffers[frameIndex],
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &*renderFinishedSemaphores[imageIndex],
		};
		graphicsQueue.submit(submitInfo, *preFrameFences[frameIndex]);

		// present
		vk::PresentInfoKHR presentInfo = {
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &*renderFinishedSemaphore,
			.swapchainCount = 1,
			.pSwapchains = &*swapChain,
			.pImageIndices = &imageIndex
		};
		graphicsQueue.presentKHR(presentInfo);

		frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
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

	void createImageViews()
	{
		vk::ImageViewCreateInfo imageViewCreateInfo = {
			.viewType = vk::ImageViewType::e2D,
			.format = swapChainSurfaceFormat.format,
			.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
		};

		for(const auto& image : swapChainImages) {
			imageViewCreateInfo.image = image;
			swapChainImageViews.emplace_back(device, imageViewCreateInfo);
		}
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
