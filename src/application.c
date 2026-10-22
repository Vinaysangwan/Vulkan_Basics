#include "application.h"
#include "logger.h"
#include "darray.h"
#include "utils.h"
#include "vma_c.h"

#include <stdlib.h>
#include <stdio.h>

#include <SDL3/SDL_vulkan.h>
#include <shaderc/shaderc.h>
#include <vulkan/vulkan_core.h>

static const VkFormat vulkan_swapchain_format = VK_FORMAT_B8G8R8A8_SRGB;
static const VkFormat vulkan_depth_format = VK_FORMAT_D32_SFLOAT;

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
  VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
  VkDebugUtilsMessageTypeFlagsEXT message_types,
  const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
  void* pUserData
) {
  if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
  {
    LOG_ERROR("Valiation Layer: %s", callback_data->pMessage);
  }

  return FALSE;
}

b8 vulkan_create_instance(Application *self)
{
  // init volk and load vk function ptrs
  if (volkInitialize() != VK_SUCCESS)
  {
    LOG_ERROR("Failed to Init volk");
    return FALSE;
  }

  VkApplicationInfo app_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app_info.pApplicationName = "Vulkan Basics";
  app_info.apiVersion = VULKAN_VERSION;

  u32 extension_count = 0;
  const char *const *extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);

  const char *required_extensions[extension_count + 1];
  u32 required_extension_count = 0;
  required_extensions[required_extension_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
  for (u32 i=0; i<extension_count; ++i)
  {
    required_extensions[required_extension_count++] = extensions[i];
  }

  LOG_INFO("Required Extensions are: ");
  for (u32 i=0; i<required_extension_count; ++i)
  {
    LOG_INFO("%s", required_extensions[i]);
  }

  // validation layers for error checking and reporting
  const char* request_layers[1] = {0};
  u32 request_layer_count = 0;
  request_layers[request_layer_count++] = "VK_LAYER_KHRONOS_validation";

  LOG_INFO("Requestion Validation Layers are: ");
  for (u32 i=0; i<request_layer_count; ++i)
  {
    LOG_INFO("%s", request_layers[i]);
  }

  VkDebugUtilsMessengerCreateInfoEXT debug_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
  debug_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;

  debug_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

  debug_info.pfnUserCallback = debug_callback;

  VkInstanceCreateInfo instance_create_info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  instance_create_info.pNext = &debug_info;
  instance_create_info.pApplicationInfo = &app_info;
  instance_create_info.enabledLayerCount = request_layer_count;
  instance_create_info.ppEnabledLayerNames = request_layers;
  instance_create_info.enabledExtensionCount = required_extension_count;
  instance_create_info.ppEnabledExtensionNames = required_extensions;

  VkResult result = vkCreateInstance(&instance_create_info, NULL, &self->vulkan_instance);
  if (result != VK_SUCCESS)
  {
    LOG_ERROR("Failed to Create Vulkan Instance (VkResult = %d)", result);
    return FALSE;
  }

  LOG_INFO("Vulkan Instance is Created");
  volkLoadInstance(self->vulkan_instance);
  return TRUE;
}

b8 vulkan_create_surface(Application* self)
{
  if (!SDL_Vulkan_CreateSurface(self->window, self->vulkan_instance, NULL, &self->vulkan_surface))
  {
    return FALSE;
  }

  LOG_INFO("Vulkan Surface is created");
  return TRUE;
}

VkPhysicalDevice vulkan_find_physical_device(Application *self)
{
  u32 physical_device_count = 0;
  vkEnumeratePhysicalDevices(self->vulkan_instance, &physical_device_count, NULL);
  VkPhysicalDevice physical_devices[physical_device_count];
  vkEnumeratePhysicalDevices(self->vulkan_instance, &physical_device_count, physical_devices);

  VkPhysicalDevice physical_device = 0;
  if (physical_device_count)
  {
    physical_device = physical_devices[0];

    for (u32 i=0; i<physical_device_count; ++i)
    {
      VkPhysicalDeviceProperties props = {};
      vkGetPhysicalDeviceProperties(physical_devices[i], &props);

      if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
      {
        physical_device = physical_devices[i];
        LOG_INFO("Vulkan Physical Device found");

        break;
      }
    }
  }

  // check if desired swapchian format is supported
  u32 format_count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, self->vulkan_surface, &format_count, NULL);
  VkSurfaceFormatKHR formats[format_count];
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, self->vulkan_surface, &format_count, formats);

  b8 format_supported = FALSE;
  for (u32 i=0; i<format_count; ++i)
  {
    if (formats[i].format == vulkan_swapchain_format)
    {
      format_supported = TRUE;
      break;
    }
  }
  if (!format_supported)
  {
    LOG_ERROR("Failed to Get Vulkan Deivce that supportes the desired Swapchain format");
  }

  return physical_device;
}

