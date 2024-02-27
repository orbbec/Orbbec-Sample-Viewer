# Copyright (c) ORBBEC. All rights reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.1.15)

# Define project name
project(orbbec_sample_viewer VERSION 1.0 LANGUAGES C CXX)

# Find Orbbec SDK, you may need to set OrbbecSDK_DIR variable
if(NOT DEFINED OrbbecSDK_FOUND)
    # set(OrbbecSDK_DIR "your/path/to/OrbbecSDK")
    message(STATUS "OrbbecSDK_DIR: ${OrbbecSDK_DIR}")
    find_package(OrbbecSDK REQUIRED)
endif()

# Find OpenCV, you may need to set OpenCV_DIR variable
# set(OpenCV_DIR "your/path/to/opencv/library")
find_package(OpenCV 4 QUIET)

# Find OpenGL
find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})

# Subdirectories
add_subdirectory(extern)
add_subdirectory(src)
