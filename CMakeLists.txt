cmake_minimum_required(VERSION 3.5)
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

if (WIN32)
    add_subdirectory(tests)
else()
    add_subdirectory(tests EXCLUDE_FROM_ALL)
endif()

