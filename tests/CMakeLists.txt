project(slick_queue_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(slick_queue_tests tests.cpp shm_tests.cpp)