cmake_minimum_required(VERSUION 3.10)
project(os_lab4)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MinGW settings
if(MINGW OR WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mwindows -static-libgcc -static-libstdc++")
endif()

# Common include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Executable Receiver file
add_executable(receiver src/receiver.cpp)
target_link_libraries(receiver)

# Executable Sender file
add_executable(sender src/sender.cpp)
target_link_libraries(sender)

