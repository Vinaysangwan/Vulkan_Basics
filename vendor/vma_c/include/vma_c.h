#pragma once

#include <volk/volk.h>
#include <vma/vk_mem_alloc.h>

VMA_CALL_PRE VkResult VMA_CALL_POST vma_c_ImportVulkanFunctionsFromVolk(
  const VmaAllocatorCreateInfo* VMA_NOT_NULL pAllocatorCreateInfo,
  VmaVulkanFunctions* VMA_NOT_NULL pDstVulkanFunctions
);
