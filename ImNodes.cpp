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

struct _DragConnectionPayload
{
    void* node_id = nullptr;
    SlotDesc* slot_desc = nullptr;
    SlotType slot_type{};
};

struct _SlotState
{
    ImVec2 pos{};
    bool curve_hovered = false;
};

struct _CanvasStateImpl
{
    ImPool<_SlotState> slot_state{};
    void* current_node_id = nullptr;
    NodeState* current_node_state = nullptr;
    SlotDesc* outputs = nullptr;
    int onum = 0;
    Connection* connections = nullptr;
    int cnum = 0;
    Connection* delete_connection = nullptr;
    Connection drop{};
};

CanvasState::CanvasState() noexcept
{
    _impl = new _CanvasStateImpl();
}

CanvasState::~CanvasState()
{
    delete _impl;
}

ImU32 SlotHash(SlotType type, void* node_id, SlotDesc* slot)
{
    return ImHash(&slot, sizeof(slot), ImHash(&type, sizeof(type), ImHash(&node_id, sizeof(node_id))));
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
    CanvasState& canvas = *(CanvasState*) ImGui::GetStateStorage()->GetVoidPtr(ImGui::GetID("canvas-state"));
    const ImGuiStyle& style = ImGui::GetStyle();

    thickness *= canvas.zoom;

    ImVec2 p2 = input_pos - ImVec2{100 * canvas.zoom, 0};
    ImVec2 p3 = output_pos + ImVec2{100 * canvas.zoom, 0};

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
    draw_list->PathStroke(
        ImColor(is_close ? style.Colors[ImGuiCol_PlotLinesHovered] : style.Colors[ImGuiCol_PlotLines]), false, thickness);
    return is_close;
}

