cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(FirefoxPause VERSION 1.0.0)

file(GLOB SOURCES src/*.cpp)
add_library(${PROJECT_NAME} SHARED ${SOURCES})

if (NOT DEFINED ENV{GEODE_SDK})
	message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable.")
endif()

# Work around fmt 11.0.2 failing to compile under Clang 20+ (consteval format-string checks).
# Must come BEFORE add_subdirectory so fmt inherits it.
add_compile_definitions(FMT_USE_CONSTEVAL=0)

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

if (WIN32)
	target_link_libraries(${PROJECT_NAME} ws2_32)
endif()

setup_geode_mod(${PROJECT_NAME})
