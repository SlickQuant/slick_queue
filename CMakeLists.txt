cmake_minimum_required(VERSION 3.10)

set(BUILD_VERSION 1.0.2.0)

project(slick_queue
        VERSION ${BUILD_VERSION}
        DESCRIPTION "A C++ Lock-Free MPMC queue"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_library(slick_queue INTERFACE)
target_include_directories(slick_queue INTERFACE include)

if(UNIX AND NOT APPLE)
    target_link_libraries(slick_queue INTERFACE rt atomic)
endif()

if (MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "-O2")
    add_definitions(-D_WIN32_WINNT=0x0A00)
    set(CMAKE_SUPPRESS_REGENERATION true)   # supress zero_check
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "-fsanitize=address,undefined")
endif()

option(BUILD_SLICK_QUEUE_TESTS "Build tests" ON)
if(BUILD_SLICK_QUEUE_TESTS)
    if (WIN32)
        add_subdirectory(tests)
    else()
        add_subdirectory(tests EXCLUDE_FROM_ALL)
    endif()
endif()

# Installation rules
install(DIRECTORY include/ DESTINATION include)

message(STATUS "slick_queue: ${BUILD_VERSION}")

# Automatically run install after build in Release mode
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_target(dist_slick_queue ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/dist/include
        COMMENT "Copying slick_queue headers to dist/include"
        VERBATIM
    )

    if (PROJECT_IS_TOP_LEVEL)
        add_custom_target(package_slick_queue ALL
            COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${CMAKE_BINARY_DIR}/dist/slick_queue_${BUILD_VERSION}.zip" --format=zip "${CMAKE_CURRENT_SOURCE_DIR}/include"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMENT "Creating zip archive"
        )
        add_dependencies(package_slick_queue dist_slick_queue)
    endif()
endif()

