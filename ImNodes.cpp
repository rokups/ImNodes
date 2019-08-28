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

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#   define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "ImNodes.h"

namespace ImNodes
{

CanvasState* gCanvas = nullptr;

bool operator ==(const ImVec2& a, const ImVec2& b)
{
    return abs(a.x - b.x) < std::numeric_limits<float>::epsilon() &&
           abs(a.y - b.y) < std::numeric_limits<float>::epsilon();
}

enum _ImNodesState
{
    State_None,
    State_Drag,
    State_Select,
};

/// Struct containing information about connection source node and slot.
struct _DragConnectionPayload
{
    /// Node id where connection started.
    void* node_id = nullptr;
    /// Source slot name.
    const char* slot_title = nullptr;
    /// Source slot kind.
    int slot_kind = 0;
};

/// Node-slot combination.
struct _IgnoreSlot
{
    /// Node id.
    void* node_id = nullptr;
    /// Slot name.
    const char* slot_name = nullptr;
    /// Slot kind. Not actual slot kind, but +1 or -1. Used to determine if slot is input or output.
    int slot_kind = 0;

    /// Equality operator, required by ImVector.
    bool operator==(const _IgnoreSlot& other) const
    {
        if (node_id != other.node_id || slot_kind != other.slot_kind)
            return false;

        if (slot_name != nullptr && other.slot_name != nullptr)
            return strcmp(slot_name, other.slot_name) == 0;

        return other.slot_name == slot_name;
    }
};

struct _CanvasStateImpl
{
    /// Storage for various internal node/slot attributes.
    ImGuiStorage cached_data{};
    /// Current node data.
    struct
    {
        /// User-provided unique node id.
        void* id = nullptr;
        /// User-provided node position.
        ImVec2* pos = nullptr;
        /// User-provided node selection status.
        bool* selected = nullptr;
    } node;
    /// Current slot data.
    struct
    {
        int kind = 0;
        const char* title = nullptr;
    } slot{};
    /// Node id which will be positioned at the mouse cursor on next frame.
    void* auto_position_node_id = nullptr;
    /// Connection that was just created.
    struct
    {
        /// Node id of input node.
        void* input_node = nullptr;
        /// Slot title of input node.
        const char* input_slot = nullptr;
        /// Node id of output node.
        void* output_node = nullptr;
        /// Slot title of output node.
        const char* output_slot = nullptr;
    } new_connection{};
    /// Starting position of node selection rect.
    ImVec2 selection_start{};
    /// Node id of node that is being dragged.
    void* drag_node = nullptr;
    /// Flag indicating that all selected nodes should be dragged.
    bool drag_node_selected = false;
    /// Node id of node that should be selected on next frame, while deselecting any other nodes.
    void* single_selected_node = nullptr;
    /// Frame on which selection logic should run.
    int do_selections_frame = 0;
    /// Current interaction state.
    _ImNodesState state{};
    /// Flag indicating that new connection was just made.
    bool just_connected = false;
    /// Previous canvas pointer. Used to restore proper gCanvas value when nesting canvases.
    CanvasState* prev_canvas = nullptr;
    /// A list of node/slot combos that can not connect to current pending connection.
    ImVector<_IgnoreSlot> ignore_connections{};
};

CanvasState::CanvasState() noexcept
{
    _impl = new _CanvasStateImpl();

    auto& imgui_style = ImGui::GetStyle();
    colors[ColCanvasLines] = imgui_style.Colors[ImGuiCol_Separator];
    colors[ColNodeBg] = imgui_style.Colors[ImGuiCol_WindowBg];
    colors[ColNodeActiveBg] = imgui_style.Colors[ImGuiCol_FrameBgActive];
    colors[ColNodeBorder] = imgui_style.Colors[ImGuiCol_Border];
    colors[ColConnection] = imgui_style.Colors[ImGuiCol_PlotLines];
    colors[ColConnectionActive] = imgui_style.Colors[ImGuiCol_PlotLinesHovered];
    colors[ColSelectBg] = imgui_style.Colors[ImGuiCol_FrameBgActive];
    colors[ColSelectBg].Value.w = 0.25f;
    colors[ColSelectBorder] = imgui_style.Colors[ImGuiCol_Border];
}

CanvasState::~CanvasState()
{
    delete _impl;
}

ImU32 MakeSlotDataID(const char* data, const char* slot_title, void* node_id, bool input_slot)
{
    ImU32 slot_id = ImHashStr(slot_title, 0, ImHashData(&node_id, sizeof(node_id)));
    if (input_slot)
    {
        // Ensure that input and output slots with same name have different ids.
        slot_id ^= ~0U;
    }
    return ImHashStr(data, 0, slot_id);
}

// Based on http://paulbourke.net/geometry/pointlineplane/
float GetDistanceToLineSquared(const ImVec2& point, const ImVec2& a, const ImVec2& b)
{
    float tx, ty, t, u;
    tx = b.x - a.x;
    ty = b.y - a.y;
    t = (tx * tx) + (ty * ty);
    u = ((point.x - a.x) * tx + (point.y - a.y) * ty) / t;
    if (u > 1)
        u = 1;
    else if (u < 0)
        u = 0;
    tx = (a.x + u * tx) - point.x;
    ty = (a.y + u * ty) - point.y;
    return tx * tx + ty * ty;
}

bool RenderConnection(const ImVec2& input_pos, const ImVec2& output_pos, float thickness)
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    CanvasState* canvas = gCanvas;
    const ImGuiStyle& style = ImGui::GetStyle();