b8 vulkan_find_graphics_queue(Application *self)
{
  u32 queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties2(self->vulkan_physical_device, &queue_family_count, NULL);
  VkQueueFamilyProperties2 queue_family_props[queue_family_count];
  for (u32 i=0; i<queue_family_count; ++i)
  {
    queue_family_props[i] = (VkQueueFamilyProperties2){VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2};
  }
  vkGetPhysicalDeviceQueueFamilyProperties2(self->vulkan_physical_device, &queue_family_count, queue_family_props);
  
  for (u32 i=0; i<queue_family_count; ++i)
  {
    VkBool32 has_present_support = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(self->vulkan_physical_device, i, self->vulkan_surface, &has_present_support);

    const VkQueueFamilyProperties2 *prop = &queue_family_props[i];
    if (prop->queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT && has_present_support)
    {
      self->vulkan_graphics_queue_family_idx = i;
      LOG_INFO("Vulkan Graphics Queue index is found.");

      return TRUE;
    }
  }
  
  return FALSE;
}

b8 vulkan_create_device(Application *self)
{
  // query supported features
  VkPhysicalDeviceVulkan14Features supported_features_14 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES};

  VkPhysicalDeviceVulkan13Features supported_features_13 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
  supported_features_13.pNext = &supported_features_14;

  VkPhysicalDeviceVulkan12Features supported_features_12 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
  supported_features_12.pNext = &supported_features_13;

  VkPhysicalDeviceFeatures2 supported_features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
  supported_features.pNext = &supported_features_12;
  vkGetPhysicalDeviceFeatures2(self->vulkan_physical_device, &supported_features);

  if (!supported_features_13.dynamicRendering || 
      !supported_features_13.synchronization2 || 
      !supported_features_12.timelineSemaphore
  ){
    LOG_ERROR("Physical Device doesn't meet feature requirement");
    return FALSE;
  }
  LOG_INFO("Physical Device meet the feature required.");

  VkPhysicalDeviceVulkan14Features features_14 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES};
  
  VkPhysicalDeviceVulkan13Features features_13 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
  features_13.pNext = &features_14;
  features_13.synchronization2 = VK_TRUE;
  features_13.dynamicRendering = VK_TRUE;

  VkPhysicalDeviceVulkan12Features features_12 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
  features_12.pNext= &features_13;
  features_12.timelineSemaphore = VK_TRUE;

  VkPhysicalDeviceFeatures2 features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
  features.pNext = &features_12;

  f32 queue_props[1];
  u32 queue_prop_count = 0;
  queue_props[queue_prop_count++] = 1.0f;
  VkDeviceQueueCreateInfo graphics_queue_info = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
  graphics_queue_info.queueCount = queue_prop_count;
  graphics_queue_info.pQueuePriorities = queue_props;
  graphics_queue_info.queueFamilyIndex = self->vulkan_graphics_queue_family_idx;

  const char* device_extensions[3];
  u32 device_extension_count = 0;
  device_extensions[device_extension_count++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

  VkDeviceCreateInfo device_create_info = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  device_create_info.pNext = &features;
  device_create_info.enabledExtensionCount = device_extension_count;
  device_create_info.ppEnabledExtensionNames = device_extensions;
  device_create_info.queueCreateInfoCount = 1;
  device_create_info.pQueueCreateInfos = &graphics_queue_info;

  if (vkCreateDevice(self->vulkan_physical_device, &device_create_info, NULL, &self->vulkan_device) != VK_SUCCESS)
  {
    return FALSE;
  }
  LOG_INFO("Vulkan Device is Created.");

  // get graphics queue
  vkGetDeviceQueue(self->vulkan_device, self->vulkan_graphics_queue_family_idx, 0, &self->vulkan_graphics_queue);
  if (!self->vulkan_graphics_queue)
  {
    LOG_ERROR("Failed to Get Vulkan Graphics Queue");
    return FALSE;
  }
  LOG_INFO("Vulkan Graphics Queue have been found.");

  return TRUE;
}

b8 vulkan_init_vma(Application* self)
{
  VmaVulkanFunctions vma_func_info = {};
  VmaAllocatorCreateInfo vma_alloc_info = {};
  vma_alloc_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  vma_alloc_info.physicalDevice = self->vulkan_physical_device;
  vma_alloc_info.device = self->vulkan_device;
  vma_alloc_info.instance = self->vulkan_instance;
  vma_alloc_info.vulkanApiVersion = VULKAN_VERSION;
  vma_alloc_info.pVulkanFunctions = &vma_func_info;

  vmaImportVulkanFunctionsFromVolk(&vma_alloc_info, &vma_func_info);

  if (vmaCreateAllocator(&vma_alloc_info, &self->vulkan_vma_allocator) != VK_SUCCESS)
  {
    LOG_ERROR("Failed to Create Vulkan vma allocator");
    return FALSE;
  }
  LOG_INFO("Created Vulkan vma allocator.");
  
  return TRUE;
}

