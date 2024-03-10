cmake_minimum_required(VERSION 3.23)

set(CMAKE_C_STANDARD 11)

project(
  microtar
  VERSION 0.1.0
  DESCRIPTION ""
  LANGUAGES C)

OPTION(MICROTAR_SHARED "Build a shared library" OFF)

if(MICROTAR_SHARED)
  add_library(microtar SHARED)
else()
  add_library(microtar STATIC)
endif()

target_include_directories(microtar
  PUBLIC
    src
)

target_sources(microtar
  PRIVATE
    src/microtar.c
)