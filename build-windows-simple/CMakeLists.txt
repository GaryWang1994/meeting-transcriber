cmake_minimum_required(VERSION 3.10)
project(MeetingTranscriber VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_EXECUTABLE_SUFFIX ".exe")

set(SOURCES
    ${CMAKE_SOURCE_DIR}/../src/main.cpp
    ${CMAKE_SOURCE_DIR}/../src/utils.cpp
)

add_executable(meeting-transcriber ${SOURCES})

target_include_directories(meeting-transcriber PRIVATE ${CMAKE_SOURCE_DIR}/../include)

target_compile_definitions(meeting-transcriber PRIVATE _WIN32 WIN32)
