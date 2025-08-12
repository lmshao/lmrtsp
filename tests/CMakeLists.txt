# CMakeLists.txt for RTSP Tests
cmake_minimum_required(VERSION 3.10)

# Test executable names
set(TEST_SOURCES
    test_rtsp_request.cpp
    test_rtsp_response.cpp
    test_rtsp_integration.cpp
)

# Create test executables
foreach(TEST_SOURCE ${TEST_SOURCES})
    # Get test name from filename
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    
    # Create test executable
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    
    # Link to RTSP library
    target_link_libraries(${TEST_NAME} rtsp)
    
    # Set include directories
    target_include_directories(${TEST_NAME} PRIVATE 
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/rtsp
    )
    
    # Set C++ standard
    target_compile_features(${TEST_NAME} PRIVATE cxx_std_17)
    
    # Set output directory
    set_target_properties(${TEST_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    )
    
    # Add test to CTest
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    
    message(STATUS "Added test: ${TEST_NAME}")
endforeach()

# Create a custom target to build all tests
add_custom_target(build-tests)
foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_dependencies(build-tests ${TEST_NAME})
endforeach()

# Create convenience targets
add_custom_target(run-all-tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS build-tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all RTSP tests..."
)

message(STATUS "Tests will be built in: ${CMAKE_BINARY_DIR}/tests")
