FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    gdb \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

CMD ["bash", "-c", "cmake -B build -G Ninja && cmake --build build"]
