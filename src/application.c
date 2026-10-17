#include "application.h"
#include "logger.h"
#include "darray.h"

#define VOLK_IMPLEMENTATION
#include <volk/volk.h>

// #define VMA_IMPLEMENTATION
// #include <vma/vk_mem_alloc.h>

#include <SDL3/SDL_vulkan.h>

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

  if (!supported_features_13.dynamicRendering || !supported_features_13.synchronization2 || !supported_features_12.timelineSemaphore)
  {
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

// b8 vulkan_init_vma(Application* self)
// {
//   VmaVulkanFunctions vma_func_info = {};
//   VmaAllocatorCreateInfo vma_alloc_info = {};
//   vma_alloc_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
//   vma_alloc_info.physicalDevice = self->vulkan_physical_device;
//   vma_alloc_info.device = self->vulkan_device;
//   vma_alloc_info.instance = self->vulkan_instance;
//   vma_alloc_info.vulkanApiVersion = VULKAN_VERSION;
//   vma_alloc_info.pVulkanFunctions = &vma_func_info;

//   vmaImportVulkanFunctionsFromVolk(&vma_alloc_info, &vma_func_info);

//   if (vmaCreateAllocator(&vma_alloc_info, &self->vulkan_vma_allocator) != VK_SUCCESS)
//   {
//     LOG_ERROR("Failed to Create Vulkan vma allocator");
//     return FALSE;
//   }
//   LOG_INFO("Created Vulkan vma allocator.");
  
//   return TRUE;
// }

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

  // if (!vulkan_init_vma(self))
  // {
  //   LOG_ERROR("Failed to init vulkan VMA.");
  //   return FALSE;
  // }
  
  return TRUE;
}

void render()
{

}

b8 application_init(Application *self)
{
  if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
  {
    LOG_ERROR("Failed to Init SDL");
    return FALSE;
  }

  self->width = 800;
  self->height = 600;

  self->window = SDL_CreateWindow("Vulkan Basics", self->width, self->height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
  if (!self->window)
  {
    LOG_ERROR("Failed to Create SDL window");
    return FALSE;
  }

  if (!vulkan_init(self))
  {
    return FALSE;
  }

  return TRUE;
}

void application_run(Application* self)
{
  self->running = TRUE;
  while (self->running)
  {
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

    render();
  }
}

void application_shutdown(Application *self)
{
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
