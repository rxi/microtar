# CMakeLists.txt --- CMake project settings

# CMake minimum version
cmake_minimum_required(VERSION 2.4)

if (COMMAND cmake_policy)
    cmake_policy(SET CMP0048 NEW)
endif()

# enable testing
enable_testing()

# project name and language
project(microtar LANGUAGES C VERSION 0.1.0)

##############################################################################

subdirs(src)

##############################################################################
