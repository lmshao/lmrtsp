cmake_minimum_required(VERSION 3.10)
project(rtsp VERSION 0.0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
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

# Compiler flags
if(MSVC)
    # MSVC specific flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")
    # Debug-specific flags for MSVC
    set(CMAKE_CXX_FLAGS_DEBUG "/MDd /Zi /Od /DDEBUG")
    # Release-specific flags for MSVC
    set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /DNDEBUG /DRELEASE")
else()
    # GCC/Clang flags for Linux/Unix
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error -Wno-format-truncation -Wno-unused-result")
    # Debug-specific flags for GCC/Clang
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
    # Release-specific flags for GCC/Clang
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -DRELEASE")
endif()

# Print build configuration
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "C++ flags for ${CMAKE_BUILD_TYPE}: ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}}")

# Library build options
option(BUILD_STATIC_LIBS "Build static libraries" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

if(NOT BUILD_STATIC_LIBS AND NOT BUILD_SHARED_LIBS)
    message(FATAL_ERROR "At least one of BUILD_STATIC_LIBS or BUILD_SHARED_LIBS must be ON")
endif()

# Enable testing
enable_testing()

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include/rtsp)
include_directories(${PROJECT_SOURCE_DIR}/network/include)
include_directories(${PROJECT_SOURCE_DIR}/src)

# Build network library first
add_subdirectory(network)

# Collect RTSP source files
file(GLOB_RECURSE RTSP_SOURCES "src/rtsp/*.cpp")
file(GLOB_RECURSE MPEGTS_SOURCES "src/mpegts/*.cpp")
set(RTSP_SOURCES ${RTSP_SOURCES} ${MPEGTS_SOURCES})

# Static library
if(BUILD_STATIC_LIBS)
    add_library(${PROJECT_NAME}_static STATIC ${RTSP_SOURCES})
    target_include_directories(${PROJECT_NAME}_static PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    target_include_directories(${PROJECT_NAME}_static PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/network/include
    )
    # Link to network library
    target_link_libraries(${PROJECT_NAME}_static PUBLIC 
        $<IF:$<TARGET_EXISTS:network::static>,network::static,network> 
    )
    set_target_properties(${PROJECT_NAME}_static PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
    set_target_properties(${PROJECT_NAME}_static PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
    
    # Create alias for static library (for internal use)
    add_library(${PROJECT_NAME}::static ALIAS ${PROJECT_NAME}_static)
    message(STATUS "Building static library: lib${PROJECT_NAME}.a")
endif()

# Shared library
if(BUILD_SHARED_LIBS)
    add_library(${PROJECT_NAME}_shared SHARED ${RTSP_SOURCES})
    target_include_directories(${PROJECT_NAME}_shared PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    target_include_directories(${PROJECT_NAME}_shared PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/network/include
    )
    # Link to network library
    target_link_libraries(${PROJECT_NAME}_shared PUBLIC 
        $<IF:$<TARGET_EXISTS:network::shared>,network::shared,network> 
    )
    set_target_properties(${PROJECT_NAME}_shared PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
    set_target_properties(${PROJECT_NAME}_shared PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
    
    # Create alias for shared library
    add_library(${PROJECT_NAME}::shared ALIAS ${PROJECT_NAME}_shared)
    message(STATUS "Building shared library: lib${PROJECT_NAME}.so")
endif()

# Create a default alias that points to static library (for examples and tests)
if(BUILD_STATIC_LIBS)
    add_library(${PROJECT_NAME} ALIAS ${PROJECT_NAME}_static)
    set(DEFAULT_LIBRARY_TYPE "static")
else()
    add_library(${PROJECT_NAME} ALIAS ${PROJECT_NAME}_shared)
    set(DEFAULT_LIBRARY_TYPE "shared")
endif()

message(STATUS "Examples and tests will use ${DEFAULT_LIBRARY_TYPE} library")

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
        
        # Create executable with original filename
        add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCE})
        
        # Set include directories for examples
        target_include_directories(${EXAMPLE_NAME} PRIVATE 
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/network/include
        )
        
        # Link to the default library (static or shared)
        target_link_libraries(${EXAMPLE_NAME} PRIVATE ${PROJECT_NAME})
        
        # Set output directory
        set_target_properties(${EXAMPLE_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
        )
        
        message(STATUS "Added example: ${EXAMPLE_NAME}")
    endforeach()
    
    # Create a custom target to build all examples at once
    add_custom_target(examples)
    foreach(EXAMPLE_SOURCE ${EXAMPLE_SOURCES})
        get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)
        add_dependencies(examples ${EXAMPLE_NAME})
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