    thickness *= canvas->zoom;

    ImVec2 p2 = input_pos - ImVec2{100 * canvas->zoom, 0};
    ImVec2 p3 = output_pos + ImVec2{100 * canvas->zoom, 0};

    // Assemble segments for path
    draw_list->PathLineTo(input_pos);
    draw_list->PathBezierCurveTo(p2, p3, output_pos, 0);

    // Check each segment and determine if mouse is hovering curve that is to be drawn
    float min_square_distance = FLT_MAX;
    for (int i = 0; i < draw_list->_Path.size() - 1; i++)
    {
        min_square_distance = ImMin(min_square_distance,
            GetDistanceToLineSquared(ImGui::GetMousePos(), draw_list->_Path[i], draw_list->_Path[i + 1]));
    }

    // Draw curve, change when it is hovered
    bool is_close = min_square_distance <= thickness * thickness;
    draw_list->PathStroke(is_close ? canvas->colors[ColConnectionActive] : canvas->colors[ColConnection], false, thickness);
    return is_close;
}

void BeginCanvas(CanvasState* canvas)
{
    canvas->_impl->prev_canvas = gCanvas;
    gCanvas = canvas;
    const ImGuiWindow* w = ImGui::GetCurrentWindow();
    ImGui::PushID(canvas);

    ImGui::ItemAdd(w->ContentsRegionRect, ImGui::GetID("canvas"));

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();

    if (!ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
    {
        if (ImGui::IsMouseDragging(1))
        {
            // handle edges of canvas and increase it
            if (w->Scroll.x == 0.f && io.MouseDelta.x > 0.f )
                canvas->rect.Min.x -= io.MouseDelta.x;
            if (w->Scroll.y == 0.f && io.MouseDelta.y > 0.f )
                canvas->rect.Min.y -= io.MouseDelta.y;
            if (w->Scroll.x == w->ScrollMax.x && io.MouseDelta.x < 0.f )
                canvas->rect.Max.x -= io.MouseDelta.x;
            if ( w->Scroll.y == w->ScrollMax.y && io.MouseDelta.y < 0.)
                canvas->rect.Max.y -= io.MouseDelta.y;
            // todo: decrease the canvas
            else
            {
                ImVec2 s = w->Scroll - io.MouseDelta;
                ImGui::SetScrollX(s.x);
                ImGui::SetScrollY(s.y);
            }
        }

        if (io.KeyShift && !io.KeyCtrl)
            ImGui::SetScrollX( ImGui::GetScrollX() + io.MouseWheel * -16.0f);

        if (!io.KeyShift && !io.KeyCtrl && io.MouseWheel != 0.f )
            ImGui::SetScrollY( ImGui::GetScrollY() + io.MouseWheel * -16.0f);

        if (!io.KeyShift && io.KeyCtrl)
        {
            if (io.MouseWheel != 0)
                canvas->zoom = ImClamp(canvas->zoom + io.MouseWheel * canvas->zoom / 16.f, 0.3f, 3.f);
        }
    }

    const float grid = 64.0f * canvas->zoom;

    ImVec2 pos = w->ClipRect.Min;
    ImVec2 size = w->ClipRect.GetSize();
    ImVec2 canvas_offset =  w->Scroll + canvas->rect.Min;

    ImU32 grid_color = ImColor(canvas->colors[ColCanvasLines]);
    for (float x = fmodf(-canvas_offset.x, grid); x < size.x;)
    {
        draw_list->AddLine(ImVec2(x, 0) + pos, ImVec2(x, size.y) + pos, grid_color);
        x += grid;
    }

    for (float y = fmodf(-canvas_offset.y, grid); y < size.y;)
    {
        draw_list->AddLine(ImVec2(0, y) + pos, ImVec2(size.x, y) + pos, grid_color);
        y += grid;
    }

    ImGui::SetWindowFontScale(canvas->zoom);
}

void EndCanvas()
{
    assert(gCanvas != nullptr);     // Did you forget calling BeginCanvas()?

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImGuiWindow* w = ImGui::GetCurrentWindow();
    auto* canvas = gCanvas;
    auto* impl = canvas->_impl;
    const ImGuiStyle& style = ImGui::GetStyle();

    // set the size of the canvas if it's smaller than the content region
    ImVec2 csize = canvas->rect.GetSize();
    ImVec2 wsize = w->InnerClipRect.GetSize();
    if ( csize.x < wsize.x )
        canvas->rect.Max.x = canvas->rect.Min.x + wsize.x;
    if ( csize.y < wsize.y )
        canvas->rect.Max.y = canvas->rect.Min.y + wsize.y;
    // set the cursor to the maximum canvas position so imgui knows where to put scrollbars
    ImGui::SetCursorPos(canvas->rect.GetSize());

    // Draw pending connection
    if (const ImGuiPayload* payload = ImGui::GetDragDropPayload())
    {
        char data_type_fragment[] = "new-node-connection-";
        if (strncmp(payload->DataType, data_type_fragment, sizeof(data_type_fragment) - 1) == 0)
        {
            auto* drag_data = (_DragConnectionPayload*)payload->Data;
            ImVec2 slot_pos{
                impl->cached_data.GetFloat(MakeSlotDataID("x", drag_data->slot_title, drag_data->node_id,
                    IsInputSlotKind(drag_data->slot_kind))),
                impl->cached_data.GetFloat(MakeSlotDataID("y", drag_data->slot_title, drag_data->node_id,
                    IsInputSlotKind(drag_data->slot_kind))),
            };

            float connection_indent = canvas->style.connection_indent * canvas->zoom;

            ImVec2 input_pos, output_pos;
            if (IsInputSlotKind(drag_data->slot_kind))
            {
                input_pos = slot_pos;
                input_pos.x += connection_indent;
                output_pos = ImGui::GetMousePos();
            }
            else
            {
                input_pos = ImGui::GetMousePos();
                output_pos = slot_pos;
                output_pos.x -= connection_indent;
            }

            RenderConnection(input_pos, output_pos, canvas->style.curve_thickness);
        }
    }

    if (impl->do_selections_frame <= ImGui::GetCurrentContext()->FrameCount)
        impl->single_selected_node = nullptr;

    switch (impl->state)
    {
    case State_None:
    {
        ImGuiID canvas_id = ImGui::GetID("canvas");
        if (ImGui::IsMouseDown(0) && ImGui::GetCurrentWindow()->ContentsRegionRect.Contains(ImGui::GetMousePos()))
        {
            if (ImGui::IsWindowHovered())
            {
                if (!ImGui::IsWindowFocused())
                    ImGui::FocusWindow(ImGui::GetCurrentWindow());

                if (!ImGui::IsAnyItemActive())
                {
                    ImGui::SetActiveID(canvas_id, ImGui::GetCurrentWindow());
                    const ImGuiIO& io = ImGui::GetIO();
                    if (!io.KeyCtrl && !io.KeyShift)
                    {
                        impl->single_selected_node = nullptr;   // unselect all
                        impl->do_selections_frame = ImGui::GetCurrentContext()->FrameCount + 1;
                    }
                }
            }

            if (ImGui::GetActiveID() == canvas_id && ImGui::IsMouseDragging(0))
            {
                impl->selection_start = ImGui::GetMousePos();
                impl->state = State_Select;
            }
        }
        else if (ImGui::GetActiveID() == canvas_id)
            ImGui::ClearActiveID();
        break;
    }
    case State_Drag:
    {
        if (!ImGui::IsMouseDown(0))
        {
            impl->state = State_None;
            impl->drag_node = nullptr;
        }
        break;
    }
    case State_Select:
    {
        if (ImGui::IsMouseDown(0))
        {
            draw_list->AddRectFilled(impl->selection_start, ImGui::GetMousePos(), canvas->colors[ColSelectBg]);
            draw_list->AddRect(impl->selection_start, ImGui::GetMousePos(), canvas->colors[ColSelectBorder]);
        }
        else
        {
            ImGui::ClearActiveID();
            impl->state = State_None;
        }
        break;
    }
    }

    ImGui::SetWindowFontScale(1.f);
    ImGui::PopID();     // canvas
    gCanvas = impl->prev_canvas;
}

bool BeginNode(void* node_id, ImVec2* pos, bool* selected)
{
    assert(gCanvas != nullptr);
    assert(node_id != nullptr);
    assert(pos != nullptr);
    assert(selected != nullptr);
    const ImGuiStyle& style = ImGui::GetStyle();
    const ImGuiWindow* w = ImGui::GetCurrentWindow();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    auto* canvas = gCanvas;
    auto* impl = canvas->_impl;

    impl->node.id = node_id;
    impl->node.pos = pos;
    impl->node.selected = selected;

    // 0 - node rect, curves
    // 1 - node content
    draw_list->ChannelsSplit(2);

    if (node_id == impl->auto_position_node_id)
    {
        // Somewhere out of view so that we dont see node flicker when it will be repositioned
        ImGui::SetCursorScreenPos(ImGui::GetWindowPos() + ImGui::GetWindowSize() + style.WindowPadding);
    }
    else
    {
        // Top-let corner of the node
        ImGui::SetCursorScreenPos(w->InnerClipRect.Min - w->Scroll + (*pos) * canvas->zoom - canvas->rect.Min);
    }

    ImGui::PushID(node_id);

    ImGui::BeginGroup();    // Slots and content group
    draw_list->ChannelsSetCurrent(1);

    return true;
}

void EndNode()
{
    assert(gCanvas != nullptr);
    const ImGuiStyle& style = ImGui::GetStyle();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    auto* canvas = gCanvas;
    auto* impl = canvas->_impl;
    auto* node_id = impl->node.id;

    bool& node_selected = *impl->node.selected;
    ImVec2& node_pos = *impl->node.pos;

    ImGui::EndGroup();    // Slots and content group

    ImRect node_rect{
        ImGui::GetItemRectMin() - style.ItemInnerSpacing * canvas->zoom,
        ImGui::GetItemRectMax() + style.ItemInnerSpacing * canvas->zoom
    };

    // Render frame
    draw_list->ChannelsSetCurrent(0);

    ImColor node_color = canvas->colors[node_selected ? ColNodeActiveBg : ColNodeBg];
    draw_list->AddRectFilled(node_rect.Min, node_rect.Max, node_color, style.FrameRounding);
    draw_list->AddRect(node_rect.Min, node_rect.Max, canvas->colors[ColNodeBorder], style.FrameRounding);

    // Create node item
    ImGuiID node_item_id = ImGui::GetID(node_id);
    ImGui::ItemSize(node_rect.GetSize());
    ImGui::ItemAdd(node_rect, node_item_id);

    // Node is active when being dragged
    if (ImGui::IsMouseDown(0) && !ImGui::IsAnyItemActive() && ImGui::IsItemHovered())
        ImGui::SetActiveID(node_item_id, ImGui::GetCurrentWindow());
    else if (!ImGui::IsMouseDown(0) && ImGui::IsItemActive())
        ImGui::ClearActiveID();

    // Save last selection state in case we are about to start dragging multiple selected nodes
    if (ImGui::IsMouseClicked(0))
    {
        ImGuiID prev_selected_id = ImHashStr("prev-selected", 0, ImHashData(&impl->node.id, sizeof(impl->node.id)));
        impl->cached_data.SetBool(prev_selected_id, node_selected);
    }

    ImGuiIO& io = ImGui::GetIO();
    switch (impl->state)
    {
    case State_None:
    {
        // Node selection behavior. Selection can change only when no node is being dragged and connections are not being made.
        if (impl->just_connected || ImGui::GetDragDropPayload() != nullptr)
        {
            // No selections are performed when nodes are being connected.
            impl->just_connected = false;
        }
        else if (impl->do_selections_frame == ImGui::GetCurrentContext()->FrameCount)
        {
            // Unselect other nodes when some node was left-clicked.
            node_selected = impl->single_selected_node == node_id;
        }
        else if (ImGui::IsMouseClicked(0) && ImGui::IsItemHovered() && ImGui::IsItemActive())
        {
            node_selected ^= true;
            if (!io.KeyCtrl && node_selected)
            {
                impl->single_selected_node = node_id;
                impl->do_selections_frame = ImGui::GetCurrentContext()->FrameCount + 1;
            }
        }
        else if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0))
        {
            impl->state = State_Drag;
            if (impl->drag_node == nullptr)
            {
                impl->drag_node = node_id;
                impl->drag_node_selected = node_selected;
            }
            else
                impl->single_selected_node = nullptr;
        }
        else if (node_id == impl->auto_position_node_id)
        {
            // Upon node creation we would like it to be positioned at the center of mouse cursor. This can be done only
            // once widget dimensions are known at the end of rendering and thus on the next frame.
            node_pos = ImGui::GetMousePos() - ImGui::GetCurrentWindow()->InnerClipRect.Min + ImGui::GetCurrentWindow()->Scroll + canvas->rect.Min - (node_rect.GetSize() / 2);
            impl->auto_position_node_id = nullptr;
        }
        break;
    }
    case State_Drag:
    {
        if (ImGui::IsMouseDown(0))
        {
            // Node dragging behavior. Drag node under mouse and other selected nodes if current node is selected.
            if ((ImGui::IsItemActive() || (impl->drag_node && impl->drag_node_selected && node_selected)))
                node_pos += ImGui::GetIO().MouseDelta / canvas->zoom;
        }
        break;
    }
    case State_Select:
    {
        ImRect selection_rect;
        selection_rect.Min.x = ImMin(impl->selection_start.x, ImGui::GetMousePos().x);
        selection_rect.Min.y = ImMin(impl->selection_start.y, ImGui::GetMousePos().y);
        selection_rect.Max.x = ImMax(impl->selection_start.x, ImGui::GetMousePos().x);
        selection_rect.Max.y = ImMax(impl->selection_start.y, ImGui::GetMousePos().y);

        ImGuiID prev_selected_id = ImHashStr("prev-selected", 0, ImHashData(&impl->node.id, sizeof(impl->node.id)));
        if (io.KeyShift)
        {
            // Append selection
            if (selection_rect.Contains(node_rect))
                node_selected = true;
            else
                node_selected = impl->cached_data.GetBool(prev_selected_id);
        }
        else if (io.KeyCtrl)
        {
            // Subtract from selection
            if (selection_rect.Contains(node_rect))
                node_selected = false;
            else
                node_selected = impl->cached_data.GetBool(prev_selected_id);
        }
        else
        {
            // Assign selection
            node_selected = selection_rect.Contains(node_rect);
        }
        break;
    }
    }

    draw_list->ChannelsMerge();

    ImGui::PopID();     // id
}

