cmake_minimum_required(VERSION 3.10)
project(slick_queue
        VERSION 0.1.0
        DESCRIPTION "A C++ Lock-Free MPMC queue"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_library(slick_queue INTERFACE)
target_include_directories(slick_queue INTERFACE include)

if(UNIX AND NOT APPLE)
    target_link_libraries(slick_queue INTERFACE rt)
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

