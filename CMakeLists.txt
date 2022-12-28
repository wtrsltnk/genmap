cmake_minimum_required(VERSION 3.7)

include(cmake/CPM.cmake)

project(genmap)

find_package(OPENGL REQUIRED)

CPMAddPackage(
    NAME spdlog
    GITHUB_REPOSITORY gabime/spdlog
    VERSION 1.7.0)

CPMAddPackage(
  NAME glm
  GITHUB_REPOSITORY g-truc/glm
  GIT_TAG 0.9.9.7
)

CPMAddPackage(
  NAME EnTT
  VERSION 3.1.1
  GITHUB_REPOSITORY skypjack/entt
  # EnTT's CMakeLists screws with configuration options
  DOWNLOAD_ONLY True
)

if (EnTT_ADDED)
  add_library(EnTT INTERFACE)
  target_include_directories(EnTT INTERFACE ${EnTT_SOURCE_DIR}/src)
endif()

add_executable(genmap
    camera.cpp
    camera.h
    entitycomponents.h
    genmapapp.cpp
    genmapapp.h
    hl1bspasset.cpp
    hl1bspasset.h
    hl1bsptypes.h
    hl1filesystem.cpp
    hl1filesystem.h
    hl1wadasset.cpp
    hl1wadasset.h
    hltexture.cpp
    hltexture.h
    hltypes.h
    include/application.h
    include/glad.c
    include/glad/glad.h
    include/glbuffer.h
    include/glshader.h
    include/stb_image.h
    include/stb_rect_pack.h
    main.cpp
    stb_image.cpp
    stb_rect_pack.cpp
    mdl/studio_render.cpp
    mdl/studio_utils.cpp
    mdl/common/mathlib.c
    mdl/common/cmdlib.c
    mdl/renderapi.cpp
    )

target_sources(genmap
    PRIVATE
        include/GL/wglext.h
)

target_include_directories(genmap
    PRIVATE
        include
        glad
)

target_link_libraries(genmap
    PRIVATE
        ${OPENGL_LIBRARIES}
        glm
        spdlog
        EnTT
)

target_compile_features(genmap
    PRIVATE
        cxx_std_17
        cxx_auto_type
        cxx_nullptr
        cxx_range_for
        cxx_thread_local
)

if(MINGW)
    target_link_options(genmap
        PUBLIC
            -static
    )
endif()