bool GetNewConnection(void** input_node, const char** input_slot_title, void** output_node, const char** output_slot_title)
{
    assert(gCanvas != nullptr);
    assert(input_node != nullptr);
    assert(input_slot_title != nullptr);
    assert(output_node != nullptr);
    assert(output_slot_title != nullptr);

    auto* canvas = gCanvas;
    auto* impl = canvas->_impl;

    if (impl->new_connection.output_node != nullptr)
    {
        *input_node = impl->new_connection.input_node;
        *input_slot_title = impl->new_connection.input_slot;
        *output_node = impl->new_connection.output_node;
        *output_slot_title = impl->new_connection.output_slot;
        memset(&impl->new_connection, 0, sizeof(impl->new_connection));
        return true;
    }

    return false;
}

bool GetPendingConnection(void** node_id, const char** slot_title, int* slot_kind)
{
    assert(gCanvas != nullptr);
    assert(node_id != nullptr);
    assert(slot_title != nullptr);
    assert(slot_kind != nullptr);

    if (auto* payload = ImGui::GetDragDropPayload())
    {
        char drag_id[] = "new-node-connection-";
        if (strncmp(drag_id, payload->DataType, sizeof(drag_id) - 1) == 0)
        {
            auto* drag_payload = (_DragConnectionPayload*)payload->Data;
            *node_id = drag_payload->node_id;
            *slot_title = drag_payload->slot_title;
            *slot_kind = drag_payload->slot_kind;
            return true;
        }
    }

    return false;
}