b8 vulkan_create_swapchain(Application* self, u32 width, u32 height)
{
  self->vulkan_swapchain_width = width;
  self->vulkan_swapchain_height = height;

  VkSurfaceCapabilitiesKHR surface_caps = {0};
  if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(self->vulkan_physical_device, self->vulkan_surface, &surface_caps) != VK_SUCCESS)
  {
    LOG_ERROR("Failed to Get Vulkan Physical Device Surface Capabilities");
    return FALSE;
  }
  LOG_INFO("Vulkan Physical Device Suface Capabilities are found.");

  u32 request_image_count = SDL_max(2, surface_caps.minImageCount);
  if (surface_caps.maxImageCount > 0)
  {
    request_image_count = SDL_min(request_image_count, surface_caps.maxImageCount);
  }

  VkSwapchainCreateInfoKHR swapchain_create_info = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
  swapchain_create_info.surface = self->vulkan_surface;
  swapchain_create_info.minImageCount = request_image_count;
  swapchain_create_info.imageFormat = vulkan_swapchain_format;
  swapchain_create_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  swapchain_create_info.imageExtent = (VkExtent2D) {
    .width = self->vulkan_swapchain_width,
    .height = self->vulkan_swapchain_height,
  };
  swapchain_create_info.imageArrayLayers = 1;
  swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchain_create_info.preTransform = surface_caps.currentTransform;
  swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchain_create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;

  if (vkCreateSwapchainKHR(self->vulkan_device, &swapchain_create_info, NULL, &self->vulkan_swapchain) != VK_SUCCESS)
  {
    return FALSE;
  }
  LOG_INFO("Vulkan Swapchin is Created.");

  // get swapchain images
  u32 image_count = 0;
  vkGetSwapchainImagesKHR(self->vulkan_device, self->vulkan_swapchain, &image_count, NULL);
  self->vulkan_swapchain_images = darray_resize(VkImage, image_count);
  vkGetSwapchainImagesKHR(self->vulkan_device, self->vulkan_swapchain, &image_count, self->vulkan_swapchain_images);
  self->vulkan_swapchain_image_views = darray_resize(VkImageView, image_count);

  // create swapchain image view
  for (u32 i=0; i<image_count; ++i)
  {
    VkImageViewCreateInfo image_view_create_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    image_view_create_info.image = self->vulkan_swapchain_images[i];
    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_info.format = vulkan_swapchain_format;
    image_view_create_info.subresourceRange = (VkImageSubresourceRange) {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1 
    };

    if (vkCreateImageView(self->vulkan_device, &image_view_create_info, NULL, &self->vulkan_swapchain_image_views[i]) != VK_SUCCESS)
    {
      LOG_ERROR("Failed to Create Vulkan Swapchain Image View");
      return FALSE;
    }
  }
  LOG_INFO("Vulkan Swapchian Image Views are Created.");

  // creating semaphores which is used to signal render completion
  self->vulkan_swapchain_render_complete_semaphores = darray_resize(VkSemaphore, image_count);
  for (u32 i=0; i<image_count; ++i)
  {
    VkSemaphoreCreateInfo semaphore_create_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    if (vkCreateSemaphore(
      self->vulkan_device, 
      &semaphore_create_info, 
      NULL, 
      &self->vulkan_swapchain_render_complete_semaphores[i]
    ) != VK_SUCCESS)
    {
      LOG_ERROR("Failed to Create Vulkan swapchain Semaphores");
      return FALSE;
    }
  }
  LOG_INFO("Vulkan Swapchian Semaphores are Created.");

  // create depth image
  VkImageCreateInfo depth_create_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  depth_create_info.imageType = VK_IMAGE_TYPE_2D;
  depth_create_info.format = vulkan_depth_format;
  depth_create_info.extent = (VkExtent3D){
    .depth = 1,
    .width = self->vulkan_swapchain_width,
    .height = self->vulkan_swapchain_height
  };
  depth_create_info.mipLevels = 1;
  depth_create_info.arrayLayers = 1;
  depth_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
  depth_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  depth_create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  depth_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VmaAllocationCreateInfo alloc_create_info = {0};
  alloc_create_info.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO;

  if (vmaCreateImage(
    self->vulkan_vma_allocator,
    &depth_create_info,
    &alloc_create_info,
    &self->vulkan_depth_image,
    &self->vulkan_depth_vma_allocator,
    NULL
  ) != VK_SUCCESS)
  {
    LOG_ERROR("Faile to Create Vulkan Depth Image");
    return FALSE;
  }
  LOG_INFO("Vulkan Depth Image is Created");

  VkImageViewCreateInfo depth_image_view_create_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  depth_image_view_create_info.image = self->vulkan_depth_image;
  depth_image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  depth_image_view_create_info.format = vulkan_depth_format;
  depth_image_view_create_info.subresourceRange = (VkImageSubresourceRange) {
    .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
    .levelCount = 1,
    .layerCount = 1
  };

  if (vkCreateImageView(
    self->vulkan_device,
    &depth_image_view_create_info,
    NULL,
    &self->vulkan_depth_image_view
  ) != VK_SUCCESS)
  {
    LOG_ERROR("Failed to Create Vulkan Depth Image View");
    return FALSE;
  }
  LOG_INFO("Vulkan Depth Image View is Created");

  return TRUE;
}

