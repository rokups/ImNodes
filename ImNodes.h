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
// Colors:
//  * ImGuiCol_PlotLines - for connector dots and connection lines.
//  * ImGuiCol_PlotLinesHovered - for connector dots and connection lines when they are hovered/active.
//  * ImGuiCol_Border - for node and connector borders.
//  * ImGuiCol_FrameBg - for node background.
//  * ImGuiCol_FrameBgActive - selected node background.
//  * ImGuiCol_Separator - color of grid lines.
//  * FrameRounding - node border rounding.

namespace ImNodes
{

static const float slot_circle_radius = 5.f;
static const ImVec2 node_offscreen_position{-999999.f, -999999.f};

enum SlotType : unsigned
{
    InputSlot,
    OutputSlot,
};

struct _CanvasStateImpl;

struct CanvasState
{
    /// Current zoom of canvas.
    float zoom = 1.0;
    /// Current scroll offset of canvas.
    ImVec2 offset{};
    /// Implementation detail.
    _CanvasStateImpl* _impl = nullptr;

    CanvasState() noexcept;
    ~CanvasState();
};

struct NodeInfo
{
    /// Title that will be rendered at the top of node.
    const char* title = nullptr;
    /// Node position on canvas.
    ImVec2 pos = node_offscreen_position;
    /// Set to `true` when current node is selected.
    bool selected = false;

    NodeInfo() = default;
    NodeInfo(const NodeInfo& other) = default;
};

struct SlotInfo
{
    /// Title that will be rendered next to slot connector.
    const char* title = nullptr;
    /// Only slots of same kind can be connected.
    int kind = 0;
};

struct Connection
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
void BeginCanvas(CanvasState* canvas);
/// Terminate a node graph canvas that was created by calling BeginCanvas().
void EndCanvas();
/// Begin rendering of node in a graph. Render custom node widgets and handle connections when this function returns `true`.
/// \param node is a state that should be preserved across sessions.
/// \param inputs is an array of slot descriptors for inputs (left side).
/// \param inum is a number of descriptors in `inputs` array.
/// \param outputs is an array of slot descriptors for outputs (right side).
/// \param onum is a number of descriptors in `outputs` array.
/// \param connections is an array of current active connections.
/// \param cnum is a number of connections in `connections` array.
bool BeginNode(NodeInfo* node, SlotInfo* inputs, int inum, SlotInfo* outputs, int onum, Connection* connections, int cnum);
/// Terminates current node. Should only be called when BeginNode() returns `true`.
void EndNode();
/// Returns `true` when new connection is made. Connection information is returned into `connection` parameter. Must be
/// called at id scope created by BeginNode().
bool GetNewConnection(Connection* connection);
/// Returns a connection from `connections` array passed to BeginNode() when that connection is deleted. Must be called
/// at id scope created by BeginNode().
Connection* GetDeleteConnection();

}   // namespace ImNodes