bool Connection(void* input_node, const char* input_slot, void* output_node, const char* output_slot)
{
    assert(gCanvas != nullptr);
    assert(input_node != nullptr);
    assert(input_slot != nullptr);
    assert(output_node != nullptr);
    assert(output_slot != nullptr);

    bool is_connected = true;
    auto* canvas = gCanvas;
    auto* impl = canvas->_impl;

    if (input_node == impl->auto_position_node_id || output_node == impl->auto_position_node_id)
        // Do not render connection to newly added output node because node is rendered outside of screen on the first frame and will be repositioned.
        return is_connected;

    ImVec2 input_slot_pos{
        impl->cached_data.GetFloat(MakeSlotDataID("x", input_slot, input_node, true)),
        impl->cached_data.GetFloat(MakeSlotDataID("y", input_slot, input_node, true)),
    };

    ImVec2 output_slot_pos{
        impl->cached_data.GetFloat(MakeSlotDataID("x", output_slot, output_node, false)),
        impl->cached_data.GetFloat(MakeSlotDataID("y", output_slot, output_node, false)),
    };

    // Indent connection a bit into slot widget.
    float connection_indent = canvas->style.connection_indent * canvas->zoom;
    input_slot_pos.x += connection_indent;
    output_slot_pos.x -= connection_indent;

    bool curve_hovered = RenderConnection(input_slot_pos, output_slot_pos, canvas->style.curve_thickness);
    if (curve_hovered && ImGui::IsWindowHovered())
    {
        if (ImGui::IsMouseDoubleClicked(0))
            is_connected = false;
    }

    impl->cached_data.SetFloat(MakeSlotDataID("hovered", input_slot, input_node, true), curve_hovered && is_connected);
    impl->cached_data.SetFloat(MakeSlotDataID("hovered", output_slot, output_node, false), curve_hovered && is_connected);

    void* pending_node_id;
    const char* pending_slot_title;
    int pending_slot_kind;
    if (GetPendingConnection(&pending_node_id, &pending_slot_title, &pending_slot_kind))
    {
        _IgnoreSlot ignore_connection{};
        if (IsInputSlotKind(pending_slot_kind))
        {
            if (pending_node_id == input_node && strcmp(pending_slot_title, input_slot) == 0)
            {
                ignore_connection.node_id = output_node;
                ignore_connection.slot_name = output_slot;
                ignore_connection.slot_kind = OutputSlotKind(1);
            }
        }
        else
        {
            if (pending_node_id == output_node && strcmp(pending_slot_title, output_slot) == 0)
            {
                ignore_connection.node_id = input_node;
                ignore_connection.slot_name = input_slot;
                ignore_connection.slot_kind = InputSlotKind(1);
            }
        }
        if (ignore_connection.node_id)
        {
            if (!impl->ignore_connections.contains(ignore_connection))
                impl->ignore_connections.push_back(ignore_connection);
        }
    }

    return is_connected;
}

