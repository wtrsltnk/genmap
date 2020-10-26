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

add_executable(genmap
    application.cpp
    batchrenderer.cpp
    hl1bspasset.cpp
    hl1wadasset.cpp
    hltexture.cpp
    include/application.h
    include/glad/glad.h
    include/glbuffer.h
    include/glshader.h
    main.cpp
    stb_rect_pack.cpp
    )

target_include_directories(genmap
    PRIVATE include
    PRIVATE glad
    )

target_link_libraries(genmap
    ${OPENGL_LIBRARIES}
    glm
    spdlog
    )

target_compile_features(genmap
    PRIVATE cxx_auto_type
    PRIVATE cxx_nullptr
    PRIVATE cxx_range_for
    PRIVATE cxx_thread_local
    )

if(MINGW)
    target_link_options(genmap
        PUBLIC -static)
endif()
