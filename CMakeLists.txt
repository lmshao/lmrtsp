cmake_minimum_required(VERSION 3.10)
project(rtsp VERSION 0.0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG)
    message(STATUS "DEBUG mode enabled")
else()
    message(STATUS "RELEASE mode enabled")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable testing
enable_testing()

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include/rtsp)
include_directories(${PROJECT_SOURCE_DIR}/network/include)

# Build network library first
add_subdirectory(network)

# Main RTSP library
file(GLOB_RECURSE RTSP_SOURCES "src/rtsp/*.cpp")
add_library(rtsp STATIC ${RTSP_SOURCES})
target_include_directories(rtsp PUBLIC include network/include)
target_link_libraries(rtsp PUBLIC network)

# Option to build applications
option(BUILD_APPS "Build RTSP applications (server/client)" ON)

if(BUILD_APPS)
    message(STATUS "Building RTSP applications...")
    add_subdirectory(apps)
endif()

# Option to build tests
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    message(STATUS "Building RTSP unit tests...")
    add_subdirectory(tests)
endif()

# Option to build examples
option(BUILD_EXAMPLES "Build example programs" ON)

if(BUILD_EXAMPLES)
    message(STATUS "Building RTSP examples...")
    
    # Create examples directory in build folder
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/examples)
    
    # Find all example source files
    file(GLOB EXAMPLE_SOURCES "examples/*.cpp")
    
    # Create executables for each example
    foreach(EXAMPLE_SOURCE ${EXAMPLE_SOURCES})
        # Get filename without extension
        get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)
        
        # Create executable with rtsp-example- prefix to avoid name conflicts
        set(EXAMPLE_TARGET "rtsp-example-${EXAMPLE_NAME}")
        add_executable(${EXAMPLE_TARGET} ${EXAMPLE_SOURCE})
        
        # Set include directories for examples
        target_include_directories(${EXAMPLE_TARGET} PRIVATE 
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/network/include
        )
        
        # Examples are header-only, so no need to link rtsp library
        # But we can link it if needed in the future
        target_link_libraries(${EXAMPLE_TARGET} PRIVATE rtsp)
        
        # Set output directory
        set_target_properties(${EXAMPLE_TARGET} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
        )
        
        message(STATUS "Added example: ${EXAMPLE_TARGET}")
    endforeach()
    
    # Create a custom target to build all examples at once
    add_custom_target(examples)
    foreach(EXAMPLE_SOURCE ${EXAMPLE_SOURCES})
        get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)
        set(EXAMPLE_TARGET "rtsp-example-${EXAMPLE_NAME}")
        add_dependencies(examples ${EXAMPLE_TARGET})
    endforeach()
    
endif()

# Print build information
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
if(BUILD_TESTS)
    message(STATUS "Tests will be built in: ${CMAKE_BINARY_DIR}/tests")
endif()
if(BUILD_EXAMPLES)
    message(STATUS "Examples will be built in: ${CMAKE_BINARY_DIR}/examples")
endif()

