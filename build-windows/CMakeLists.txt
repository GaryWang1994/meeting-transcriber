# Windows Build Configuration for Meeting Transcriber

# Minimal CMake for Windows cross-compilation
# This avoids complex dependencies that cause header conflicts

cmake_minimum_required(VERSION 3.10)
project(MeetingTranscriber VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Release)

# Windows executable settings
set(CMAKE_EXECUTABLE_SUFFIX ".exe")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Source files - core functionality only
set(SOURCES
    ${CMAKE_SOURCE_DIR}/../src/main.cpp
    ${CMAKE_SOURCE_DIR}/../src/utils.cpp
)

# Create executable
add_executable(meeting-transcriber ${SOURCES})

# Include directories
target_include_directories(meeting-transcriber PRIVATE 
    ${CMAKE_SOURCE_DIR}/../include
)

# Windows specific definitions
target_compile_definitions(meeting-transcriber PRIVATE
    _WIN32
    WIN32
    _WINDOWS
)

# Static linking for portability
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")

# Print configuration
message(STATUS "")
message(STATUS "Windows Build Configuration:")
message(STATUS "  Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "  Output: meeting-transcriber.exe")
message(STATUS "  Type: Minimal build (no dependencies)")
message(STATUS "")
