cmake_minimum_required(VERSION 3.14)
project(argparse)

set(BUILD_ADDITIONAL_TARGETS OFF)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(BUILD_ADDITIONAL_TARGETS ON)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(argparse SHARED
    src/argparse.cpp
)
target_include_directories(argparse PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if (NOT ${BUILD_ADDITIONAL_TARGETS})
    return()
endif()


# Examples

add_executable(example_basic example/basic.cpp)
target_link_libraries(example_basic argparse)

add_executable(example_args example/args.cpp)
target_link_libraries(example_args argparse)

add_executable(example_subcommand example/subcommand.cpp)
target_link_libraries(example_subcommand argparse)

# Install target

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/argparseConfigVersion.cmake"
    VERSION 0.1
    COMPATIBILITY AnyNewerVersion
)

install(TARGETS argparse
    EXPORT argparseTargets
    LIBRARY DESTINATION lib COMPONENT Runtime
    ARCHIVE DESTINATION lib COMPONENT Development
    RUNTIME DESTINATION bin COMPONENT Runtime
    PUBLIC_HEADER DESTINATION include COMPONENT Development
    BUNDLE DESTINATION bin COMPONENT Runtime
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/argparseConfig.cmake.in"
    "${PROJECT_BINARY_DIR}/argparseConfig.cmake"
    INSTALL_DESTINATION lib/cmake/argparse
)

install(EXPORT argparseTargets DESTINATION lib/cmake/argparse)
install(FILES "${PROJECT_BINARY_DIR}/argparseConfigVersion.cmake"
              "${PROJECT_BINARY_DIR}/argparseConfig.cmake"
        DESTINATION lib/cmake/argparse)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ DESTINATION include)
