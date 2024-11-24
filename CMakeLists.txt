
cmake_minimum_required(VERSION 3.15)
project(CvStabilize VERSION 1.0 LANGUAGES CXX)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define source files
set(SOURCES stabilize.cpp)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/release)

# Include directories
#include_directories(/usr/local/include/opencv4 /opt/homebrew/Cellar/ffmpeg/*/include)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

# Link libraries (use pkg-config for OpenCV if available)
find_package(OpenCV REQUIRED)
find_package(Threads REQUIRED)

# Compiler options
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gdwarf-4 -g3")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif()

# Add the executable target
add_executable(${PROJECT_NAME} ${SOURCES})

# Link necessary libraries
target_link_libraries(${PROJECT_NAME}
  Threads::Threads
  ${OpenCV_LIBS}
)

