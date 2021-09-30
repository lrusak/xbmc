if(OPENGLES_FOUND)
  set(SOURCES RenderSystemGLES.cpp
              ScreenshotSurfaceGLES.cpp
              GLESShader.cpp
              RenderBuffer.cpp)

  set(HEADERS RenderSystemGLES.h
              ScreenshotSurfaceGLES.h
              GLESShader.h
              RenderBuffer.h)

  core_add_library(rendering_gles)
endif()
