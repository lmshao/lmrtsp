# RTP Receiver Application
# A tool for receiving RTP streams

add_executable(rtp_receiver main.cpp)

target_include_directories(rtp_receiver PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(rtp_receiver PRIVATE lmrtsp)

set_target_properties(rtp_receiver PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

install(TARGETS rtp_receiver
    RUNTIME DESTINATION bin
)

