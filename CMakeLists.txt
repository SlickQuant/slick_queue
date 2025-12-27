cmake_minimum_required(VERSION 3.10)

project(slick_queue
        VERSION 1.2.0
        DESCRIPTION "A C++ Lock-Free MPMC queue"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

add_library(slick_queue INTERFACE)
add_library(slick::slick_queue ALIAS slick_queue)
target_include_directories(slick_queue INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(UNIX AND NOT APPLE)
    target_link_libraries(slick_queue INTERFACE rt atomic)
endif()

option(BUILD_SLICK_QUEUE_TESTS "Build tests" ON)
if(BUILD_SLICK_QUEUE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation rules
install(DIRECTORY include/ DESTINATION include)

# Generate and install CMake config files for vcpkg compatibility
include(CMakePackageConfigHelpers)

# Create the config file
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/slick_queueConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/slick_queueConfig.cmake"
    INSTALL_DESTINATION lib/cmake/slick_queue
)

# Create the version file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/slick_queueConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install the config files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/slick_queueConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/slick_queueConfigVersion.cmake"
    DESTINATION lib/cmake/slick_queue
)

# Install the target
install(TARGETS slick_queue
    EXPORT slick_queueTargets
)

# Install the export set
install(EXPORT slick_queueTargets
    FILE slick_queueTargets.cmake
    NAMESPACE slick::
    DESTINATION lib/cmake/slick_queue
)

message(STATUS "slick_queue: ${PROJECT_VERSION}")

# # Automatically run install after build in Release mode
# if(CMAKE_BUILD_TYPE STREQUAL "Release")
#     add_custom_target(dist_slick_queue ALL
#         COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/dist/include
#         COMMENT "Copying slick_queue headers to dist/include"
#         VERBATIM
#     )

#     if (MSVC)
#         set_target_properties(dist_slick_queue PROPERTIES
#             VS_GLOBAL_IntDir "$(Platform)\\$(Configuration)\\dist_slick_queue\\"
#         )
#     endif()

#     if (PROJECT_IS_TOP_LEVEL)
#         add_custom_target(package_slick_queue ALL
#             COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${CMAKE_BINARY_DIR}/dist/slick_queue_${PROJECT_VERSION}.zip" --format=zip "${CMAKE_CURRENT_SOURCE_DIR}/include"
#             WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
#             COMMENT "Creating zip archive"
#         )
#         add_dependencies(package_slick_queue dist_slick_queue)

#         if (MSVC)
#             set_target_properties(package_slick_queue PROPERTIES
#                 VS_GLOBAL_IntDir "$(Platform)\\$(Configuration)\\package_slick_queue\\"
#             )
#         endif()
#     endif()
# endif()

