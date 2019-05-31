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

static const float SLOT_CIRCLE_RADIUS = 5.f;
static CanvasState* gCanvas = nullptr;

void PopID(int n)
{
    while (n-- > 0)
        ImGui::PopID();
}

template<typename T>
void PushIDs(T value)
{
    ImGui::PushID(value);
}

template<typename T, typename... Args>
void PushIDs(T value, Args... args)
{
    ImGui::PushID(value);
    PushIDs(args...);
}

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

struct _DragConnectionPayload
{
    NodeInfo* node = nullptr;
    SlotInfo* slot = nullptr;
    SlotType slot_type{};
};

struct _CanvasStateImpl
{
    ImVector<NodeInfo*> all_nodes{};
    NodeInfo* current_node = nullptr;
    SlotInfo* outputs = nullptr;
    int onum = 0;
    Connection* connections = nullptr;
    int cnum = 0;
    Connection* delete_connection = nullptr;
    Connection drop{};
    ImVec2 selection_start{};
    NodeInfo* drag_node = nullptr;
    NodeInfo* single_selected_node = nullptr;
    _ImNodesState state{};
    int last_connected_frame = 0;
};

CanvasState::CanvasState() noexcept
{
    _impl = new _CanvasStateImpl();

    const ImGuiStyle& style = ImGui::GetStyle();
    colors[CanvasLines] = style.Colors[ImGuiCol_Separator];
    colors[NodeBg] = style.Colors[ImGuiCol_WindowBg];
    colors[NodeActiveBg] = style.Colors[ImGuiCol_FrameBgActive];
    colors[NodeBorder] = style.Colors[ImGuiCol_Border];
    colors[Conn] = style.Colors[ImGuiCol_PlotLines];
    colors[ConnActive] = style.Colors[ImGuiCol_PlotLinesHovered];
    colors[ConnBorder] = style.Colors[ImGuiCol_Border];
    colors[SelectBg] = style.Colors[ImGuiCol_FrameBgActive];
    colors[SelectBg].Value.w = 0.25f;
    colors[SelectBorder] = style.Colors[ImGuiCol_Border];
}

CanvasState::~CanvasState()
{
    delete _impl;
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
    CanvasState* canvas = (CanvasState*) ImGui::GetStateStorage()->GetVoidPtr(ImGui::GetID("canvas-state"));
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
    draw_list->PathStroke(is_close ? canvas->colors[ConnActive] : canvas->colors[Conn], false, thickness);
    return is_close;
}