void vulkan_destroy_swapchain(Application* self)
{
  // swapchain image views cleanup
  for (u32 i=0; i<darray_len(self->vulkan_swapchain_image_views); ++i)
  {
    VkImageView *image_view = &self->vulkan_swapchain_image_views[i];
    vkDestroyImageView(self->vulkan_device, *image_view, NULL);
    *image_view = 0;
  }
  darray_destroy(self->vulkan_swapchain_image_views);

  // swapchain semaphores cleanup
  for (u32 i=0; i<darray_len(self->vulkan_swapchain_render_complete_semaphores); ++i)
  {
    VkSemaphore *semaphore = &self->vulkan_swapchain_render_complete_semaphores[i];
    vkDestroySemaphore(self->vulkan_device, *semaphore, NULL);
    *semaphore = 0;
  }
  darray_destroy(self->vulkan_swapchain_render_complete_semaphores);

  if (self->vulkan_swapchain)
  {
    vkDestroySwapchainKHR(self->vulkan_device, self->vulkan_swapchain, NULL);
    self->vulkan_swapchain = 0;
  }

  // depth image and image view cleanup
  if (self->vulkan_depth_image_view)
  {
    vkDestroyImageView(self->vulkan_device, self->vulkan_depth_image_view, NULL);
    vmaDestroyImage(self->vulkan_vma_allocator, self->vulkan_depth_image, self->vulkan_depth_vma_allocator);
    self->vulkan_depth_image_view = 0;
    self->vulkan_depth_image = 0;
    self->vulkan_depth_vma_allocator = 0;
  }
}

VkShaderModule application_create_shader_module(const Application* self, const char* file_name, shaderc_shader_kind kind)
{
  char shader_path[512] = {0};
  snprintf(shader_path, 512, "src/shaders/%s", file_name);
  u64 shader_len = 0;
  char* src = read_file(shader_path, &shader_len);
  if (!src)
  {
    return NULL;
  }

  // compile shader to SPIR-V
  LOG_INFO("Compiling Shader: %s...", file_name);
  shaderc_compiler_t compiler = shaderc_compiler_initialize();
  shaderc_compile_options_t opts = shaderc_compile_options_initialize();
  shaderc_compile_options_set_target_env(opts, shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_4);
  shaderc_compile_options_set_target_spirv(opts, shaderc_spirv_version_1_6);
  shaderc_compile_options_set_optimization_level(opts, shaderc_optimization_level_performance);
  shaderc_compilation_result_t result = shaderc_compile_into_spv(compiler, src, shader_len, kind, shader_path, "main", opts);
  if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success)
  {
    LOG_ERROR("Failed to Compile Shader Code of file: %s: %s", file_name, shaderc_result_get_error_message(result));
    shaderc_result_release(result);
    return NULL;
  }

  const u64 shader_size = shaderc_result_get_length(result);
  VkShaderModuleCreateInfo module_create_info = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  module_create_info.codeSize = shader_size;
  module_create_info.pCode = (u32 *)shaderc_result_get_bytes(result);

  VkShaderModule shader_module = NULL;
  if (vkCreateShaderModule(self->vulkan_device, &module_create_info, NULL, &shader_module))
  {
    LOG_ERROR("Failed to Create Vulkan Shader Module");
    shaderc_result_release(result);
    return NULL;
  }
  LOG_INFO("Vulkan Shader Module is Created");

  free((void *)src);
  shaderc_result_release(result);
  return shader_module;
}

b8 vulkan_create_shaders(Application* self)
{
  // create vertex shader
  self->vulkan_vertex_shader = application_create_shader_module(self, "shader.vert", shaderc_vertex_shader);
  if (!self->vulkan_vertex_shader)
  {
    return FALSE;
  }

  self->vulkan_fragment_shader = application_create_shader_module(self, "shader.frag", shaderc_fragment_shader);
  if (!self->vulkan_fragment_shader)
  {
    return FALSE;
  }

  LOG_INFO("All Vulkan Shaders are Created");
  return TRUE;
}

