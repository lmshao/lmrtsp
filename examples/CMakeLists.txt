# CMakeLists.txt for RTSP Examples
cmake_minimum_required(VERSION 3.10)

project(lmrtsp-examples)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Add all .cpp files as separate executables
file(GLOB_RECURSE EXAMPLE_SOURCES "*.cpp")
foreach(EXAMPLE_SOURCE ${EXAMPLE_SOURCES})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)
    add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCE})
    
    # Link lmrtsp library
    target_link_libraries(${EXAMPLE_NAME} PRIVATE lmrtsp)
    
    # Set output directory
    set_target_properties(${EXAMPLE_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/examples
    )
endforeach()