void SlotsInternal(SlotType type, SlotDesc* slots, int snum)
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImGuiStyle& style = ImGui::GetStyle();
    auto* canvas = (CanvasState*) ImGui::GetStateStorage()->GetVoidPtr(ImGui::GetID("canvas-state"));
    auto* impl = canvas->_impl;

    ImGui::PushID(type);
    ImGui::BeginGroup();

    // Max title length will be used for aligning output block connectors to the right border
    float max_title_length = 0;
    for (int i = 0; i < snum; i++)
    {
        if (type == OutputSlot)
            max_title_length = ImMax(max_title_length, ImGui::CalcTextSize(slots[i].title).x);
    }

    // Render slots and their names
    for (int i = 0; i < snum; i++)
    {
        SlotDesc* current_slot = &slots[i];
        auto* slot_state = impl->slot_state.GetOrAddByKey(SlotHash(type, impl->current_node_id, current_slot));
        ImGui::PushID(current_slot);
        if (type == OutputSlot)
        {
            float offset = (max_title_length + style.ItemSpacing.x) - ImGui::CalcTextSize(current_slot->title).x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
            ImGui::TextUnformatted(current_slot->title);
            ImGui::SameLine(0, 0);
        }

        const float circle_radius = slot_circle_radius * canvas->zoom;
        ImRect circle_rect;
        if (type == InputSlot)
        {
            circle_rect = {ImGui::GetCursorScreenPos() - ImVec2{circle_radius, 0},
                ImGui::GetCursorScreenPos() + ImVec2{circle_radius, circle_radius * 2}};
        }
        else
        {
            circle_rect = {ImGui::GetCursorScreenPos() + ImVec2{circle_radius, 0},
                ImGui::GetCursorScreenPos() + ImVec2{circle_radius * 3, circle_radius * 2}};
        }

        slot_state->pos = circle_rect.GetCenter();

        ImGuiID slot_id = ImGui::GetID(current_slot->title);
        ImGui::ItemSize(circle_rect.GetSize());
        ImGui::ItemAdd(circle_rect, slot_id);

        if (ImGui::IsItemHoveredRect())
        {
            if (ImGui::IsMouseClicked(0))
                ImGui::SetActiveID(slot_id, ImGui::GetCurrentWindow());
        }

        if (ImGui::IsItemActive() && !ImGui::IsMouseDown(0))
            ImGui::SetActiveID(0, ImGui::GetCurrentWindow());

        bool is_connected = false;
        bool is_active = ImGui::IsItemActive();
        bool is_hovered = slot_state->curve_hovered || (!ImGui::IsMouseDown(0) && ImGui::IsItemHoveredRect());

        for (int j = 0; !is_connected && j < impl->cnum; j++)
        {
            Connection* connection = &impl->connections[j];
            if (connection->input_node == impl->current_node_id)
                is_connected = current_slot == connection->input_slot;
            else if (connection->output_node == impl->current_node_id)
                is_connected = current_slot == connection->output_slot;
        }

        if (ImGui::BeginDragDropSource())
        {
            auto* payload = ImGui::GetDragDropPayload();
            char drag_id[32];
            snprintf(drag_id, sizeof(drag_id), "new-node-connection-%u-%d", type, current_slot->kind);
            if (payload == nullptr || !payload->IsDataType(drag_id))
            {
                _DragConnectionPayload drag_data{ };
                drag_data.node_id = impl->current_node_id;
                drag_data.slot_type = type;
                drag_data.slot_desc = current_slot;

                ImGui::SetDragDropPayload(drag_id, &drag_data, sizeof(drag_data));

                // Clear new connection info
                impl->drop.input_node = nullptr;
                impl->drop.input_slot = nullptr;
                impl->drop.output_node = nullptr;
                impl->drop.output_slot = nullptr;
            }
            ImGui::TextUnformatted(current_slot->title);
            ImGui::EndDragDropSource();
            is_hovered |= true;
        }

        if (ImGui::BeginDragDropTarget())
        {
            // Accept drags from opposite type (input <-> output, and same kind)
            char drag_id[32];
            snprintf(drag_id, sizeof(drag_id), "new-node-connection-%u-%d", type ^ 1U, current_slot->kind);

            if (auto* payload = ImGui::GetDragDropPayload())
                is_hovered |= payload->IsDataType(drag_id);

            if (auto* payload = ImGui::AcceptDragDropPayload(drag_id))
            {
                auto* drag_data = (_DragConnectionPayload*) payload->Data;

                // Store info of source slot to be queried by ImNodes::GetConnection()
                if (type == OutputSlot)
                {
                    impl->drop.input_node = drag_data->node_id;
                    impl->drop.input_slot = drag_data->slot_desc;
                    impl->drop.output_node = impl->current_node_id;
                    impl->drop.output_slot = current_slot;
                }
                else
                {
                    impl->drop.input_node = impl->current_node_id;
                    impl->drop.input_slot = current_slot;
                    impl->drop.output_node = drag_data->node_id;
                    impl->drop.output_slot = drag_data->slot_desc;
                }
            }

            ImGui::EndDragDropTarget();
        }

        if (is_active || is_hovered || is_connected)
        {
            slot_state->curve_hovered = false;    // Will be set on next frame if it is hovered
            draw_list->AddCircleFilled(circle_rect.GetCenter(), circle_radius,
                ImColor(is_hovered ? style.Colors[ImGuiCol_PlotLinesHovered] : style.Colors[ImGuiCol_PlotLines]));
        }
        else
        {
            draw_list->AddCircleFilled(circle_rect.GetCenter(), circle_radius, ImColor(style.Colors[ImGuiCol_PlotLines]));
            draw_list->AddCircle(circle_rect.GetCenter(), circle_radius, ImColor(style.Colors[ImGuiCol_Border]));
        }

        if (type == InputSlot)
        {
            ImGui::SameLine(0, 0);
            ImGui::TextUnformatted(current_slot->title);
        }

        ImGui::PopID();     // current_slot
    }

    ImGui::EndGroup();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + style.ItemSpacing.y * canvas->zoom);
    ImGui::PopID();     // type
}

