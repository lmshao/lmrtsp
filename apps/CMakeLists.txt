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
        add_executable(rtsp-server
            rtsp/rtsp_server.cpp
            rtsp/h264_file_reader.cpp
        )
        target_include_directories(rtsp-server PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/lmnet/include
            ${CMAKE_CURRENT_SOURCE_DIR}/rtsp
        )
        target_link_libraries(rtsp-server PRIVATE lmrtsp)

        set_target_properties(rtsp-server PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/apps
        )

        message(STATUS "Added application: rtsp-server")
    endif()

    # RTSP Client
    if(EXISTS "${RTSP_APP_DIR}/rtsp_client.cpp")
        add_executable(rtsp-client
            rtsp/rtsp_client.cpp
        )
        target_include_directories(rtsp-client PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/lmnet/include
            ${CMAKE_CURRENT_SOURCE_DIR}/rtsp
        )
        target_link_libraries(rtsp-client PRIVATE lmrtsp)

        set_target_properties(rtsp-client PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/apps
        )

        message(STATUS "Added application: rtsp-client")
    endif()

    # RTP Sender (H.264 stream sender)
    if(EXISTS "${RTP_APP_DIR}/rtp_sender.cpp")
        add_executable(rtp-sender
            rtp/rtp_sender.cpp
        )
        target_include_directories(rtp-sender PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/lmnet/include
            ${CMAKE_CURRENT_SOURCE_DIR}/rtp
        )
        target_link_libraries(rtp-sender PRIVATE lmrtsp)

        set_target_properties(rtp-sender PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/apps
        )

        message(STATUS "Added application: rtp-sender")
    endif()

    # RTP Receiver (H.264 stream receiver)
    if(EXISTS "${RTP_APP_DIR}/rtp_receiver.cpp")
        add_executable(rtp-receiver
            rtp/rtp_receiver.cpp
        )
        target_include_directories(rtp-receiver PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/lmnet/include
            ${CMAKE_CURRENT_SOURCE_DIR}/rtp
        )
        target_link_libraries(rtp-receiver PRIVATE lmrtsp)

        set_target_properties(rtp-receiver PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/apps
        )

        message(STATUS "Added application: rtp-receiver")
    endif()

    # Create a custom target to build all applications
    add_custom_target(apps)
    if(TARGET rtsp-server)
        add_dependencies(apps rtsp-server)
    endif()
    if(TARGET rtsp-client)
        add_dependencies(apps rtsp-client)
    endif()
    if(TARGET rtp-sender)
        add_dependencies(apps rtp-sender)
    endif()
    if(TARGET rtp-receiver)
        add_dependencies(apps rtp-receiver)
    endif()

    # Create custom targets for specific application groups
    add_custom_target(rtsp-apps)
    if(TARGET rtsp-server)
        add_dependencies(rtsp-apps rtsp-server)
    endif()
    if(TARGET rtsp-client)
        add_dependencies(rtsp-apps rtsp-client)
    endif()

    add_custom_target(rtp-apps)
    if(TARGET rtp-sender)
        add_dependencies(rtp-apps rtp-sender)
    endif()
    if(TARGET rtp-receiver)
        add_dependencies(rtp-apps rtp-receiver)
    endif()

    message(STATUS "Applications will be built in: ${CMAKE_BINARY_DIR}/apps")
    message(STATUS "Build targets: apps, rtsp-apps, rtp-apps")
endif()