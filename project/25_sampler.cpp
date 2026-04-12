#define STB_IMAGE_IMPLEMENTATION // function body
#include <stb_image.h> // only function prototypes
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <ranges>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

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

struct Vertex
{
	glm::vec2 pos;
	glm::vec3 color;

	static vk::VertexInputBindingDescription getBindingDescription()
	{
		vk::VertexInputBindingDescription bindingDescription {
			.binding = 0,
			.stride = sizeof(Vertex),
			.inputRate = vk::VertexInputRate::eVertex
		};
		return bindingDescription;
	}

	static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescription()
	{
		return {{
			{.location = 0, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = offsetof(Vertex, pos)},
			{.location = 1, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, color)}
		}};
	}
};

constexpr std::array<Vertex, 4> vertices = {{
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
}};

constexpr std::array<uint16_t, 6> indices = {{
	0, 1, 2, 2, 3, 0
}};

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

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
	bool framebufferResized = false;
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

	vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
	vk::raii::DescriptorPool descriptorPool =nullptr;
	std::vector<vk::raii::DescriptorSet> descriptorSets;

	vk::raii::PipelineLayout pipelineLayout = nullptr;
    vk::raii::Pipeline graphicsPipeline = nullptr;
    vk::raii::CommandPool commandPool = nullptr;
	std::vector<vk::raii::CommandBuffer> commandBuffers;

	vk::raii::Buffer vertexBuffer = nullptr;
	vk::raii::DeviceMemory vertexMemory = nullptr;
	vk::raii::Buffer indexBuffer = nullptr;
	vk::raii::DeviceMemory indexMemory = nullptr;

	vk::raii::Image textureImage = nullptr;
	vk::raii::DeviceMemory textureMemory = nullptr;
	vk::raii::ImageView textureImageView = nullptr;
	vk::raii::Sampler textureImageSampler = nullptr;

	std::vector<vk::raii::Buffer> uniformBuffers;
	std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
	std::vector<void*> uniformBuffersMapped;

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
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	}

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
	{
		auto appPointer = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
		appPointer -> framebufferResized = true;
	}


    void createInstance()
    {
        constexpr vk::ApplicationInfo appInfo {
            .pApplicationName = "Hello Triangle",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "No Engine",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = vk::ApiVersion14
        };

		uint32_t glfwExtensionCount = 0;
		auto     glfwExtensions     = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		vk::InstanceCreateInfo createInfo {
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
		createDescriptorLayout();
        createGraphicsPipeline();
        createCommandPool();
		createTextureImage();
		createTextureImageView();
		createTextureSampler();
		createVertexBuffer();
		createIndexBuffer();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();
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

	void cleanupSwapChain()
	{
		swapChainImageViews.clear();
		renderFinishedSemaphores.clear();
		swapChainImages.clear();
		swapChain = nullptr;
	}

	void recreateSwapChain()
	{
		int width = 0, height = 0;
		glfwGetFramebufferSize(window, &width, &height);
		while (width == 0 || height == 0)
		{
			glfwWaitEvents();
			glfwGetFramebufferSize(window, &width, &height);
		}

		device.waitIdle();

		cleanupSwapChain();
		createSwapChain();
		createImageViews();
		createRenderFinishedSemaphores();
		framebufferResized = false;
	}

    bool isDeviceSuitable(vk::raii::PhysicalDevice const& physicalDevice)
    {
        // API check
        bool supportsVulkan1_4 = physicalDevice.getProperties().apiVersion >= vk::ApiVersion14;

        // Queue family check
        auto queueFamilyPropertiesVector = physicalDevice.getQueueFamilyProperties();

        bool supportsGraphics = false;
		for (auto [index, queueFamilyProperties] : queueFamilyPropertiesVector | std::views::enumerate) {
			if (!!(queueFamilyProperties.queueFlags & vk::QueueFlagBits::eGraphics) && physicalDevice.getSurfaceSupportKHR(
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
        bool supportsAllRequiredExtensions = std::ranges::all_of (
            requiredDeviceExtension, [&availableDeviceExtensions](auto const& requiredDeviceExtension){
                return std::ranges::any_of (
                    availableDeviceExtensions, [&requiredDeviceExtension](auto const& availableDeviceExtension){
                        return strcmp( availableDeviceExtension.extensionName, requiredDeviceExtension ) == 0;
                    });
            });
        // features check
        auto features = physicalDevice.template getFeatures2 <vk::PhysicalDeviceFeatures2,
															vk::PhysicalDeviceVulkan11Features,
                                                            vk::PhysicalDeviceVulkan13Features,
                                                            vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
        bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
										features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
										features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
										features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
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
        if (iter == physicalDevices.end()) {
            throw std::runtime_error("has no availiable physical device!");
        }

        physicalDevice = *iter;
    }

	void createLogicalDevice()
	{
		// queue
		auto queueFamilyPropertiesVector = physicalDevice.getQueueFamilyProperties();

		graphicsQueueIndex = ~0;
		for (auto [index, queueFamilyProperties] : queueFamilyPropertiesVector | std::views::enumerate) {
			if(!!(queueFamilyProperties.queueFlags & vk::QueueFlagBits::eGraphics) && physicalDevice.getSurfaceSupportKHR(
				static_cast<uint32_t>(index),
				surface
			)) {
				graphicsQueueIndex = static_cast<uint32_t>(index);
				break;
			}
		}
		if (graphicsQueueIndex == ~0) {
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
		> featureChain;
		featureChain.get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy = true;
		featureChain.get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters = true;
		featureChain.get<vk::PhysicalDeviceVulkan13Features>().synchronization2 = true;
		featureChain.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering = true;
		featureChain.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState = true;

		// create
		vk::DeviceCreateInfo deviceCreateInfo = {
			.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
			.queueCreateInfoCount = 1,
			.pQueueCreateInfos = &deviceQueueCreateInfo,
			.enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtension.size()),
			.ppEnabledExtensionNames = requiredDeviceExtension.data(),
			.pEnabledFeatures = nullptr,
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

	void createDescriptorLayout()
	{
		// descriptor layout binding
		vk::DescriptorSetLayoutBinding uboLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex);
		vk::DescriptorSetLayoutCreateInfo layoutInfo = {
			.bindingCount = 1,
			.pBindings = &uboLayoutBinding
		};
		descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
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
		auto vertexBindingDescriptions = Vertex::getBindingDescription();
		auto vertexAttributeDescriptions = Vertex::getAttributeDescription();
		vk::PipelineVertexInputStateCreateInfo vertexInput = {
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &vertexBindingDescriptions,
			.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributeDescriptions.size()),
			.pVertexAttributeDescriptions = vertexAttributeDescriptions.data()
		};

		// input assemble
		vk::PipelineInputAssemblyStateCreateInfo inputAssembly = {
			.topology = vk::PrimitiveTopology::eTriangleList,
			.primitiveRestartEnable = vk::False
		};

		// viewport and scissor
		std::vector<vk::DynamicState>      dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
		vk::PipelineDynamicStateCreateInfo dynamicState{
			.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
			.pDynamicStates = dynamicStates.data()
		};
		vk::PipelineViewportStateCreateInfo      viewportState{.viewportCount = 1, .scissorCount = 1};

		// rasterizer
		vk::PipelineRasterizationStateCreateInfo rasterizer = {
			.depthClampEnable = vk::False,
			.rasterizerDiscardEnable = vk::False,
			.polygonMode = vk::PolygonMode::eFill,
			.cullMode = vk::CullModeFlagBits::eBack,
			.frontFace = vk::FrontFace::eCounterClockwise,
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
		vk::PipelineLayoutCreateInfo pipelineLayoutInfo{.setLayoutCount = 1, .pSetLayouts = &*descriptorSetLayout};
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
            .pDynamicState = &dynamicState,
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

	uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
	{
		vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
		for(uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
		{
			if ((typeFilter & (1 << i)) && ((memProperties.memoryTypes[i].propertyFlags & properties) == properties)) {
				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type");
	}

	void createBuffer(
		vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties,
		vk::SharingMode sharingMode, vk::raii::Buffer& buffer, vk::raii::DeviceMemory& memory
	)
	{
		vk::BufferCreateInfo bufferCreateInfo = {
			.size = size,
			.usage = usage,
			.sharingMode = sharingMode
		};
		buffer = vk::raii::Buffer(device, bufferCreateInfo);

		vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
		vk::MemoryAllocateInfo memoryAllocateInfo = {
			.allocationSize = memRequirements.size,
			.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)
		};

		memory = vk::raii::DeviceMemory(device, memoryAllocateInfo);
		buffer.bindMemory(*memory, 0);
	}

	vk::raii::CommandBuffer beginSingleTimeCommands() 
	{
		vk::CommandBufferAllocateInfo cmdCopyInfo = {
			.commandPool = commandPool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = 1,
		};
		vk::raii::CommandBuffer commandBuffer = std::move(device.allocateCommandBuffers(cmdCopyInfo).front());
		vk::CommandBufferBeginInfo beginInfo = {
			.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
		};
		commandBuffer.begin(beginInfo);
		return commandBuffer;
	}

	void endSingleTimeCommands(vk::raii::CommandBuffer& commandBuffer)
	{	
		commandBuffer.end();
		graphicsQueue.submit(vk::SubmitInfo{ .commandBufferCount = 1, .pCommandBuffers = &*commandBuffer }, nullptr);
		graphicsQueue.waitIdle();
	}

	void transitionImageLayoutSwapChain(
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

	void transitionImageLayout(const vk::raii::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
	{
		auto commandBuffer = beginSingleTimeCommands();

		vk::PipelineStageFlagBits2 srcStageMask;
		vk::PipelineStageFlagBits2 dstStageMask;
		vk::AccessFlagBits2 srcAccessMask;
		vk::AccessFlagBits2 dstAccessMask;
		if(oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
		{
			srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
			dstStageMask = vk::PipelineStageFlagBits2::eTransfer;

			srcAccessMask = vk::AccessFlagBits2::eNone;
			dstAccessMask = vk::AccessFlagBits2::eTransferWrite;
		} 
		else if(oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
		{
			srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
			dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;

			srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
			dstAccessMask = vk::AccessFlagBits2::eShaderRead;
		}
		else
		{
			throw std::invalid_argument("unsupported ImageLayout for transition!");
		}

		vk::ImageMemoryBarrier2 barrier = {
			.srcStageMask        = srcStageMask,
			.srcAccessMask       = srcAccessMask,
			.dstStageMask        = dstStageMask,
			.dstAccessMask       = dstAccessMask,
			.oldLayout           = oldLayout,
			.newLayout           = newLayout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image               = image,
			.subresourceRange    = {
				.aspectMask     = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel   = 0,
				.levelCount     = 1,
				.baseArrayLayer = 0,
				.layerCount     = 1
			}
		};
		vk::DependencyInfo dependencyInfo = {
			.dependencyFlags         = {},
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers    = &barrier
		};
		commandBuffer.pipelineBarrier2(dependencyInfo);

		endSingleTimeCommands(commandBuffer);
	}

	void copyBuffer(vk::raii::Buffer& srcBuffer, vk::raii::Buffer& dstBuffer, vk::DeviceSize size)
	{
		auto commandCopyBuffer = beginSingleTimeCommands();
		commandCopyBuffer.copyBuffer(srcBuffer, dstBuffer, vk::BufferCopy(0, 0, size));
		endSingleTimeCommands(commandCopyBuffer);
	}

	void copyBufferToImage(vk::raii::Buffer& srcBuffer, vk::raii::Image& dstImage, uint32_t width, uint32_t height)
	{
		auto commandBuffer = beginSingleTimeCommands();

		vk::BufferImageCopy2 region = {
			.bufferOffset = 0,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource = {
				vk::ImageAspectFlagBits::eColor,
				0, 0, 1
			},
			.imageOffset = {0, 0, 0},
			.imageExtent = {width, height, 1},
		};

		auto copyInfo = vk::CopyBufferToImageInfo2 {
			.srcBuffer = srcBuffer,
			.dstImage = dstImage,
			.dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
			.regionCount = 1,
			.pRegions = &region
		};
		commandBuffer.copyBufferToImage2(copyInfo);

		endSingleTimeCommands(commandBuffer);
	}

	void createTextureImage()
	{
		// load image
		int texWidth, texHeight, texChannels;
		stbi_uc* pixels = stbi_load("../textures/texture.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		vk::DeviceSize imageSize = static_cast<vk::DeviceSize>(texWidth) * static_cast<vk::DeviceSize>(texHeight) * 4;

		if(!pixels) {
			throw std::runtime_error("cannot load pixels from file");
		}

		// create staging buffer and fill it
		vk::raii::Buffer stagingBuffer = nullptr;
		vk::raii::DeviceMemory stagingMemory = nullptr;
		createBuffer(
			imageSize, 
			vk::BufferUsageFlagBits::eTransferSrc, 
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, 
			vk::SharingMode::eExclusive,
			stagingBuffer, 
			stagingMemory
		);
		
		void *data = stagingMemory.mapMemory(0, imageSize);
		memcpy(data, pixels, imageSize);
		stagingMemory.unmapMemory();

		stbi_image_free(pixels);

		// create image object
		vk::ImageCreateInfo imageInfo = {
			.imageType = vk::ImageType::e2D,
			.format = vk::Format::eR8G8B8A8Srgb,
			.extent = {static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1},
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = vk::SampleCountFlagBits::e1,
			.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
			.sharingMode = vk::SharingMode::eExclusive,
			.initialLayout = vk::ImageLayout::eUndefined
		};
		textureImage = vk::raii::Image(device, imageInfo);

		vk::MemoryRequirements memRequirements = textureImage.getMemoryRequirements();
		vk::MemoryAllocateInfo allocInfo = {
			.allocationSize = memRequirements.size,
			.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)
		};
		textureMemory = vk::raii::DeviceMemory(device, allocInfo);
		textureImage.bindMemory(*textureMemory, 0);

		// copy buffer to image
		transitionImageLayout(textureImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
		copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

		// transfer layout to shader read
		transitionImageLayout(textureImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

	}

	void createTextureSampler()
	{
		vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();
		vk::SamplerCreateInfo samplerInfo{
			.magFilter = vk::Filter::eLinear, 
			.minFilter = vk::Filter::eLinear,  
			.mipmapMode = vk::SamplerMipmapMode::eLinear,
			.addressModeU = vk::SamplerAddressMode::eRepeat, 
			.addressModeV = vk::SamplerAddressMode::eRepeat,
			.anisotropyEnable = vk::True, 
			.maxAnisotropy = properties.limits.maxSamplerAnisotropy,
		};
		textureImageSampler = vk::raii::Sampler(device, samplerInfo);
	}

	void createVertexBuffer()
	{
		// staging buffer
		constexpr vk::DeviceSize stagingBufferSize = sizeof(vertices);
		vk::raii::Buffer stagingBuffer = nullptr;
		vk::raii::DeviceMemory stagingMemory = nullptr;
		createBuffer(
			stagingBufferSize,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			vk::SharingMode::eExclusive,
			stagingBuffer,
			stagingMemory
		);

		auto data = stagingMemory.mapMemory(0, stagingBufferSize);
		memcpy(data, vertices.data(), static_cast<size_t>(stagingBufferSize));
		stagingMemory.unmapMemory();

		// final vertex buffer
		constexpr vk::DeviceSize vertexBufferSize = sizeof(vertices);
		createBuffer(
			vertexBufferSize,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			vk::SharingMode::eExclusive,
			vertexBuffer,
			vertexMemory
		);

		// copy
		copyBuffer(stagingBuffer, vertexBuffer, stagingBufferSize);
	}

	void createIndexBuffer()
	{
		// staging buffer
		constexpr vk::DeviceSize stagingBufferSize = sizeof(indices);
		vk::raii::Buffer stagingBuffer = nullptr;
		vk::raii::DeviceMemory stagingMemory = nullptr;
		createBuffer(
			stagingBufferSize,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			vk::SharingMode::eExclusive,
			stagingBuffer,
			stagingMemory
		);

		auto data = stagingMemory.mapMemory(0, stagingBufferSize);
		memcpy(data, indices.data(), static_cast<size_t>(stagingBufferSize));
		stagingMemory.unmapMemory();

		// final vertex buffer
		constexpr vk::DeviceSize indexBufferSize = sizeof(indices);
		createBuffer(
			indexBufferSize,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			vk::SharingMode::eExclusive,
			indexBuffer,
			indexMemory
		);

		// copy
		copyBuffer(stagingBuffer, indexBuffer, stagingBufferSize);
	}

	void createUniformBuffers()
	{
		uniformBuffers.clear();
		uniformBuffersMemory.clear();
		uniformBuffersMapped.clear();

		for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vk::raii::Buffer buffer = nullptr;
			vk::raii::DeviceMemory memory = nullptr;
			createBuffer(
				sizeof(UniformBufferObject),
				vk::BufferUsageFlagBits::eUniformBuffer,
				vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
				vk::SharingMode::eExclusive,
				buffer,
				memory
			);
			uniformBuffers.emplace_back(std::move(buffer));
			uniformBuffersMemory.emplace_back(std::move(memory));
			uniformBuffersMapped.emplace_back(uniformBuffersMemory[i].mapMemory(0, sizeof(UniformBufferObject)));
		}
	}

	void createDescriptorPool()
	{
		vk::DescriptorPoolSize descriptorPoolSize = {
			.type = vk::DescriptorType::eUniformBuffer,
			.descriptorCount = MAX_FRAMES_IN_FLIGHT
		};
		vk::DescriptorPoolCreateInfo poolInfo = {
			.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
			.maxSets = MAX_FRAMES_IN_FLIGHT,
			.poolSizeCount = 1,
			.pPoolSizes = &descriptorPoolSize
		};
		descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
	}

	void createDescriptorSets()
	{
		// allocate sets from pool
		std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);
		vk::DescriptorSetAllocateInfo allocateInfo = {
			.descriptorPool = descriptorPool,
			.descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
			.pSetLayouts = layouts.data()
		};
		descriptorSets.clear();
		descriptorSets = device.allocateDescriptorSets(allocateInfo);

		// specify buffer for the every descriptor
		for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vk::DescriptorBufferInfo bufferInfo = {
				.buffer = *uniformBuffers[i],
				.offset = 0,
				.range = sizeof(UniformBufferObject)
			};

			vk::WriteDescriptorSet descriptorWrite{
				.dstSet          = *descriptorSets[i],
				.dstBinding      = 0,                                 // 对应 shader 里 binding = 0
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType  = vk::DescriptorType::eUniformBuffer,
				.pBufferInfo     = &bufferInfo
			};

			device.updateDescriptorSets(descriptorWrite, nullptr);
		}
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
		createRenderFinishedSemaphores();
	}

	void createRenderFinishedSemaphores()
	{
		renderFinishedSemaphores.clear();
		renderFinishedSemaphores.reserve(swapChainImages.size());
		for (size_t i = 0; i < swapChainImages.size(); ++i)
		{
			renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
		}
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
		transitionImageLayoutSwapChain(imageIndex, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
			{}, vk::AccessFlagBits2::eColorAttachmentWrite,
			vk::PipelineStageFlagBits2::eTopOfPipe, vk::PipelineStageFlagBits2::eColorAttachmentOutput);

		// render pass
		commandBuffers[frameIndex].beginRendering(renderingInfo);
		commandBuffers[frameIndex].bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
		commandBuffers[frameIndex].setViewport(0, vk::Viewport{0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f});
		commandBuffers[frameIndex].setScissor(0, vk::Rect2D{vk::Offset2D{0, 0}, swapChainExtent});
		commandBuffers[frameIndex].bindVertexBuffers(0, *vertexBuffer, {0});
		commandBuffers[frameIndex].bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint16);
		commandBuffers[frameIndex].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, *descriptorSets[frameIndex], nullptr);
		commandBuffers[frameIndex].drawIndexed(indices.size(), 1, 0, 0, 0);
		commandBuffers[frameIndex].endRendering();

		transitionImageLayoutSwapChain(imageIndex, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
			vk::AccessFlagBits2::eColorAttachmentWrite, {},
			vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eBottomOfPipe);

		commandBuffers[frameIndex].end();
	}

	void updateUniformBuffer(uint32_t currentFrame)
	{
		static auto startTime = std::chrono::high_resolution_clock::now();

		auto  currentTime = std::chrono::high_resolution_clock::now();
		float time        = std::chrono::duration<float>(currentTime - startTime).count();

		UniformBufferObject ubo{};
		ubo.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.view  = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.proj  = glm::perspective(glm::radians(45.0f), static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height), 0.1f, 10.0f);
		ubo.proj[1][1] *= -1;

		memcpy(uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));
	}

	void drawFrame()
	{
		// wait for pre frame
		auto result = device.waitForFences(*preFrameFences[frameIndex], vk::True, std::numeric_limits<uint64_t>::max());
		if (result != vk::Result::eSuccess) {
			throw std::runtime_error("device cant wait fence");
		}

		//acquire swap chain image
		auto [acquireResult, imageIndex] = swapChain.acquireNextImage(
			std::numeric_limits<uint64_t>::max(),
			*imageAvailableSemaphores[frameIndex],
			nullptr);
		if (acquireResult == vk::Result::eErrorOutOfDateKHR) {
			recreateSwapChain();
			return;
		}
		if (acquireResult != vk::Result::eSuccess && acquireResult != vk::Result::eSuboptimalKHR) {
			throw std::runtime_error("failed to acquire image from swap chain");
		}

		updateUniformBuffer(frameIndex);
		device.resetFences(*preFrameFences[frameIndex]);

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
			.pWaitSemaphores = &*renderFinishedSemaphores[imageIndex],
			.swapchainCount = 1,
			.pSwapchains = &*swapChain,
			.pImageIndices = &imageIndex
		};
		auto presentResult = graphicsQueue.presentKHR(presentInfo);
		if (presentResult == vk::Result::eSuboptimalKHR || presentResult == vk::Result::eErrorOutOfDateKHR || framebufferResized) {
			recreateSwapChain();
		}
		else if (presentResult != vk::Result::eSuccess) {
			throw std::runtime_error("failed to present");
		}

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

	vk::raii::ImageView createImageView(const vk::Image& image, vk::ImageViewType viewType, vk::Format format, vk::ImageSubresourceRange range)
	{
		vk::ImageViewCreateInfo viewInfo = {
			.image = image,
			.viewType = viewType,
			.format = format,
			.subresourceRange = range
		};
		return vk::raii::ImageView(device, viewInfo);
	}

	void createTextureImageView()
	{
		textureImageView = createImageView(
			*textureImage, 
			vk::ImageViewType::e2D,
			vk::Format::eR8G8B8A8Srgb,
			{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
		);
	}

	void createImageViews()
	{
		swapChainImageViews.clear();
		swapChainImageViews.reserve(swapChainImages.size());
		for(const auto& image : swapChainImages) {
			swapChainImageViews.emplace_back(createImageView(
				image,
				vk::ImageViewType::e2D,
				swapChainSurfaceFormat.format,
				{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
			));
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