void BeginCanvas(CanvasState* canvas)
{
    assert(canvas != nullptr);
    ImGui::PushID(&canvas);

    ImGui::GetStateStorage()->SetVoidPtr(ImGui::GetID("canvas-state"), canvas);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();

    if (!ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
    {
        if (ImGui::IsMouseDragging(1))
            canvas->offset += io.MouseDelta;

        if (io.KeyShift && !io.KeyCtrl)
            canvas->offset.x += io.MouseWheel * 16.0f;

        if (!io.KeyShift && !io.KeyCtrl)
            canvas->offset.y += io.MouseWheel * 16.0f;

        if (!io.KeyShift && io.KeyCtrl)
        {
            if (io.MouseWheel != 0)
                canvas->zoom = ImClamp(canvas->zoom + io.MouseWheel * canvas->zoom / 16.f, 0.3f, 3.f);
            canvas->offset += ImGui::GetMouseDragDelta();
        }
    }

    const float grid = 64.0f * canvas->zoom;

    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 size = ImGui::GetWindowSize();

    for (float x = fmodf(canvas->offset.x, grid); x < size.x;)
    {
        draw_list->AddLine(ImVec2(x, 0) + pos, ImVec2(x, size.y) + pos, ImColor(0.5f, 0.5f, 0.5f, 0.1f));
        x += grid;
    }

    for (float y = fmodf(canvas->offset.y, grid); y < size.y;)
    {
        draw_list->AddLine(ImVec2(0, y) + pos, ImVec2(size.x, y) + pos, ImColor(0.5f, 0.5f, 0.5f, 0.1f));
        y += grid;
    }

    ImGui::SetWindowFontScale(canvas->zoom);
}

void EndCanvas()
{
    // Draw pending connection
    if (const ImGuiPayload* payload = ImGui::GetDragDropPayload())
    {
        char data_type_fragment[] = "new-node-connection-";
        if (strncmp(payload->DataType, data_type_fragment, sizeof(data_type_fragment) - 1) == 0)
        {
            auto* canvas = (CanvasState*)ImGui::GetStateStorage()->GetVoidPtr(ImGui::GetID("canvas-state"));
            auto* impl = canvas->_impl;
            auto* drag_data = (_DragConnectionPayload*)payload->Data;
            auto* slot_state = impl->slot_state.GetOrAddByKey(SlotHash(drag_data->slot_type, drag_data->node_id, drag_data->slot_desc));

            ImVec2 input_pos, output_pos;
            if (drag_data->slot_type == InputSlot)
            {
                input_pos = slot_state->pos;
                output_pos = ImGui::GetMousePos();
            }
            else
            {
                input_pos = ImGui::GetMousePos();
                output_pos = slot_state->pos;
            }

            RenderConnection(input_pos, output_pos, slot_circle_radius);
        }
    }

    ImGui::SetWindowFontScale(1.f);
    ImGui::PopID();     // canvas
}

bool BeginNode(void* id, NodeState& node, SlotDesc* inputs, int inum, SlotDesc* outputs, int onum,
    Connection* connections, int cnum)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    ImGuiStorage* storage = ImGui::GetStateStorage();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    auto* canvas = (CanvasState*)storage->GetVoidPtr(ImGui::GetID("canvas-state"));
    auto* impl = canvas->_impl;

    ImGui::PushID(id);

    // 0 - node rect, curves
    // 1 - node content
    draw_list->ChannelsSplit(2);

    // Store canvas state in scope of the node. It is used by slot rendering.
    storage->SetVoidPtr(ImGui::GetID("canvas-state"), canvas);

    impl->current_node_id = id;
    impl->current_node_state = &node;
    impl->outputs = outputs;
    impl->onum = onum;
    impl->connections = connections;
    impl->cnum = cnum;

    // Top-let corner of the node
    ImGui::SetCursorScreenPos(ImGui::GetWindowPos() + node.pos * canvas->zoom + canvas->offset);

    ImGui::BeginGroup();    // Slots and content group
    ImGui::NewLine();       // Leave a spare line for node title
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + style.ItemSpacing.y);

    // Render input slots
    draw_list->ChannelsSetCurrent(1);
    {
        SlotsInternal(InputSlot, inputs, inum);
        ImRect rect{ImGui::GetItemRectMin(), ImGui::GetItemRectMax()};
        // Go to position for rendering custom node widgets
        ImGui::SetCursorScreenPos(rect.Min + ImVec2{rect.GetWidth() + style.ItemSpacing.x * 2 * canvas->zoom, 0});
        ImGui::BeginGroup();    // Content group
    }

    // Render connections
    draw_list->ChannelsSetCurrent(0);
    for (int i = 0; i < cnum; i++)
    {
        Connection* connection = &connections[i];
        // Render connections from current node inputs. This way each connection is rendered only once even though both
        // input and output nodes have same connection in their lists.
        if (connection->input_node == id)
        {
            auto* input_slot_state = impl->slot_state.GetOrAddByKey(SlotHash(InputSlot, connection->input_node, connection->input_slot));
            auto* output_slot_state = impl->slot_state.GetOrAddByKey(SlotHash(OutputSlot, connection->output_node, connection->output_slot));

            bool curve_hovered = RenderConnection(input_slot_state->pos, output_slot_state->pos,
                slot_circle_radius);
            if (curve_hovered)
            {
                if (ImGui::IsMouseDoubleClicked(0))
                    impl->delete_connection = connection;
            }

            impl->slot_state.GetOrAddByKey(
                SlotHash(InputSlot, connection->input_node, connection->input_slot))->curve_hovered = curve_hovered;

            impl->slot_state.GetOrAddByKey(
                SlotHash(OutputSlot, connection->output_node, connection->output_slot))->curve_hovered = curve_hovered;
        }
    }

    draw_list->ChannelsSetCurrent(1);

    return true;
}