VkPipeline vulkan_create_graphics_pipeline(Application *self)
{
  VkPipelineLayoutCreateInfo layout_create_info = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  layout_create_info.setLayoutCount = 0;
  layout_create_info.pushConstantRangeCount = 0;

  if (vkCreatePipelineLayout(
    self->vulkan_device, 
    &layout_create_info, 
    NULL, 
    &self->vulkan_pipeline_layout
  ) != VK_SUCCESS)
  {
    LOG_ERROR("Failed to Create Vulkan Pipeline Layout");
  }
  LOG_INFO("Vulkan Pipeline Layout is Created");

  const char* entry_point = "main";
  VkPipelineShaderStageCreateInfo shader_stages[3] = {0};
  u32 shader_stage_count = 0;

  shader_stages[shader_stage_count++] = (VkPipelineShaderStageCreateInfo) {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = VK_SHADER_STAGE_VERTEX_BIT,
    .module = self->vulkan_vertex_shader,
    .pName = entry_point,
  };

  shader_stages[shader_stage_count++] = (VkPipelineShaderStageCreateInfo) {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
    .module = self->vulkan_fragment_shader,
    .pName = entry_point    
  };

  // vertex putting
  VkPipelineVertexInputStateCreateInfo vertex_input_create_info = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

  // input assembly
  VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  // depth and stencil config
  VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  depth_stencil_create_info.depthTestEnable = VK_TRUE;
  depth_stencil_create_info.depthWriteEnable = VK_TRUE;
  depth_stencil_create_info.depthCompareOp = VK_COMPARE_OP_LESS;
  depth_stencil_create_info.stencilTestEnable = VK_FALSE;

  // define viewport
  VkPipelineViewportStateCreateInfo viewport_create_info = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  viewport_create_info.viewportCount = 1;
  viewport_create_info.pViewports = NULL;
  viewport_create_info.scissorCount = 1;
  viewport_create_info.pScissors = NULL;

  // rasterizer settings
  VkPipelineRasterizationStateCreateInfo rasterization_create_info = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  rasterization_create_info.polygonMode = VK_POLYGON_MODE_FILL;
  rasterization_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterization_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterization_create_info.lineWidth = 1.0f;

  // no multisampling
  VkPipelineMultisampleStateCreateInfo multisample_create_info = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  multisample_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  // blending
  VkPipelineColorBlendAttachmentState attach_state = {
    .blendEnable = VK_FALSE,
    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };

  VkPipelineColorBlendStateCreateInfo blend_create_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .attachmentCount = 1,
    .pAttachments = &attach_state,
  };

  // enable dynamic states
  VkDynamicState dynamic_states[3] = {};
  u32 dynamic_state_count = 0;
  dynamic_states[dynamic_state_count++] = VK_DYNAMIC_STATE_VIEWPORT;
  dynamic_states[dynamic_state_count++] = VK_DYNAMIC_STATE_SCISSOR;

  VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  dynamic_state_create_info.dynamicStateCount = dynamic_state_count;
  dynamic_state_create_info.pDynamicStates = dynamic_states;

  // structure for dynamic rendering
  VkPipelineRenderingCreateInfo rendering_create_info = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  rendering_create_info.colorAttachmentCount = 1;
  rendering_create_info.pColorAttachmentFormats = &vulkan_swapchain_format;
  rendering_create_info.depthAttachmentFormat = vulkan_depth_format;

  VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  graphics_pipeline_create_info.pNext = &rendering_create_info;
  graphics_pipeline_create_info.stageCount = shader_stage_count;
  graphics_pipeline_create_info.pStages = shader_stages;
  graphics_pipeline_create_info.pVertexInputState = &vertex_input_create_info;
  graphics_pipeline_create_info.pInputAssemblyState = &input_assembly_create_info;
  graphics_pipeline_create_info.pColorBlendState = &blend_create_info;
  graphics_pipeline_create_info.pDepthStencilState = &depth_stencil_create_info;
  graphics_pipeline_create_info.pDynamicState = &dynamic_state_create_info;
  graphics_pipeline_create_info.pMultisampleState = &multisample_create_info;
  graphics_pipeline_create_info.pRasterizationState = &rasterization_create_info;
  graphics_pipeline_create_info.pViewportState = &viewport_create_info;
  graphics_pipeline_create_info.layout = self->vulkan_pipeline_layout;
  graphics_pipeline_create_info.renderPass = VK_NULL_HANDLE;

  VkPipeline new_pipeline = {0};
  if (vkCreateGraphicsPipelines(
    self->vulkan_device,
    NULL,
    1,
    &graphics_pipeline_create_info,
    NULL,
    &new_pipeline
  ) != VK_SUCCESS)
  {
    LOG_ERROR("Failed to create Vulkan Graphics Pipeline");
    return NULL;
  }
  LOG_INFO("Vulkan Graphics Pipeline is Created");

  return new_pipeline;
}

b8 vulkan_create_sync_resources(Application* self)
{
  VkSemaphoreTypeCreateInfo semaphore_type_create_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
    .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
    .initialValue = VULKAN_MAX_FRAMES_IN_FLIGHT,
  };

  VkSemaphoreCreateInfo semaphore_create_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    .pNext = &semaphore_type_create_info
  };

  if (vkCreateSemaphore(
    self->vulkan_device,
    &semaphore_create_info,
    NULL,
    &self->vulkan_timeline_semaphore
  ))
  {
    LOG_ERROR("Failed to create Vulkan Timeline Semaphore");
    return FALSE;
  }
  LOG_INFO("Vulkan Timeline Semaphore is Created");

  for (u32 i=0; i<VULKAN_MAX_FRAMES_IN_FLIGHT; ++i)
  {
    // create binary semphore
    VkSemaphoreCreateInfo binary_semaphore_create_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    if(vkCreateSemaphore(
      self->vulkan_device,
      &binary_semaphore_create_info,
      NULL,
      &self->vulkan_frame_resources[i].image_acquired_semaphore
    ))
    {
      LOG_ERROR("Failed to Create Vulkan Image Acquired Semaphore in vulkan frame resources index: %d", i);
      return FALSE;
    }
    LOG_INFO("Created Vulkan Image Acquired Semaphore index: %d", i);
  }
  LOG_INFO("Created all Vulkan Image Acquired Semaphores in Vulkan Frame Resouces");
  
  return TRUE;
}

