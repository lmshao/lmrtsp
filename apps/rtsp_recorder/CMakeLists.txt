# RTSP Recorder Application
# A tool for pulling RTSP streams and recording to file

add_executable(rtsp_recorder main.cpp)

target_include_directories(rtsp_recorder PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(rtsp_recorder PRIVATE lmrtsp)

set_target_properties(rtsp_recorder PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

install(TARGETS rtsp_recorder
    RUNTIME DESTINATION bin
)

