# Copyright (c) ORBBEC. All rights reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.1.15)

# Define project name
project(orbbec_sample_viewer VERSION 1.0 LANGUAGES C CXX)

# Clone Orbbec SDK from git repository to extern and set OrbbecSDK_DIR to it
execute_process(
    COMMAND git clone https://github.com/orbbec/OrbbecSDK.git
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/extern"
)
set(OrbbecSDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/OrbbecSDK")
message(STATUS "OrbbecSDK_DIR: ${OrbbecSDK_DIR}")
find_package(OrbbecSDK REQUIRED)

# Find OpenCV, you may need to set OpenCV_DIR variable
# set(OpenCV_DIR "your/path/to/opencv/library")
# For example:
set(OpenCV_DIR "C:/OpenCV4.4/opencv/build")
find_package(OpenCV 4 REQUIRED)

# Find OpenGL
find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})

# Subdirectories
add_subdirectory(extern)
add_subdirectory(src)