b8 vulkan_create_command_buffers(Application *self)
{
  for (u32 i=0; i<VULKAN_MAX_FRAMES_IN_FLIGHT; ++i)
  {
    FrameResource *res = &self->vulkan_frame_resources[i];
    
    VkCommandPoolCreateInfo pool_create_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .queueFamilyIndex = self->vulkan_graphics_queue_family_idx
    };
    if (vkCreateCommandPool(
      self->vulkan_device,
      &pool_create_info,
      NULL,
      &res->command_pool
    ) != VK_SUCCESS)
    {
      LOG_ERROR("Failed to Create Vulkan Command Pool for Frame Resources index: %d", i);
      return FALSE;
    }
    LOG_INFO("Created Vulkan Command Pool for Frame Resources index: %d", i);

    VkCommandBufferAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = res->command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1
    };
    if (vkAllocateCommandBuffers(
      self->vulkan_device,
      &alloc_info,
      &res->command_buffer
    ))
    {
      LOG_ERROR("Failed to Create Vulkan Command Buffer for Frame Resources index: %d", i);
      return FALSE;
    }
    LOG_INFO("Created Vulkan Command Buffer for Frame Resources index: %d", i);
  }
  LOG_INFO("Vulkan Command Buffers in Frame Resources are created");
  
  return TRUE;
}

b8 vulkan_init(Application *self)
{
  if (!vulkan_create_instance(self))
  {
    LOG_ERROR("Failed to Create Vulkan Instance");
    return FALSE;
  }

  if (!vulkan_create_surface(self))
  {
    LOG_ERROR("Failed to Create Vulkan Surface");
    return FALSE;
  }

  self->vulkan_physical_device = vulkan_find_physical_device(self);
  if (!self->vulkan_physical_device)
  {
    LOG_ERROR("Failed to Find Vulkan Physical Device");
    return FALSE;
  }

  if (!vulkan_find_graphics_queue(self))
  {
    LOG_ERROR("Failed to find Vulkan graphics queue.");
    return FALSE;
  }

  if (!vulkan_create_device(self))
  {
    LOG_ERROR("Failed to Create vulkan device.");
    return FALSE;
  }

  if (!vulkan_init_vma(self))
  {
    LOG_ERROR("Failed to init vulkan VMA.");
    return FALSE;
  }

  if (!vulkan_create_swapchain(self, self->width, self->height))
  {
    LOG_ERROR("Failed to Create Vulkan Swapchain.");
    return FALSE;
  }
  self->vulkan_swapchain_require_recreate = FALSE;

  if (!vulkan_create_shaders(self))
  {
    LOG_ERROR("Failed to Create Vulkan Shader");
    return FALSE;
  }

  self->vulkan_pipeline = vulkan_create_graphics_pipeline(self);
  if (!self->vulkan_pipeline)
  {
    LOG_ERROR("Failed to Create Vulkan Graphics Pipeline");
    return FALSE;
  }

  if (!vulkan_create_sync_resources(self))
  {
    LOG_ERROR("Failed to Create Vulkan Sync Resouces");
    return FALSE;
  }

  if (!vulkan_create_command_buffers(self))
  {
    LOG_ERROR("Failed to Create Vulkan Command Buffers");
    return FALSE;
  }
  
  return TRUE;
}

