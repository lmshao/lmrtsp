# CMakeLists.txt for LMRTSP Applications
cmake_minimum_required(VERSION 3.10)

# Option to build applications
option(BUILD_APPS "Build lmrtsp applications (rtsp client/server + rtp pusher/receiver)" ON)

if(BUILD_APPS)
    message(STATUS "Building lmrtsp applications...")

    # RTSP applications directory
    set(RTSP_APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rtsp)

    # RTP applications directory
    set(RTP_APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rtp)

    # RTSP Server
    if(EXISTS "${RTSP_APP_DIR}/rtsp_server.cpp")
        add_executable(rtsp_server
            rtsp/rtsp_server.cpp
            rtsp/h264_file_reader.cpp
        )
        target_include_directories(rtsp_server PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/lmnet/include
            ${CMAKE_CURRENT_SOURCE_DIR}/rtsp
        )
        target_link_libraries(rtsp_server PRIVATE lmrtsp)

        set_target_properties(rtsp_server PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/apps
        )

        message(STATUS "Added application: rtsp_server")
    endif()

    # RTSP Client
    if(EXISTS "${RTSP_APP_DIR}/rtsp_client.cpp")
        add_executable(rtsp_client
            rtsp/rtsp_client.cpp
        )
        target_include_directories(rtsp_client PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/lmnet/include
            ${CMAKE_CURRENT_SOURCE_DIR}/rtsp
        )
        target_link_libraries(rtsp_client PRIVATE lmrtsp)

        set_target_properties(rtsp_client PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/apps
        )

        message(STATUS "Added application: rtsp_client")
    endif()

    # RTP Pusher (H.264 stream pusher)
    if(EXISTS "${RTP_APP_DIR}/rtp_pusher.cpp")
        add_executable(rtp_pusher
            rtp/rtp_pusher.cpp
        )
        target_include_directories(rtp_pusher PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/lmnet/include
            ${CMAKE_CURRENT_SOURCE_DIR}/rtp
        )
        target_link_libraries(rtp_pusher PRIVATE lmrtsp)

        set_target_properties(rtp_pusher PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/apps
        )

        message(STATUS "Added application: rtp_pusher")
    endif()

    # RTP Receiver (H.264 stream receiver)
    if(EXISTS "${RTP_APP_DIR}/rtp_receiver.cpp")
        add_executable(rtp_receiver
            rtp/rtp_receiver.cpp
        )
        target_include_directories(rtp_receiver PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/lmnet/include
            ${CMAKE_CURRENT_SOURCE_DIR}/rtp
        )
        target_link_libraries(rtp_receiver PRIVATE lmrtsp)

        set_target_properties(rtp_receiver PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/apps
        )

        message(STATUS "Added application: rtp_receiver")
    endif()

    # Create a custom target to build all applications
    add_custom_target(apps)
    if(TARGET rtsp_server)
        add_dependencies(apps rtsp_server)
    endif()
    if(TARGET rtsp_client)
        add_dependencies(apps rtsp_client)
    endif()
    if(TARGET rtp_pusher)
        add_dependencies(apps rtp_pusher)
    endif()
    if(TARGET rtp_receiver)
        add_dependencies(apps rtp_receiver)
    endif()

    # Create custom targets for specific application groups
    add_custom_target(rtsp_apps)
    if(TARGET rtsp_server)
        add_dependencies(rtsp_apps rtsp_server)
    endif()
    if(TARGET rtsp_client)
        add_dependencies(rtsp_apps rtsp_client)
    endif()

    add_custom_target(rtp_apps)
    if(TARGET rtp_pusher)
        add_dependencies(rtp_apps rtp_pusher)
    endif()
    if(TARGET rtp_receiver)
        add_dependencies(rtp_apps rtp_receiver)
    endif()

    message(STATUS "Applications will be built in: ${CMAKE_BINARY_DIR}/apps")
    message(STATUS "Build targets: apps, rtsp_apps, rtp_apps")
endif()