void SlotsInternal(SlotType type, SlotInfo* slots, int snum)
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImGuiStyle& style = ImGui::GetStyle();
    auto* canvas = (CanvasState*) ImGui::GetStateStorage()->GetVoidPtr(ImGui::GetID("canvas-state"));
    auto* impl = canvas->_impl;
    auto* storage = ImGui::GetStateStorage();

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
        SlotInfo* current_slot = &slots[i];
        ImGui::PushID(current_slot);
        ImVec2 text_size = ImGui::CalcTextSize(current_slot->title);

        if (type == OutputSlot)
        {
            float offset = (max_title_length + style.ItemSpacing.x) - text_size.x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
            ImGui::TextUnformatted(current_slot->title);
            ImGui::SameLine(0, 0);
        }

        const float circle_radius = SLOT_CIRCLE_RADIUS * canvas->zoom;
        ImRect circle_rect;
        float circle_center_y = ImGui::GetCursorScreenPos().y + text_size.y / 2;
        circle_rect.Min = {
            ImGui::GetCursorScreenPos().x + circle_radius * (type == InputSlot ? -1 : 1),
            circle_center_y - circle_radius
        };
        circle_rect.Max = circle_rect.Min + ImVec2{circle_radius * 2, circle_radius * 2};

        ImVec2 slot_center = circle_rect.GetCenter();
        storage->SetFloat(ImGui::GetID("x"), slot_center.x);
        storage->SetFloat(ImGui::GetID("y"), slot_center.y);

        ImGuiID slot_id = ImGui::GetID(current_slot->title);
        ImGui::ItemSize(circle_rect.GetSize());
        ImGui::ItemAdd(circle_rect, slot_id);

        if (ImGui::IsMouseClicked(0) && ImGui::IsItemHovered())
            ImGui::SetActiveID(slot_id, ImGui::GetCurrentWindow());

        if (ImGui::IsItemActive() && !ImGui::IsMouseDown(0))
            ImGui::ClearActiveID();

        ImU32 curve_hovered_id = ImGui::GetID("curve-hovered");
        bool is_connected = false;
        bool is_active = ImGui::IsItemActive();
        bool is_hovered = storage->GetBool(curve_hovered_id) || (!ImGui::IsMouseDown(0) && ImGui::IsItemHovered());

        for (int j = 0; !is_connected && j < impl->cnum; j++)
        {
            Connection* connection = &impl->connections[j];
            if (connection->input_node == impl->current_node)
                is_connected = current_slot == connection->input_slot;
            else if (connection->output_node == impl->current_node)
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
                drag_data.node = impl->current_node;
                drag_data.slot_type = type;
                drag_data.slot = current_slot;

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
                    impl->drop.input_node = drag_data->node;
                    impl->drop.input_slot = drag_data->slot;
                    impl->drop.output_node = impl->current_node;
                    impl->drop.output_slot = current_slot;
                }
                else
                {
                    impl->drop.input_node = impl->current_node;
                    impl->drop.input_slot = current_slot;
                    impl->drop.output_node = drag_data->node;
                    impl->drop.output_slot = drag_data->slot;
                }
                impl->last_connected_frame = ImGui::GetCurrentContext()->FrameCount;

                // Prevent double-connecting same connection
                for (int k = 0; k < impl->cnum; k++)
                {
                    Connection* connection = &impl->connections[k];
                    if (*connection == impl->drop)
                    {
                        impl->drop.input_node = nullptr;
                        impl->drop.input_slot = nullptr;
                        impl->drop.output_node = nullptr;
                        impl->drop.output_slot = nullptr;
                        break;
                    }
                }
            }

            ImGui::EndDragDropTarget();
        }

        if (is_active || is_hovered || is_connected)
        {
            storage->SetBool(curve_hovered_id, false); // Will be set on next frame if it is hovered
            draw_list->AddCircleFilled(circle_rect.GetCenter(), circle_radius,
                is_hovered ? canvas->colors[ConnActive] : canvas->colors[Conn]);
        }
        else
        {
            draw_list->AddCircleFilled(circle_rect.GetCenter(), circle_radius, canvas->colors[Conn]);
            draw_list->AddCircle(circle_rect.GetCenter(), circle_radius, canvas->colors[ConnBorder]);
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
    assert(gCanvas == nullptr);     // Canvas nesting is not supported.
    gCanvas = canvas;
    const ImGuiWindow* w = ImGui::GetCurrentWindow();
    ImGui::PushID(canvas);

    ImGui::ItemAdd(w->ContentsRegionRect, ImGui::GetID("canvas"));

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

    ImU32 grid_color = ImColor(canvas->colors[CanvasLines]);
    for (float x = fmodf(canvas->offset.x, grid); x < size.x;)
    {
        draw_list->AddLine(ImVec2(x, 0) + pos, ImVec2(x, size.y) + pos, grid_color);
        x += grid;
    }

    for (float y = fmodf(canvas->offset.y, grid); y < size.y;)
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
    auto* canvas = (CanvasState*)ImGui::GetStateStorage()->GetVoidPtr(ImGui::GetID("canvas-state"));
    auto* impl = canvas->_impl;
    auto* storage = ImGui::GetStateStorage();
    const ImGuiStyle& style = ImGui::GetStyle();

    // Draw pending connection
    if (const ImGuiPayload* payload = ImGui::GetDragDropPayload())
    {
        char data_type_fragment[] = "new-node-connection-";
        if (strncmp(payload->DataType, data_type_fragment, sizeof(data_type_fragment) - 1) == 0)
        {
            auto* drag_data = (_DragConnectionPayload*)payload->Data;

            PushIDs(drag_data->node, drag_data->slot_type, drag_data->slot);
            ImVec2 input_slot_pos{
                storage->GetFloat(ImGui::GetID("x")),
                storage->GetFloat(ImGui::GetID("y"))
            };
            PopID(3);

            ImVec2 input_pos, output_pos;
            if (drag_data->slot_type == InputSlot)
            {
                input_pos = input_slot_pos;
                output_pos = ImGui::GetMousePos();
            }
            else
            {
                input_pos = ImGui::GetMousePos();
                output_pos = input_slot_pos;
            }

            RenderConnection(input_pos, output_pos, SLOT_CIRCLE_RADIUS);
        }
    }

    switch (impl->state)
    {
    case State_None:
    {
        bool unselect_all = false;
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
                        unselect_all = true;
                }
            }
            
            if (ImGui::GetActiveID() == canvas_id && ImGui::IsMouseDragging(0))
            {
                impl->selection_start = ImGui::GetMousePos();
                impl->state = State_Select;

                auto* storage = ImGui::GetStateStorage();
                for (auto* node : impl->all_nodes)
                {
                    ImGui::PushID(node);
                    storage->SetBool(ImGui::GetID("prev-selected"), node->selected);
                    ImGui::PopID();     // node
                }
            }
        }
        else if (ImGui::GetActiveID() == canvas_id)
            ImGui::ClearActiveID();

        // Unselect other nodes when some node was left-clicked.
        if (impl->single_selected_node != nullptr || unselect_all)
        {
            for (auto* node : impl->all_nodes)
            {
                if (node != impl->single_selected_node)
                    node->selected = false;
            }
            impl->single_selected_node = nullptr;
        }
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
            draw_list->AddRectFilled(impl->selection_start, ImGui::GetMousePos(), canvas->colors[SelectBg]);
            draw_list->AddRect(impl->selection_start, ImGui::GetMousePos(), canvas->colors[SelectBorder]);
        }
        else
        {
            ImGui::ClearActiveID();
            impl->state = State_None;
        }
        break;
    }
    }

    impl->all_nodes.clear();
    ImGui::SetWindowFontScale(1.f);
    ImGui::PopID();     // canvas
    gCanvas = nullptr;
}

