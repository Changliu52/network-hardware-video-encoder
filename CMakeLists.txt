cmake_minimum_required(VERSION 3.0)

project(
    nhve
)

# use fPIC for all libraries
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# compile dependencies as static libraries
add_subdirectory(hardware-video-encoder)
add_subdirectory(minimal-latency-streaming-protocol)

# this is our main target
add_library(nhve STATIC nhve.c)
target_include_directories(nhve PRIVATE hardware-video-encoder)
target_include_directories(nhve PRIVATE minimal-latency-streaming-protocol)

# note that nhve depends through hve on FFMpeg avcodec and avutil, at least 3.4 version
target_link_libraries(nhve hve mlsp avcodec avutil)

# examples
add_executable(nhve-stream-h264 examples/nhve_stream_h264.c)
target_link_libraries(nhve-stream-h264 nhve)

add_executable(nhve-stream-hevc10 examples/nhve_stream_hevc10.c)
target_link_libraries(nhve-stream-hevc10 nhve)


