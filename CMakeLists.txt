cmake_minimum_required(VERSION 3.28)
project(vulkan_basics)

find_package(Vulkan REQUIRED COMPONENTS volk shaderc_combined)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
  src/**.c
  src/**.h
)

add_executable(${PROJECT_NAME} ${SRC_FILES})

add_subdirectory(vendor)

target_include_directories(${PROJECT_NAME} PRIVATE
  src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  Vulkan::shaderc_combined

  SDL3::SDL3-static

  vma_c
)