bool BeginNode(NodeInfo* node, SlotInfo* inputs, int inum, SlotInfo* outputs, int onum,
    Connection* connections, int cnum)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    ImGuiStorage* storage = ImGui::GetStateStorage();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    auto* canvas = (CanvasState*)storage->GetVoidPtr(ImGui::GetID("canvas-state"));
    auto* impl = canvas->_impl;
    impl->all_nodes.push_back(node);

    impl->current_node = node;
    impl->outputs = outputs;
    impl->onum = onum;
    impl->connections = connections;
    impl->cnum = cnum;

    // 0 - node rect, curves
    // 1 - node content
    draw_list->ChannelsSplit(2);

    if (node->pos == NODE_OFFSCREEN_POSITION)
    {
        // Somewhere out of view so that we dont see node flicker when it will be repositioned
        ImGui::SetCursorScreenPos(ImGui::GetWindowPos() + ImGui::GetWindowSize() + style.WindowPadding);
        // Do not render connections from this node because node is rendered outside of screen on the first frame and will be repositioned.
        cnum = 0;
    }
    else
    {
        // Top-let corner of the node
        ImGui::SetCursorScreenPos(ImGui::GetWindowPos() + node->pos * canvas->zoom + canvas->offset);
    }

    // Render connections
    draw_list->ChannelsSetCurrent(0);
    for (int i = 0; i < cnum; i++)
    {
        Connection* connection = &connections[i];
        // Render connections from current node inputs. This way each connection is rendered only once even though both
        // input and output nodes have same connection in their lists.
        if (connection->input_node == node)
        {
            if (connection->output_node->pos == NODE_OFFSCREEN_POSITION)
                // Do not render connection to newly added output node because node is rendered outside of screen on the first frame and will be repositioned.
                continue;

            // /!\ This code depends on ID stack structure!
            PushIDs(connection->input_node, InputSlot, connection->input_slot);
            ImU32 input_slot_curve_hovered_id = ImGui::GetID("curve-hovered");
            ImVec2 input_slot_pos{
                storage->GetFloat(ImGui::GetID("x")),
                storage->GetFloat(ImGui::GetID("y"))
            };
            PopID(3);

            PushIDs(connection->output_node, OutputSlot, connection->output_slot);
            ImU32 output_slot_curve_hovered_id = ImGui::GetID("curve-hovered");
            ImVec2 output_slot_pos{
                storage->GetFloat(ImGui::GetID("x")),
                storage->GetFloat(ImGui::GetID("y"))
            };
            PopID(3);

            bool curve_hovered = RenderConnection(input_slot_pos, output_slot_pos, SLOT_CIRCLE_RADIUS);
            if (curve_hovered && ImGui::IsWindowHovered())
            {
                if (ImGui::IsMouseDoubleClicked(0))
                    impl->delete_connection = connection;
            }

            storage->SetBool(input_slot_curve_hovered_id, curve_hovered);
            storage->SetBool(output_slot_curve_hovered_id, curve_hovered);
        }
    }

    ImGui::PushID(node);

    // Store canvas state in scope of the node. It is used by slot rendering.
    storage->SetVoidPtr(ImGui::GetID("canvas-state"), canvas);

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

    draw_list->ChannelsSetCurrent(1);

    return true;
}

