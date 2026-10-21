# CMakeList.txt : CMake project for filters, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.13)

cmake_policy(SET CMP0079 NEW)

# Add source to this project's executable.
file(GLOB_RECURSE sources ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/*.c ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE ${sources})

include(FetchContent)

FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG main
)
FetchContent_MakeAvailable(fmt)

target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt-header-only)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(BUILD_EXAMPLES OFF)
set(BUILD_TESTS OFF)

option(BUILD_TESTS "Build tests" OFF)
option(BUILD_EXAMPLES "Build examples" OFF)

FetchContent_Declare(
  AudioFile
  GIT_REPOSITORY https://github.com/adamstark/AudioFile.git
)
FetchContent_MakeAvailable(AudioFile)

target_link_libraries(${PROJECT_NAME} PRIVATE AudioFile)