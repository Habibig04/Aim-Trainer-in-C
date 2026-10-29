cmake_minimum_required(VERSION 3.14)
project(aim_trainer C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# AUTO | 3.3 | 2.1 | "ES 2.0" | "ES 3.0"
set(RAYLIB_GL "AUTO" CACHE STRING "OpenGL backend raylib is built for")
option(USE_SYSTEM_RAYLIB "Use an installed raylib instead of building one" OFF)

if(USE_SYSTEM_RAYLIB)
  find_package(raylib REQUIRED)
else()
  if(RAYLIB_GL STREQUAL "AUTO")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm")
      set(GL_CHOICE "ES 2.0")
    else()
      set(GL_CHOICE "3.3")
    endif()
  else()
    set(GL_CHOICE "${RAYLIB_GL}")
  endif()
  message(STATUS "raylib OpenGL backend: ${GL_CHOICE}")

  set(OPENGL_VERSION "${GL_CHOICE}" CACHE STRING "" FORCE)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

  include(FetchContent)
  FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 6.0
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(raylib)
endif()

add_executable(aim_trainer aim_trainer.cpp)
target_link_libraries(aim_trainer PRIVATE raylib)