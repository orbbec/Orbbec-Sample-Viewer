# Copyright (c) ORBBEC. All rights reserved.
# Licensed under the MIT License.

# Set the C++ standard to C++ 11 and C standard to C11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 11)

# Compiler and Linker options
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -O3")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fPIC -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -O3")
set(CMAKE_BUILD_TYPE "Release")
	
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

if (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	add_compile_options(-Wall -Wextra -Wpedantic)
endif()

file(GLOB SOURCES  "${CMAKE_CURRENT_SOURCE_DIR}/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB IMGUI    "${CMAKE_SOURCE_DIR}/include/imgui/*.cpp")

if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

if(WIN32)
	execute_process(COMMAND windres orbbec_sample_viewer.rc -o icon.o)
	add_executable(${PROJECT_NAME} 
		${SOURCES} ${IMGUI}
		${CMAKE_SOURCE_DIR}/res/resource.h
		${CMAKE_SOURCE_DIR}/res/orbbec_sample_viewer.rc)
else()
	add_executable(${PROJECT_NAME} ${SOURCES} ${IMGUI})
endif()

target_include_directories(${PROJECT_NAME} PRIVATE 
     ${CMAKE_SOURCE_DIR}/include
     #${OrbbecSDK_INCLUDE_DIRS}
)

# Put all libraries into a variable
set(LIBS glfw ${OPENGL_LIBRARIES} ${OpenCV_LIBRARIES} OrbbecSDK::OrbbecSDK)

# Dependencies of this library
target_link_libraries(${PROJECT_NAME} ${LIBS})