void render(Application *self)
{
  if (self->vulkan_swapchain_require_recreate)
  {
    vkDeviceWaitIdle(self->vulkan_device);
    vulkan_destroy_swapchain(self);
    vulkan_create_swapchain(self, self->width, self->height);
    self->vulkan_swapchain_require_recreate = FALSE;
  }

  const u32 frame_res_idx = self->frame_idx++ % VULKAN_MAX_FRAMES_IN_FLIGHT;
  const u64 signal_value = self->next_signal_value++;
  const u64 wait_value = signal_value - VULKAN_MAX_FRAMES_IN_FLIGHT;

  VkSemaphoreWaitInfo wait_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
    .semaphoreCount = 1,
    .pSemaphores = &self->vulkan_timeline_semaphore,
    .pValues = &wait_value
  };
  vkWaitSemaphores(self->vulkan_device, &wait_info, UINT64_MAX);

  FrameResource *res = &self->vulkan_frame_resources[frame_res_idx];

  // start recording commands
  vkResetCommandPool(self->vulkan_device, res->command_pool, 0);

  // get resources for this frame
  u32 image_idx = 0;
  VkResult acquired_result = vkAcquireNextImageKHR(
    self->vulkan_device, 
    self->vulkan_swapchain,
    UINT64_MAX,
    res->image_acquired_semaphore,
    VK_NULL_HANDLE,
    &image_idx
  );

  if (acquired_result == VK_ERROR_OUT_OF_DATE_KHR)
  {
    self->vulkan_swapchain_require_recreate = TRUE;
    return;
  }
  else if(acquired_result == VK_SUBOPTIMAL_KHR)
  {
    self->vulkan_swapchain_require_recreate = TRUE;
  }

  // begin recording commands
  VkCommandBufferBeginInfo command_begin_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
  };
  vkBeginCommandBuffer(res->command_buffer, &command_begin_info);

  VkImageMemoryBarrier2 layout_barriers[2] = {0};
  u32 layout_barrier_count = 0;

  layout_barriers[layout_barrier_count++] = (VkImageMemoryBarrier2) {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
    .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    .srcAccessMask =  0,
    .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .image = self->vulkan_swapchain_images[image_idx],
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1
    }
  };

  layout_barriers[layout_barrier_count++] = (VkImageMemoryBarrier2) {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
    .srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
    .srcAccessMask =  0,
    .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                    VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
    .dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    .image = self->vulkan_depth_image,
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1
    }
  };

  VkDependencyInfo dependency_info = {
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    .imageMemoryBarrierCount = layout_barrier_count,
    .pImageMemoryBarriers = layout_barriers
  };
  vkCmdPipelineBarrier2(res->command_buffer, &dependency_info);

  VkRenderingAttachmentInfo color_attach_info = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .imageView = self->vulkan_swapchain_image_views[image_idx],
    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .clearValue = {
      .color = {0.01f, 0.01f, 0.01f, 1}
    },
  };

  VkRenderingAttachmentInfo depth_attach_info = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .imageView = self->vulkan_depth_image_view,
    .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .clearValue = {
      .depthStencil = {1.0f, 0}
    }
  };

  VkRenderingInfo rendering_info = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .renderArea = {
      .offset = {.x = 0, .y = 0},
      .extent = {
        .width = self->vulkan_swapchain_width,
        .height = self->vulkan_swapchain_height
      }
    },
    .layerCount = 1,
    .colorAttachmentCount = 1,
    .pColorAttachments = &color_attach_info,
    .pDepthAttachment = &depth_attach_info
  };

  // begin dynamic rendering
  vkCmdBeginRendering(res->command_buffer, &rendering_info);
  {
    // set viewport and scissor state
    VkViewport viewport = {
      .x = 0,
      .y = 0,
      .width = self->vulkan_swapchain_width,
      .height = self->vulkan_swapchain_height,
    };
    vkCmdSetViewport(res->command_buffer, 0, 1, &viewport);

    VkRect2D scissor = {
      .offset = {
        .x = 0,
        .y = 0
      },
      .extent = {
        .width = self->vulkan_swapchain_width,
        .height = self->vulkan_swapchain_height
      }
    };
    vkCmdSetScissor(res->command_buffer, 0, 1, &scissor);

    // draw triangle
    vkCmdBindPipeline(res->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, self->vulkan_pipeline);
    vkCmdDraw(res->command_buffer, 3, 1, 0, 0);
  }
  // end dynamic rendering
  vkCmdEndRendering(res->command_buffer);

  // transition image from color attachment to present to show it
  VkImageMemoryBarrier2 present_layout_barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
    .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    .dstStageMask = VK_PIPELINE_STAGE_2_NONE,
    .dstAccessMask = 0,
    .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    .image = self->vulkan_swapchain_images[image_idx],
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1
    }
  };

  VkDependencyInfo present_dep_info = {
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    .imageMemoryBarrierCount = 1,
    .pImageMemoryBarriers = &present_layout_barrier
  };
  vkCmdPipelineBarrier2(res->command_buffer, &present_dep_info);

  vkEndCommandBuffer(res->command_buffer);

  VkSemaphoreSubmitInfo image_acquire_wait_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
    .semaphore = res->image_acquired_semaphore,
    .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
  };

  VkSemaphoreSubmitInfo semaphore_signals[3] = {0};
  u32 semaphore_signal_count = 0;
  semaphore_signals[semaphore_signal_count++] = (VkSemaphoreSubmitInfo) {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
    .semaphore = self->vulkan_swapchain_render_complete_semaphores[image_idx],
    .stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT
  };
  semaphore_signals[semaphore_signal_count++] = (VkSemaphoreSubmitInfo) {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
    .semaphore = self->vulkan_timeline_semaphore,
    .value = signal_value,
    .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
  };

  VkCommandBufferSubmitInfo cmd_submit_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
    .commandBuffer = res->command_buffer
  };

  VkSubmitInfo2 submit_info = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
    .waitSemaphoreInfoCount = 1,
    .pWaitSemaphoreInfos = &image_acquire_wait_info,
    .commandBufferInfoCount = 1,
    .pCommandBufferInfos = &cmd_submit_info,
    .signalSemaphoreInfoCount = semaphore_signal_count,
    .pSignalSemaphoreInfos = semaphore_signals
  };
  vkQueueSubmit2(self->vulkan_graphics_queue, 1, &submit_info, VK_NULL_HANDLE);

  // present the image
  VkPresentInfoKHR present_info = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &self->vulkan_swapchain_render_complete_semaphores[image_idx],
    .swapchainCount = 1,
    .pSwapchains = &self->vulkan_swapchain,
    .pImageIndices = &image_idx,
    .pResults = NULL
  };

  vkQueuePresentKHR(self->vulkan_graphics_queue, &present_info);
}

