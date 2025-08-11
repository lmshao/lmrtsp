
cmake_minimum_required(VERSION 3.10)
project(rtsp VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include/rtsp)
include_directories(${PROJECT_SOURCE_DIR}/network/include)

add_library(rtsp STATIC src/rtsp/rtsp.cpp)
target_include_directories(rtsp PUBLIC include network/include)

add_executable(rtsp-server src/server/main.cpp)
target_include_directories(rtsp-server PRIVATE include network/include)
target_link_libraries(rtsp-server PRIVATE rtsp)

