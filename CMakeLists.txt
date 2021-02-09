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
    batchrenderer.cpp
    camera.cpp
    entitycomponents.h
    hl1bspasset.cpp
    hl1bspasset.h
    hl1wadasset.cpp
    hl1wadasset.h
    hltexture.cpp
    hltexture.h
    include/application.h
    include/glad/glad.h
    include/glbuffer.h
    include/glshader.h
    main.cpp
    stb_rect_pack.cpp
    stb_image.cpp
    )

target_include_directories(genmap
    PRIVATE include
    PRIVATE glad
    )

target_link_libraries(genmap
    ${OPENGL_LIBRARIES}
    glm
    spdlog
    EnTT
    )

target_compile_features(genmap
    PRIVATE cxx_std_17
    PRIVATE cxx_auto_type
    PRIVATE cxx_nullptr
    PRIVATE cxx_range_for
    PRIVATE cxx_thread_local
    )

if(MINGW)
    target_link_options(genmap
        PUBLIC -static)
endif()
