#pragma once

#include "defines.h"

#include <SDL3/SDL.h>
#include <vma_c.h>
#include <vulkan/vulkan_core.h>

#define VULKAN_VERSION VK_API_VERSION_1_4
#define VULKAN_MAX_FRAMES_IN_FLIGHT 2

typedef struct FrameResource
{
  VkCommandPool command_pool;
  VkCommandBuffer command_buffer;
  VkSemaphore image_acquired_semaphore;
} FrameResource;

typedef struct Application
{
  SDL_Window *window;
  u32 width;
  u32 height;
  b8 running;
  u64 frame_idx;
  u64 next_signal_value;

  VkInstance vulkan_instance;
  VkSurfaceKHR vulkan_surface;
  VkPhysicalDevice vulkan_physical_device;
  VkDevice vulkan_device;

  u32 vulkan_graphics_queue_family_idx;
  VkQueue vulkan_graphics_queue;

  VmaAllocator vulkan_vma_allocator;

  VkSwapchainKHR vulkan_swapchain;
  VkImage *vulkan_swapchain_images;
  VkImageView *vulkan_swapchain_image_views;
  VkSemaphore *vulkan_swapchain_render_complete_semaphores;
  b8 vulkan_swapchain_require_recreate;
  u32 vulkan_swapchain_width;
  u32 vulkan_swapchain_height;

  VkImage vulkan_depth_image;
  VkImageView vulkan_depth_image_view;
  VmaAllocation vulkan_depth_vma_allocator;

  VkShaderModule vulkan_vertex_shader;
  VkShaderModule vulkan_fragment_shader;

  VkPipeline vulkan_pipeline;
  VkPipelineLayout vulkan_pipeline_layout;

  VkSemaphore vulkan_timeline_semaphore;
  FrameResource vulkan_frame_resources[VULKAN_MAX_FRAMES_IN_FLIGHT];
} Application;

b8 application_init(Application *self);
void application_run(Application* self);
void application_shutdown(Application *self);