void EndNode()
{
    const ImGuiStyle& style = ImGui::GetStyle();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    auto* canvas = (CanvasState*)ImGui::GetStateStorage()->GetVoidPtr(ImGui::GetID("canvas-state"));
    auto* impl = canvas->_impl;

    // Render output slots
    {
        ImGui::EndGroup();    // Content group
        ImRect rect{ImGui::GetItemRectMin(), ImGui::GetItemRectMax()};
        // Go to position for rendering output slots
        ImGui::SetCursorScreenPos(rect.Min + ImVec2{rect.GetWidth() + style.ItemSpacing.x * 2 * canvas->zoom, 0});
        SlotsInternal(OutputSlot, impl->outputs, impl->onum);
    }

    ImGui::EndGroup();    // Slots and content group

    ImRect node_rect{ImGui::GetItemRectMin(), ImGui::GetItemRectMax()};

    // Render title
    draw_list->ChannelsSetCurrent(1);
    float title_width = ImGui::CalcTextSize(impl->current_node_state->title).x;
    ImGui::SetCursorScreenPos(node_rect.Min + ImVec2{node_rect.GetWidth() / 2 - title_width / 2, style.ItemSpacing.y});
    ImGui::TextUnformatted(impl->current_node_state->title);

    // Render frame
    draw_list->ChannelsSetCurrent(0);
    draw_list->AddRectFilled(node_rect.Min, node_rect.Max, ImColor(style.Colors[ImGuiCol_FrameBg]), 3);
    draw_list->AddRect(node_rect.Min, node_rect.Max, ImColor(style.Colors[ImGuiCol_Border]), 3);

    // Create node item
    ImGuiID node_id = ImGui::GetID("node");
    ImGui::ItemSize(node_rect);
    ImGui::ItemAdd(node_rect, node_id);

    // Node is active when being dragged
    if (!ImGui::IsAnyItemActive() && ImGui::IsMouseHoveringRect(node_rect.Min, node_rect.Max) && ImGui::IsMouseDown(0))
        ImGui::SetActiveID(node_id, ImGui::GetCurrentWindow());
    else if (!ImGui::IsMouseDown(0) && ImGui::IsItemActive())
        ImGui::SetActiveID(0, ImGui::GetCurrentWindow());

    // Node dragging behavior
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0, 0.0f))
        impl->current_node_state->pos += ImGui::GetIO().MouseDelta / canvas->zoom;

    draw_list->ChannelsMerge();

    ImGui::PopID();     // id
}

bool GetNewConnection(Connection* connection)
{
    assert(connection != nullptr);

    auto* canvas = (CanvasState*)ImGui::GetStateStorage()->GetVoidPtr(ImGui::GetID("canvas-state"));
    auto* impl = canvas->_impl;

    if (impl->drop.output_node != nullptr)
    {
        *connection = impl->drop;
        memset(&impl->drop, 0, sizeof(impl->drop));
        return true;
    }

    return false;
}

Connection* GetDeleteConnection()
{
    auto* canvas = (CanvasState*)ImGui::GetStateStorage()->GetVoidPtr(ImGui::GetID("canvas-state"));
    if (auto* connection = canvas->_impl->delete_connection)
    {
        canvas->_impl->delete_connection = nullptr;
        return connection;
    }
    return nullptr;
}

bool Connection::operator==(const Connection& other) const
{
    return input_node == other.input_node &&
           input_slot == other.input_slot &&
           output_node == other.output_node &&
           output_slot == other.output_slot;
}

bool Connection::operator!=(const Connection& other) const
{
    return !operator ==(other);
}

}
