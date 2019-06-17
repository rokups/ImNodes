//
// Copyright (c) 2019 Rokas Kupstys.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
//
#pragma once

#include "ImNodes.h"

namespace ImNodes
{

/// This namespace includes functions for easily creating nodes. They implement a somewhat nice node layout. If you need
/// a quick solution - use easy nodes. If you want to customize node look - use lower level node functions from ImNodes.h
namespace Ez
{

struct SlotInfo
{
    /// Slot title, will be displayed on the node.
    const char* title;
    /// Slot kind, will be used for matching connections to slots of same kind.
    int kind;
};

/// Begin rendering of node in a graph. Render node content when returns `true`.
IMGUI_API bool BeginNode(void* node_id, const char* title, ImVec2* pos, bool* selected);
/// Terminates current node. Should be called regardless of BeginNode() returns value.
IMGUI_API void EndNode();
/// Renders input slot region. Kind is unique value whose sign is ignored.
IMGUI_API void InputSlots(const SlotInfo* slots, int snum);
/// Renders output slot region. Kind is unique value whose sign is ignored.
IMGUI_API void OutputSlots(const SlotInfo* slots, int snum);

}

}