void EndNode()
{
    const ImGuiStyle& style = ImGui::GetStyle();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    auto* canvas = (CanvasState*)ImGui::GetStateStorage()->GetVoidPtr(ImGui::GetID("canvas-state"));
    auto* impl = canvas->_impl;
    auto* node = impl->current_node;

    // Spacing after node content. Required for proper rendering when slot column heights are less than content column height.
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + style.ItemSpacing.y * canvas->zoom);

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
    float title_width = ImGui::CalcTextSize(node->title).x;
    ImGui::SetCursorScreenPos(node_rect.Min + ImVec2{node_rect.GetWidth() / 2 - title_width / 2, style.ItemSpacing.y});
    ImGui::TextUnformatted(node->title);

    // Render frame
    draw_list->ChannelsSetCurrent(0);

    ImColor node_color = canvas->colors[node->selected ? NodeActiveBg : NodeBg];
    draw_list->AddRectFilled(node_rect.Min, node_rect.Max, node_color, style.FrameRounding);
    draw_list->AddRect(node_rect.Min, node_rect.Max, canvas->colors[NodeBorder], style.FrameRounding);

    // Create node item
    ImGuiID node_id = ImGui::GetID("node");
    ImGui::ItemSize(node_rect.GetSize());
    ImGui::ItemAdd(node_rect, node_id);

    // Node is active when being dragged
    if (ImGui::IsMouseDown(0) && !ImGui::IsAnyItemActive() && ImGui::IsItemHovered())
        ImGui::SetActiveID(node_id, ImGui::GetCurrentWindow());
    else if (!ImGui::IsMouseDown(0) && ImGui::IsItemActive())
        ImGui::ClearActiveID();

    ImGuiIO& io = ImGui::GetIO();
    switch (impl->state)
    {
    case State_None:
    {
        // Node selection behavior. Selection can change only when no node is being dragged.
        if (impl->last_connected_frame == ImGui::GetCurrentContext()->FrameCount)
        {
            // No selections are performed when nodes are being connected.
        }
        else if (ImGui::IsMouseReleased(0) && ImGui::IsItemHovered() && !ImGui::IsAnyItemActive())
        {
            node->selected ^= true;
            if (!io.KeyCtrl)
                impl->single_selected_node = node;
        }
        else if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0))
        {
            impl->state = State_Drag;
            if (impl->drag_node == nullptr)
                impl->drag_node = node;
            else
                impl->single_selected_node = nullptr;
        }
        else if (node->pos == NODE_OFFSCREEN_POSITION)
        {
            // Upon node creation we would like it to be positioned at the center of mouse cursor. This can be done only
            // once widget dimensions are known at the end of rendering and thus on the next frame.
            node->pos =
                ImGui::GetMousePos() - ImGui::GetCurrentWindow()->Pos - canvas->offset - (node_rect.GetSize() / 2);
        }

        break;
    }
    case State_Drag:
    {
        if (ImGui::IsMouseDown(0))
        {
            // Node dragging behavior. Drag node under mouse and other selected nodes if current node is selected.
            if ((ImGui::IsItemActive() || (impl->drag_node && impl->drag_node->selected && node->selected)))
                node->pos += ImGui::GetIO().MouseDelta / canvas->zoom;
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

        if (io.KeyShift)
        {
            // Append selection
            if (selection_rect.Contains(node_rect))
                node->selected = true;
            else
                node->selected = ImGui::GetStateStorage()->GetBool(ImGui::GetID("prev-selected"));
        }
        else if (io.KeyCtrl)
        {
            // Subtract from selection
            if (selection_rect.Contains(node_rect))
                node->selected = false;
            else
                node->selected = ImGui::GetStateStorage()->GetBool(ImGui::GetID("prev-selected"));
        }
        else
        {
            // Assign selection
            node->selected = selection_rect.Contains(node_rect);
        }
        break;
    }
    }

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

CanvasState* GetCurrentCanvas()
{
    return gCanvas;
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
