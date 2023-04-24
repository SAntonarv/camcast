cmake_minimum_required(VERSION 3.21)
project(camcast)

option(USE_STATIC "Use static library" ON)

set(CMAKE_CXX_STANDARD 14)
set(jsonlib /usr/lib/aarch64-linux-gnu)
set(syslib /home/armbian/ffmpeg-5.1)
find_package(${jsonlib})

find_library(LIBAVFORMAT libavformat.a ${syslib}/libavformat)
find_library(LIBAVCODEC libavcodec.a ${syslib}/libavcodec)
find_library(LIBAVUTIL libavutil.a ${syslib}/libavutil)
find_library(LIBSWRESAMPLE libswresample.a ${syslib}/libswresample)

add_executable(camcast main.cpp)

target_link_libraries(camcast PRIVATE
        ${LIBAVFORMAT}
        ${LIBAVCODEC}
        ${LIBAVUTIL}
        ${LIBSWRESAMPLE}
        "libjsoncpp.so")
target_include_directories(camcast PUBLIC ${syslib})

