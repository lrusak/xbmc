set(SOURCES BaseRenderBuffer.cpp
            BaseRenderBufferPool.cpp
            RenderBufferManager.cpp
            RenderBufferFBO.cpp
            RenderBufferPoolFBO.cpp
)

set(HEADERS BaseRenderBuffer.h
            BaseRenderBufferPool.h
            IRenderBuffer.h
            IRenderBufferPool.h
            RenderBufferManager.h
            RenderBufferFBO.h
            RenderBufferPoolFBO.h
)

core_add_library(rp-buffers)
