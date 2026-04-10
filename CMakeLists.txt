cmake_minimum_required(VERSION 3.10)
project(os_lab4)

set(CMAKE_C_COMPILER /usr/bin/x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/x86_64-w64-mingw32-g++)

# MinGW settings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")

# Common include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Executable Receiver file
add_executable(receiver src/receiver.cpp)

# Executable Sender file
add_executable(sender src/sender.cpp)

set_target_properties(receiver sender PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
