# RTP Pusher Application
# A tool for pushing RTP streams

add_executable(rtp_pusher main.cpp)

target_include_directories(rtp_pusher PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(rtp_pusher PRIVATE lmrtsp)

set_target_properties(rtp_pusher PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

install(TARGETS rtp_pusher
    RUNTIME DESTINATION bin
)