CanvasState* GetCurrentCanvas()
{
    return gCanvas;
}

bool BeginSlot(const char* title, int kind)
{
    auto* canvas = gCanvas;
    auto* impl = canvas->_impl;

    impl->slot.title = title;
    impl->slot.kind = kind;

    ImGui::BeginGroup();
    return true;
}

void EndSlot()
{
    const ImGuiStyle& style = ImGui::GetStyle();
    auto* canvas = gCanvas;
    auto* impl = canvas->_impl;

    ImGui::EndGroup();

    ImGui::PushID(impl->slot.title);
    ImGui::PushID(impl->slot.kind);

    ImRect slot_rect{ImGui::GetItemRectMin(), ImGui::GetItemRectMax()};
    // This here adds extra line between slots because after user renders slot cursor is already past those items.
    // ImGui::ItemSize(slot_rect.GetSize());
    ImGui::ItemAdd(slot_rect, ImGui::GetID(impl->slot.title));

    if (ImGui::IsMouseClicked(0) && ImGui::IsItemHovered())
        ImGui::SetActiveID(ImGui::GetID(impl->slot.title), ImGui::GetCurrentWindow());

    if (ImGui::IsItemActive() && !ImGui::IsMouseDown(0))
        ImGui::ClearActiveID();

    // Store slot edge positions, curves will connect there
    {
        float x;
        if (IsInputSlotKind(impl->slot.kind))
            x = slot_rect.Min.x;
        else
            x = slot_rect.Max.x;

        impl->cached_data.SetFloat(MakeSlotDataID("x", impl->slot.title, impl->node.id, IsInputSlotKind(impl->slot.kind)), x);
        impl->cached_data.SetFloat(MakeSlotDataID("y", impl->slot.title, impl->node.id, IsInputSlotKind(impl->slot.kind)),
            slot_rect.Max.y - slot_rect.GetHeight() / 2);
    }

    if (ImGui::BeginDragDropSource())
    {
        auto* payload = ImGui::GetDragDropPayload();
        char drag_id[32];
        snprintf(drag_id, sizeof(drag_id), "new-node-connection-%08X", impl->slot.kind);
        if (payload == nullptr || !payload->IsDataType(drag_id))
        {
            _DragConnectionPayload drag_data{ };
            drag_data.node_id = impl->node.id;
            drag_data.slot_kind = impl->slot.kind;
            drag_data.slot_title = impl->slot.title;

            ImGui::SetDragDropPayload(drag_id, &drag_data, sizeof(drag_data));

            // Clear new connection info
            impl->new_connection.input_node = nullptr;
            impl->new_connection.input_slot = nullptr;
            impl->new_connection.output_node = nullptr;
            impl->new_connection.output_slot = nullptr;
            canvas->_impl->ignore_connections.clear();
        }
        ImGui::TextUnformatted(impl->slot.title);
        ImGui::EndDragDropSource();
    }

    if (IsConnectingCompatibleSlot() && ImGui::BeginDragDropTarget())
    {
        // Accept drags from opposite type (input <-> output, and same kind)
        char drag_id[32];
        snprintf(drag_id, sizeof(drag_id), "new-node-connection-%08X", impl->slot.kind * -1);

        if (auto* payload = ImGui::AcceptDragDropPayload(drag_id))
        {
            auto* drag_data = (_DragConnectionPayload*) payload->Data;

            // Store info of source slot to be queried by ImNodes::GetConnection()
            if (!IsInputSlotKind(impl->slot.kind))
            {
                impl->new_connection.input_node = drag_data->node_id;
                impl->new_connection.input_slot = drag_data->slot_title;
                impl->new_connection.output_node = impl->node.id;
                impl->new_connection.output_slot = impl->slot.title;
            }
            else
            {
                impl->new_connection.input_node = impl->node.id;
                impl->new_connection.input_slot = impl->slot.title;
                impl->new_connection.output_node = drag_data->node_id;
                impl->new_connection.output_slot = drag_data->slot_title;
            }
            impl->just_connected = true;
            canvas->_impl->ignore_connections.clear();
        }

        ImGui::EndDragDropTarget();
    }

    ImGui::PopID(); // kind
    ImGui::PopID(); // name
}

