# RTSP VOD Server Application
# A full-featured video-on-demand server supporting H264 and MPEG-TS files

set(VOD_SOURCES
    main.cpp
    aac_file_reader.cpp
    h264_file_reader.cpp
    ts_file_reader.cpp
    session_aac_reader.cpp
    session_h264_reader.cpp
    session_ts_reader.cpp
    file_manager.cpp
    session_manager.cpp
    session_aac_worker_thread.cpp
    session_worker_thread.cpp
    session_ts_worker_thread.cpp
)

add_executable(rtsp_vod_server ${VOD_SOURCES})

target_include_directories(rtsp_vod_server PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(rtsp_vod_server PRIVATE lmrtsp)

set_target_properties(rtsp_vod_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

install(TARGETS rtsp_vod_server
    RUNTIME DESTINATION bin
)