b8 application_init(Application *self)
{
  self->width = 800;
  self->height = 600;
  self->frame_idx = 0;
  self->next_signal_value = VULKAN_MAX_FRAMES_IN_FLIGHT + 1;
  
  if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
  {
    LOG_ERROR("Failed to Init SDL");
    return FALSE;
  }

  self->window = SDL_CreateWindow(
    "Vulkan Basics", 
    self->width, 
    self->height, 
    SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN
  );
  if (!self->window)
  {
    LOG_ERROR("Failed to Create SDL window");
    return FALSE;
  }

  if (!vulkan_init(self))
  {
    LOG_ERROR("Failed to Init Vulkan");
    return FALSE;
  }
  LOG_INFO("Vulkan is Initialized");

  SDL_ShowWindow(self->window);
  return TRUE;
}

void application_run(Application* self)
{
  self->running = TRUE;
  
  // application loop
  while (self->running)
  {
    // event loop
    SDL_Event event = {0};
    while (SDL_PollEvent(&event))
    {
      if (event.type == SDL_EVENT_QUIT)
      {
        self->running = FALSE;
        break;
      }
      else if (event.type == SDL_EVENT_WINDOW_RESIZED)
      {
        self->width = event.window.data1;
        self->height = event.window.data2;
      }
    }

    render(self);
  }
}

void application_shutdown(Application *self)
{
  // wait in case resources are in use
  vkDeviceWaitIdle(self->vulkan_device);
  
  if (self->vulkan_timeline_semaphore)
  {
    vkDestroySemaphore(self->vulkan_device, self->vulkan_timeline_semaphore, NULL);
    self->vulkan_timeline_semaphore = 0;
  }

  for (u32 i=0; i<VULKAN_MAX_FRAMES_IN_FLIGHT; ++i)
  {
    FrameResource *res = &self->vulkan_frame_resources[i];
    vkDestroySemaphore(self->vulkan_device, res->image_acquired_semaphore, NULL);
    vkDestroyCommandPool(self->vulkan_device, res->command_pool, NULL);
    res->command_buffer = 0;
    res->image_acquired_semaphore = 0;
  }

  // pipeline cleanup
  if (self->vulkan_pipeline_layout)
  {
    vkDestroyPipelineLayout(self->vulkan_device, self->vulkan_pipeline_layout, NULL);
    self->vulkan_pipeline_layout = 0;
  }

  if (self->vulkan_pipeline)
  {
    vkDestroyPipeline(self->vulkan_device, self->vulkan_pipeline, NULL);
    self->vulkan_pipeline = 0;
  }

  // shaders cleanup
  if (self->vulkan_vertex_shader)
  {
    vkDestroyShaderModule(self->vulkan_device, self->vulkan_vertex_shader, NULL);
    self->vulkan_vertex_shader = 0;
  }

  if (self->vulkan_fragment_shader)
  {
    vkDestroyShaderModule(self->vulkan_device, self->vulkan_fragment_shader, NULL);
    self->vulkan_fragment_shader = 0;
  }
  
  // swapchain cleanup
  vulkan_destroy_swapchain(self);
  
  // vma cleanup
  if (self->vulkan_vma_allocator)
  {
    vmaDestroyAllocator(self->vulkan_vma_allocator);
  }
  
  // vulkan cleanup
  if (self->vulkan_device)
  {
    vkDestroyDevice(self->vulkan_device, NULL);
    self->vulkan_device = 0;
  }  
  
  if (self->vulkan_physical_device)
  {
    self->vulkan_physical_device = 0;
  }
  
  if (self->vulkan_surface)
  {
    vkDestroySurfaceKHR(self->vulkan_instance, self->vulkan_surface, NULL);
    self->vulkan_surface = 0;
  }
  
  if (self->vulkan_instance)
  {
    vkDestroyInstance(self->vulkan_instance, NULL);
    self->vulkan_instance = 0;
  }
  volkFinalize();

  // sdl cleanup 
  if (self->window)
  {
    SDL_DestroyWindow(self->window);
  }
  SDL_Quit();
}
