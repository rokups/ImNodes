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

#pragma once


#include <imgui.h>
#include <imgui_internal.h>
#include <limits>

//
// Appearance can be styled by altering imgui style before calls to ImNodes::*,
// Style:
//  * FrameRounding - node border rounding.
//

namespace ImNodes
{

/// Initial position of every node. Node placed here will be automatically moved to the mouse cursor.
static const ImVec2 NODE_OFFSCREEN_POSITION{-999999.f, -999999.f};

enum SlotType : unsigned
{
    InputSlot,
    OutputSlot,
};

enum StyleColor
{
    CanvasLines,
    NodeBg,
    NodeActiveBg,
    NodeBorder,
    Conn,
    ConnActive,
    ConnBorder,
    SelectBg,
    SelectBorder,
    ColorMax
};

struct _CanvasStateImpl;

struct IMGUI_API CanvasState
{
    /// Current zoom of canvas.
    float zoom = 1.0;
    /// Current scroll offset of canvas.
    ImVec2 offset{};
    /// Colors used to style elements of this canvas.
    ImColor colors[StyleColor::ColorMax];
    /// Implementation detail.
    _CanvasStateImpl* _impl = nullptr;

    CanvasState() noexcept;
    ~CanvasState();
};

/// Each node must have a unique NodeInfo struct.
struct IMGUI_API NodeInfo
{
    /// Title that will be rendered at the top of node.
    const char* title = nullptr;
    /// Node position on canvas.
    ImVec2 pos = NODE_OFFSCREEN_POSITION;
    /// Set to `true` when current node is selected.
    bool selected = false;
    /// A pointer that user is free to set to anything.
    void* userdata = nullptr;

    NodeInfo() = default;
    NodeInfo(const NodeInfo& other) = default;
};

/// SlotInfo struct may be shared between different nodes.
struct IMGUI_API SlotInfo
{
    /// Title that will be rendered next to slot connector.
    const char* title = nullptr;
    /// Only slots of same kind can be connected.
    int kind = 0;
};

struct IMGUI_API Connection
{
    /// `id` that was passed to BeginNode() of input node.
    NodeInfo* input_node = nullptr;
    /// Descriptor of input slot.
    SlotInfo* input_slot = nullptr;
    /// `id` that was passed to BeginNode() of output node.
    NodeInfo* output_node = nullptr;
    /// Descriptor of output slot.
    SlotInfo* output_slot = nullptr;

    bool operator ==(const Connection& other) const;
    bool operator != (const Connection& other) const;
};

/// Create a node graph canvas in current window.
IMGUI_API void BeginCanvas(CanvasState* canvas);
/// Terminate a node graph canvas that was created by calling BeginCanvas().
IMGUI_API void EndCanvas();
/// Begin rendering of node in a graph. Render custom node widgets and handle connections when this function returns `true`.
/// \param node is a state that should be preserved across sessions.
/// \param inputs is an array of slot descriptors for inputs (left side).
/// \param inum is a number of descriptors in `inputs` array.
/// \param outputs is an array of slot descriptors for outputs (right side).
/// \param onum is a number of descriptors in `outputs` array.
/// \param connections is an array of current active connections.
/// \param cnum is a number of connections in `connections` array.
IMGUI_API bool BeginNode(NodeInfo* node, SlotInfo* inputs, int inum, SlotInfo* outputs, int onum, Connection* connections, int cnum);
/// Terminates current node. Should only be called when BeginNode() returns `true`.
IMGUI_API void EndNode();
/// Returns `true` when new connection is made. Connection information is returned into `connection` parameter. Must be
/// called at id scope created by BeginNode().
IMGUI_API bool GetNewConnection(Connection* connection);
/// Returns a connection from `connections` array passed to BeginNode() when that connection is deleted. Must be called
/// at id scope created by BeginNode().
IMGUI_API Connection* GetDeleteConnection();
/// Returns active canvas state when called between BeginCanvas() and EndCanvas(). Returns nullptr otherwise. This function is not thread-safe.
IMGUI_API CanvasState* GetCurrentCanvas();

}   // namespace ImNodes
