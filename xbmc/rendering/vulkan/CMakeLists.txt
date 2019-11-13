set(SOURCES RenderSystemVulkan.cpp
            VulkanDebug.cpp
            VulkanDevice.cpp
            VulkanShader.cpp)

set(HEADERS RenderSystemVulkan.h
            UniformBuffer.h
            VertexBuffer.h
            VulkanDebug.h
            VulkanDevice.h
            VulkanShader.h
            VulkanShaders.h)

core_add_library(rendering_vulkan)
add_dependencies(rendering_vulkan generate-vulkan-shaders)
