project(vma_c)

find_package(Vulkan REQUIRED COMPONENTS volk)

file(GLOB_RECURSE SRC_FILES
  src/**.cpp
  include/**.h
)

add_library(${PROJECT_NAME} STATIC ${SRC_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC
  include
)

target_link_libraries(${PROJECT_NAME} PUBLIC
  Vulkan::Vulkan
  Vulkan::volk
)
