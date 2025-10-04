add_executable(slick_queue_tests tests.cpp shm_tests.cpp)

target_link_libraries(slick_queue_tests PRIVATE slick_queue)

enable_testing()
add_test(NAME slick_queue_tests COMMAND slick_queue_tests)
