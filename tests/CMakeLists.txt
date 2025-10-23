find_package(GTest QUIET)

if(NOT GTest_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
  )
  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

add_executable(slick_queue_tests tests.cpp shm_tests.cpp)

target_link_libraries(slick_queue_tests PRIVATE slick_queue GTest::gtest_main)

enable_testing()
include(GoogleTest)
gtest_discover_tests(slick_queue_tests)