void AutoPositionNode(void* node_id)
{
    assert(gCanvas != nullptr);
    gCanvas->_impl->auto_position_node_id = node_id;
}

bool IsSlotCurveHovered()
{
    assert(gCanvas != nullptr);
    auto* canvas = gCanvas;
    auto* impl = canvas->_impl;

    void* node_id;
    const char* slot_title;
    int slot_kind;
    if (ImNodes::GetPendingConnection(&node_id, &slot_title, &slot_kind))
    {
        // In-progress connection to current slot is hovered
        return node_id == impl->node.id && strcmp(slot_title, impl->slot.title) == 0 &&
               slot_kind == impl->slot.kind;
    }

    // Actual curve is hovered
    return impl->cached_data.GetBool(MakeSlotDataID("hovered", impl->slot.title, impl->node.id,
           IsInputSlotKind(impl->slot.kind)));
}

bool IsConnectingCompatibleSlot()
{
    assert(gCanvas != nullptr);
    auto* canvas = gCanvas;
    auto* impl = canvas->_impl;

    if (auto* payload = ImGui::GetDragDropPayload())
    {
        auto* drag_payload = (_DragConnectionPayload*)payload->Data;

        if (drag_payload->node_id == impl->node.id)
            // Node can not connect to itself
            return false;

        char drag_id[32];
        snprintf(drag_id, sizeof(drag_id), "new-node-connection-%08X", impl->slot.kind * -1);
        if (strcmp(drag_id, payload->DataType) != 0)
            return false;

        for (int i = 0; i < impl->ignore_connections.size(); i++)
        {
            const _IgnoreSlot& ignored = impl->ignore_connections[i];
            if (ignored.node_id == impl->node.id && strcmp(ignored.slot_name, ignored.slot_name) == 0 &&
                IsInputSlotKind(ignored.slot_kind) == IsInputSlotKind(impl->slot.kind))
                return false;
        }

        return true;
    }

    return false;
}

}
