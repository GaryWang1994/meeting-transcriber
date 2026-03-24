cmake_minimum_required(VERSION 3.15)
project(MeetingTranscriber VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Options
option(USE_CUDA "Enable CUDA support for ONNX Runtime" OFF)
option(USE_OPENVINO "Enable OpenVINO support for ONNX Runtime" OFF)

# Find required packages
find_package(OpenMP REQUIRED)

# Find FFmpeg
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavformat
    libavcodec
    libswresample
    libavutil
)

# Find ONNX Runtime
# Note: User may need to set ONNXRUNTIME_ROOT to their installation path
set(ONNXRUNTIME_ROOT "" CACHE PATH "Path to ONNX Runtime installation")

if(NOT ONNXRUNTIME_ROOT)
    message(WARNING "ONNXRUNTIME_ROOT not set, trying to find automatically...")
endif()

find_path(ONNXRUNTIME_INCLUDE_DIR
    NAMES onnxruntime_cxx_api.h
    PATHS ${ONNXRUNTIME_ROOT}/include
    DOC "ONNX Runtime include directory"
)

find_library(ONNXRUNTIME_LIBRARY
    NAMES onnxruntime onnxruntime_providers_shared
    PATHS ${ONNXRUNTIME_ROOT}/lib
    DOC "ONNX Runtime library"
)

if(NOT ONNXRUNTIME_INCLUDE_DIR OR NOT ONNXRUNTIME_LIBRARY)
    message(FATAL_ERROR "ONNX Runtime not found. Please set ONNXRUNTIME_ROOT to your ONNX Runtime installation path.")
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/audio_processor.cpp
    src/asr_engine.cpp
    src/speaker_diarization.cpp
    src/transcript_generator.cpp
    src/utils.cpp
)

# Header files (for IDEs)
set(HEADERS
    include/audio_processor.h
    include/asr_engine.h
    include/speaker_diarization.h
    include/transcript_generator.h
    include/utils.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${FFMPEG_INCLUDE_DIRS}
        ${ONNXRUNTIME_INCLUDE_DIR}
)

# Link libraries
target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_FFMPEG=1)
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        PkgConfig::FFMPEG
        ${ONNXRUNTIME_LIBRARY}
        OpenMP::OpenMP_CXX
)

# Platform-specific settings
if(WIN32)
    # Windows specific settings
    set_target_properties(${PROJECT_NAME} PROPERTIES
        OUTPUT_NAME "meeting-transcriber"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    
    # Copy DLLs to output directory on Windows
    if(ONNXRUNTIME_ROOT)
        file(GLOB ONNXRUNTIME_DLLS "${ONNXRUNTIME_ROOT}/lib/*.dll")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${ONNXRUNTIME_DLLS}
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        )
    endif()
else()
    # Linux/macOS settings
    set_target_properties(${PROJECT_NAME} PROPERTIES
        OUTPUT_NAME "meeting-transcriber"
    )
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Installation
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include/meeting-transcriber
)

# Print summary
message(STATUS "")
message(STATUS "Configuration Summary:")
message(STATUS "  Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  ONNX Runtime: ${ONNXRUNTIME_LIBRARY}")
message(STATUS "  CUDA Support: ${USE_CUDA}")
message(STATUS "  OpenVINO Support: ${USE_OPENVINO}")
message(STATUS "")
