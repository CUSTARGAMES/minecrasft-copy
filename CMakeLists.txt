cmake_minimum_required(VERSION 3.10)
project(Minecraft)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")

# Find raylib
find_package(raylib REQUIRED)

# Create executable
add_executable(minecraft minecraft.c)

# Link raylib
target_link_libraries(minecraft raylib m)

# For Windows, add required libraries
if(WIN32)
    target_link_libraries(minecraft opengl32 glu32 winmm)
endif()
