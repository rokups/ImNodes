#
# Copyright (c) 2019 Rokas Kupstys.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

cmake_minimum_required(VERSION 3.1)
project(ImNodes)

set(CMAKE_CXX_STANDARD 14)
set(IMNODES_IMGUI_VERSION "v1.80" CACHE STRING "ImGui version to use for building sample,")

if (NOT IMGUI_DIR)
    if ("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
        set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
        set (IMGUI_DIR "${CMAKE_CURRENT_BINARY_DIR}/imgui-${IMNODES_IMGUI_VERSION}")

        # Build samples and pull in engine dependency only when this library is built
        # independently. These are not necessary when user is consuming library in their
        # own project.
        if (NOT EXISTS "${IMGUI_DIR}.zip")
            file(DOWNLOAD "https://github.com/ocornut/imgui/archive/${IMNODES_IMGUI_VERSION}.zip" ${IMGUI_DIR}.zip)
        endif ()
        if (NOT EXISTS "${IMGUI_DIR}")
            execute_process(COMMAND ${CMAKE_COMMAND} -E tar xzf ${IMGUI_DIR}.zip)
        endif ()
    endif ()
endif ()

if (NOT EXISTS "${IMGUI_DIR}/CMakeLists.txt")
    file(DOWNLOAD "https://gist.githubusercontent.com/rokups/f771217b2d530d170db5cb1e08e9a8f4/raw/b714a5c8208a57a2ed43ac7d38026bc5fb5bbbf0/CMakeLists.txt" "${IMGUI_DIR}/CMakeLists.txt")
endif ()

set(IMGUI_DISABLE_OBSOLETE_FUNCTIONS ON)
add_subdirectory(${IMGUI_DIR})

# Warnings.
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Werror -Wunused-parameter -Wformat $<$<COMPILE_LANGUAGE:CXX>:-pedantic>)
endif ()

add_executable(ImNodes
    ImNodes.h
    ImNodes.cpp
    ImNodesEz.h
    ImNodesEz.cpp
    sample.cpp
)

target_link_libraries(ImNodes PRIVATE imgui)
if (TARGET imgui-sdl)
    target_link_libraries(ImNodes PRIVATE imgui-opengl3 imgui-sdl)
    target_sources(ImNodes PRIVATE ${IMGUI_DIR}/examples/example_sdl_opengl3/main.cpp)
elseif (TARGET imgui-glfw)
    target_link_libraries(ImNodes PRIVATE imgui-opengl3 imgui-glfw)
    target_sources(ImNodes PRIVATE ${IMGUI_DIR}/examples/example_glfw_opengl3/main.cpp)
elseif (TARGET imgui-dx11)
    target_link_libraries(ImNodes PRIVATE imgui-dx11 imgui-win32)
    target_sources(ImNodes PRIVATE ${IMGUI_DIR}/examples/example_win32_directx11/main.cpp)
else ()
    message(FATAL_ERROR "No suitable backend found.")
endif ()
