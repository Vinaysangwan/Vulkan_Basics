#pragma once

#include "defines.h"

#include <SDL3/SDL.h>
#include <volk/volk.h>
// #include <vma/vk_mem_alloc.h>

#define VULKAN_VERSION VK_API_VERSION_1_4

typedef struct Application
{
  SDL_Window *window;
  u32 width;
  u32 height;
  b8 running;

  VkInstance vulkan_instance;
  VkSurfaceKHR vulkan_surface;
  VkPhysicalDevice vulkan_physical_device;
  VkDevice vulkan_device;

  u32 vulkan_graphics_queue_family_idx;
  VkQueue vulkan_graphics_queue;

  // VmaAllocator vulkan_vma_allocator;
} Application;

b8 application_init(Application *self);
void application_run(Application* self);
void application_shutdown(Application *self);
