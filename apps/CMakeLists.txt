# CMakeLists.txt for RTSP Applications
cmake_minimum_required(VERSION 3.10)

# Option to build applications
option(BUILD_APPS "Build RTSP applications (server/client)" ON)

if(BUILD_APPS)
    message(STATUS "Building RTSP applications...")

    # RTSP Server
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/server/main.cpp")
        add_executable(rtsp-server server/main.cpp)
        target_include_directories(rtsp-server PRIVATE 
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/network/include
            ${CMAKE_SOURCE_DIR}/src/rtsp
        )
        target_link_libraries(rtsp-server PRIVATE rtsp)
        
        set_target_properties(rtsp-server PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )
        
        message(STATUS "Added application: rtsp-server")
    endif()
    
    # RTSP Client (placeholder for future implementation)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/client/main.cpp")
        add_executable(rtsp-client client/main.cpp)
        target_include_directories(rtsp-client PRIVATE 
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/network/include
        )
        target_link_libraries(rtsp-client PRIVATE rtsp)
        
        set_target_properties(rtsp-client PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )
        
        message(STATUS "Added application: rtsp-client")
    endif()
    
    # Create a custom target to build all applications
    add_custom_target(apps)
    if(TARGET rtsp-server)
        add_dependencies(apps rtsp-server)
    endif()
    if(TARGET rtsp-client)
        add_dependencies(apps rtsp-client)
    endif()
    
    message(STATUS "Applications will be built in: ${CMAKE_BINARY_DIR}/bin")
endif()
