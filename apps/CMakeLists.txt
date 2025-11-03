# LMRTSP Applications
if(BUILD_APPS)
    message(STATUS "Building lmrtsp applications...")

    # Common settings for all applications
    set(APP_OUTPUT_DIR ${CMAKE_BINARY_DIR}/apps)

    # Helper function to create an application target
    function(add_lmrtsp_app APP_NAME)
        add_executable(${APP_NAME} ${ARGN})
        target_include_directories(${APP_NAME} PRIVATE
            ${CMAKE_SOURCE_DIR}/include
        )
        target_link_libraries(${APP_NAME} PRIVATE lmrtsp)
        set_target_properties(${APP_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${APP_OUTPUT_DIR}
        )
        message(STATUS "Added application: ${APP_NAME}")
    endfunction()

    # RTSP applications
    add_lmrtsp_app(rtsp_server rtsp/rtsp_server.cpp rtsp/h264_file_reader.cpp)
    add_lmrtsp_app(rtsp_vod_server 
        rtsp/rtsp_vod_server.cpp 
        rtsp/h264_file_reader.cpp
        rtsp/file_manager.cpp
        rtsp/session_h264_reader.cpp
        rtsp/session_worker_thread.cpp
        rtsp/session_manager.cpp)
    add_lmrtsp_app(rtsp_client rtsp/rtsp_client.cpp)

    # RTP applications
    add_lmrtsp_app(rtp_pusher rtp/rtp_pusher.cpp)
    add_lmrtsp_app(rtp_receiver rtp/rtp_receiver.cpp)

    message(STATUS "Applications will be built in: ${APP_OUTPUT_DIR}")
endif()