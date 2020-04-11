cmake_minimum_required(VERSION 3.7)

project(genmap)

find_package(OPENGL REQUIRED)

add_executable(genmap
    include/glad/glad.h
    include/application.h
    include/glbuffer.h
    include/glmath.h
    include/glshader.h
    main.cpp
    hl1bspasset.cpp
    hl1wadasset.cpp
    hltexture.cpp
    glmath.cpp
    stb_rect_pack.cpp
    )

target_include_directories(genmap
    PRIVATE include
    )

target_link_libraries(genmap
    ${OPENGL_LIBRARIES}
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
