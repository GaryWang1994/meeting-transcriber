#!/bin/bash
set -e

echo "===================================="
echo "Windows Build with Docker"
echo "===================================="
echo ""

PROJECT_DIR="/home/garywang/Desktop/project/meeting-transcriber"
BUILD_SCRIPT="$PROJECT_DIR/docker-build.sh"

# Check if Docker is installed
if ! command -v docker &> /dev/null; then
    echo "Installing Docker..."
    sudo apt-get update -qq
    sudo apt-get install -y -qq docker.io
    sudo systemctl start docker
    sudo usermod -aG docker $USER
    echo "Docker installed. Please log out and log back in, then re-run this script."
    exit 1
fi

echo "Building Windows executable using Docker..."
echo ""

# Create a Dockerfile
cat > "$PROJECT_DIR/Dockerfile.windows" << 'DOCKERFILE'
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    mingw-w64 \
    cmake \
    make \
    curl \
    unzip \
    zip \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . .

RUN mkdir -p build-windows && cd build-windows && \
    cmake .. \
        -DCMAKE_SYSTEM_NAME=Windows \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
        -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
        -DONNXRUNTIME_ROOT="" && \
    make -j$(nproc) || true

RUN if [ -f build-windows/meeting-transcriber.exe ]; then \
        mkdir -p /output && \
        cp build-windows/meeting-transcriber.exe /output/ && \
        echo "Build successful"; \
    else \
        echo "Build may have failed or produced warnings"; \
    fi
DOCKERFILE

# Build Docker image
echo "Building Docker image..."
cd "$PROJECT_DIR"
sudo docker build -t meeting-transcriber-windows -f Dockerfile.windows . 2>&1 | tail -50

# Extract the executable
echo ""
echo "Extracting Windows executable..."
sudo docker create --name temp-extract meeting-transcriber-windows
sudo docker cp temp-extract:/output/meeting-transcriber.exe "$PROJECT_DIR/" 2>/dev/null || true
sudo docker rm temp-extract

# Check if file was extracted
if [ -f "$PROJECT_DIR/meeting-transcriber.exe" ]; then
    echo ""
    echo "===================================="
    echo "Build Successful!"
    echo "===================================="
    ls -lh "$PROJECT_DIR/meeting-transcriber.exe"
    echo ""
    echo "Windows executable: $PROJECT_DIR/meeting-transcriber.exe"
else
    echo ""
    echo "Build completed but executable not found."
    echo "Check Docker logs for details."
fi
