# RTSP Client Application
# A simple RTSP client for pulling streams and saving to file

add_executable(rtsp_client main.cpp)

target_include_directories(rtsp_client PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(rtsp_client PRIVATE lmrtsp)

set_target_properties(rtsp_client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

install(TARGETS rtsp_client
    RUNTIME DESTINATION bin
)

