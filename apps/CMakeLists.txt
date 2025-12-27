# LMRTSP Applications
if(BUILD_APPS)
    message(STATUS "Building lmrtsp applications...")

    # RTSP VOD Server - Full-featured video-on-demand server
    add_subdirectory(rtsp_vod_server)
    
    # RTSP Recorder - Stream pulling and recording
    add_subdirectory(rtsp_recorder)
    
    # RTP Pusher - RTP stream pushing tool
    add_subdirectory(rtp_pusher)
    
    # RTP Receiver - RTP stream receiving tool
    add_subdirectory(rtp_receiver)

    message(STATUS "Applications will be built in: ${CMAKE_BINARY_DIR}/bin")
endif()