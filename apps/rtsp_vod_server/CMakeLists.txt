# RTSP VOD Server Application
# A full-featured video-on-demand server supporting H264, H265, AAC, MPEG-TS, and MKV files

# Try to find installed lmmkv library, fallback to local source if not found
find_package(lmmkv QUIET)
if(lmmkv_FOUND)
    message(STATUS "rtsp_vod_server: Using system-installed lmmkv library")
    set(LMMKV_SOURCE "system")
else()
    # Check if local lmmkv directory exists
    set(LMMKV_LOCAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../lmmkv")
    if(EXISTS "${LMMKV_LOCAL_DIR}/CMakeLists.txt")
        if(NOT TARGET lmmkv)
            add_subdirectory(${LMMKV_LOCAL_DIR} lmmkv_build)
        endif()
        message(STATUS "rtsp_vod_server: Using local lmmkv from sibling directory")
        set(LMMKV_SOURCE "local")
    else()
        # Neither system nor local lmmkv found - show helpful warning
        message(WARNING
            "rtsp_vod_server: Cannot find lmmkv dependency!\n"
            "\n"
            "Searched locations:\n"
            "  1. System installation (using find_package lmmkv)\n"
            "  2. Local directory: ${LMMKV_LOCAL_DIR}\n"
            "\n"
            "MKV support will be disabled.\n"
            "\n"
            "Solution - Download lmmkv to sibling directory:\n"
            "\n"
            "    cd ${CMAKE_CURRENT_SOURCE_DIR}/../../..\n"
            "    git clone https://github.com/lmshao/lmmkv.git\n"
            "\n"
            "After downloading, re-run: cmake .."
        )
        set(LMMKV_SOURCE "none")
    endif()
endif()

set(VOD_SOURCES
    main.cpp
    aac_file_reader.cpp
    h264_file_reader.cpp
    h265_file_reader.cpp
    ts_file_reader.cpp
    session_aac_reader.cpp
    session_h264_reader.cpp
    session_h265_reader.cpp
    session_ts_reader.cpp
    file_manager.cpp
    session_manager.cpp
    base_session_worker_thread.cpp
    session_aac_worker_thread.cpp
    session_h264_worker_thread.cpp
    session_h265_worker_thread.cpp
    session_ts_worker_thread.cpp
)

# Add MKV support sources if lmmkv is available
if(NOT LMMKV_SOURCE STREQUAL "none")
    list(APPEND VOD_SOURCES
        session_mkv_reader.cpp
        session_mkv_worker_thread.cpp
    )
    message(STATUS "rtsp_vod_server: MKV support enabled (using ${LMMKV_SOURCE} lmmkv)")
else()
    message(STATUS "rtsp_vod_server: MKV support disabled (lmmkv not found)")
endif()

add_executable(rtsp_vod_server ${VOD_SOURCES})

target_include_directories(rtsp_vod_server PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link lmrtsp library
target_link_libraries(rtsp_vod_server PRIVATE lmrtsp)

# Link lmmkv if available
if(NOT LMMKV_SOURCE STREQUAL "none")
    if(lmmkv_FOUND)
        target_link_libraries(rtsp_vod_server PRIVATE
            $<IF:$<TARGET_EXISTS:lmmkv::lmmkv_static>,lmmkv::lmmkv_static,lmmkv::lmmkv>
        )
    else()
        target_link_libraries(rtsp_vod_server PRIVATE lmmkv_static)
    endif()
    
    # Define preprocessor macro to enable MKV support
    target_compile_definitions(rtsp_vod_server PRIVATE ENABLE_MKV_SUPPORT)
endif()

set_target_properties(rtsp_vod_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

install(TARGETS rtsp_vod_server
    RUNTIME DESTINATION bin
)

