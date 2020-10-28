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
set(IMNODES_IMGUI_VERSION "1.79" CACHE STRING "ImGui version to use for building sample,")

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

if (NOT IMGUI_DIR)
    if ("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
        set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
        set (IMGUI_DIR "${CMAKE_CURRENT_BINARY_DIR}/imgui-${IMNODES_IMGUI_VERSION}")

        # Build samples and pull in engine dependency only when this library is built
        # independently. These are not necessary when user is consuming library in their
        # own project.
        if (NOT EXISTS "${IMGUI_DIR}.zip")
            file(DOWNLOAD "https://github.com/ocornut/imgui/archive/v${IMNODES_IMGUI_VERSION}.zip" ${IMGUI_DIR}.zip)
        endif ()
        if (NOT EXISTS "${IMGUI_DIR}")
            execute_process(COMMAND ${CMAKE_COMMAND} -E tar xzf ${IMGUI_DIR}.zip)
        endif ()
    endif ()
endif ()

if (UNIX)
    add_executable(ImNodes
        ${IMGUI_DIR}/examples/example_sdl_opengl3/main.cpp
        ${IMGUI_DIR}/examples/imgui_impl_opengl3.cpp
        ${IMGUI_DIR}/examples/imgui_impl_opengl3.h
        ${IMGUI_DIR}/examples/imgui_impl_sdl.cpp
        ${IMGUI_DIR}/examples/imgui_impl_sdl.h
        ${IMGUI_DIR}/imconfig.h
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui.h
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_internal.h
        ${IMGUI_DIR}/imstb_rectpack.h
        ${IMGUI_DIR}/imstb_textedit.h
        ${IMGUI_DIR}/imstb_truetype.h
        ${IMGUI_DIR}/examples/libs/gl3w/GL/gl3w.c
        ${IMGUI_DIR}/examples/libs/gl3w/GL/gl3w.h
        ${IMGUI_DIR}/examples/libs/gl3w/GL/glcorearb.h
        ImNodes.h
        ImNodes.cpp
        ImNodesEz.h
        ImNodesEz.cpp
        sample.cpp
    )
    target_include_directories(ImNodes PUBLIC
        ${SDL2_INCLUDE_DIRS}
        ${IMGUI_DIR}
        ${IMGUI_DIR}/examples
        ${IMGUI_DIR}/examples/libs/gl3w
    )
    target_link_libraries(ImNodes PUBLIC ${SDL2_LIBRARIES} -ldl -lGL)
    target_compile_options(ImNodes PUBLIC ${SDL2_CFLAGS_OTHER})
    target_compile_definitions(ImNodes PUBLIC -DIMGUI_DISABLE_OBSOLETE_FUNCTIONS=1 -DIMGUI_IMPL_OPENGL_LOADER_GL3W=1)
endif ()
