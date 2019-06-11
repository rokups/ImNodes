//
// Copyright (c) 2017-2019 Rokas Kupstys.
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
#include <vector>
#include <map>
#include <string>
#include <imgui.h>
#include "ImNodes.h"

ImNodes::CanvasState* gCanvas = nullptr;

struct Connection
{
    /// `id` that was passed to BeginNode() of input node.
    void* input_node = nullptr;
    /// Descriptor of input slot.
    const char* input_slot = nullptr;
    /// `id` that was passed to BeginNode() of output node.
    void* output_node = nullptr;
    /// Descriptor of output slot.
    const char* output_slot = nullptr;

    bool operator==(const Connection& other) const
    {
        return input_node == other.input_node &&
               input_slot == other.input_slot &&
               output_node == other.output_node &&
               output_slot == other.output_slot;
    }

    bool operator!=(const Connection& other) const
    {
        return !operator ==(other);
    }
};

enum NodeSlotTypes
{
    NodeSlotPosition = 1,   // ID can not be 0
    NodeSlotRotation,
    NodeSlotMatrix,
};

struct BaseNode
{
    /// Title which will be displayed at the center-top of the node.
    const char* title = nullptr;
    /// Flag indicating that node is selected by the user.
    bool selected = false;
    /// Node position on the canvas.
    ImVec2 pos{};
    /// List of node connections.
    std::vector<Connection> connections{};
    /// Last recorded width of the node. Used to center node title.
    float node_width = 0.f;
    /// Max width of output node title. Used to align output nodes to the right edge.
    float output_max_title_width = 0.f;

    explicit BaseNode(const char* title)
    {
        this->title = title;
    }
    virtual ~BaseNode() = default;

    BaseNode(const BaseNode& other) = delete;

    /// Renders a single node and it's connections.
    void RenderNode()
    {
        // Start rendering node
        if (ImNodes::BeginNode(this, &pos, &selected))
        {
            // Here goes node content

            if (node_width > 0)
            {
                // Center node title
                ImVec2 title_size = ImGui::CalcTextSize(title);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + node_width / 2.f - title_size.x / 2.f);
            }

            // Render node title
            ImGui::TextUnformatted(title);

            // Render node-specific slots
            RenderNodeSlots();

            // Store new connections when they are created
            Connection new_connection;
            if (ImNodes::GetNewConnection(&new_connection.input_node, &new_connection.input_slot,
                &new_connection.output_node, &new_connection.output_slot))
            {
                ((BaseNode*) new_connection.input_node)->connections.push_back(new_connection);
                ((BaseNode*) new_connection.output_node)->connections.push_back(new_connection);
            }

            // Render output connections of this node
            for (const Connection& connection : connections)
            {
                // Node contains all it's connections (both from output and to input slots). This means that multiple
                // nodes will have same connection. We render only output connections and ensure that each connection
                // will be rendered once.
                if (connection.output_node != this)
                    continue;

                if (!ImNodes::Connection(connection.input_node, connection.input_slot, connection.output_node,
                    connection.output_slot))
                {
                    // Remove deleted connections
                    ((BaseNode*) connection.input_node)->DeleteConnection(connection);
                    ((BaseNode*) connection.output_node)->DeleteConnection(connection);
                }
            }

            // Node rendering is done. This call will render node background based on size of content inside node.
            ImNodes::EndNode();

            // Store node width which is needed for centering title.
            node_width = ImGui::GetItemRectSize().x;
        }
    }

    /// Renders custom node content (slots, widgets)
    virtual void RenderNodeSlots() = 0;

    /// Renders a single slot, handles positioning, colors, icon, title.
    void RenderSlot(const char* slot_title, int kind)
    {
        const auto& style = ImGui::GetStyle();
        const float CIRCLE_RADIUS = 5.f * gCanvas->zoom;
        ImVec2 title_size = ImGui::CalcTextSize(slot_title);
        // Pull entire slot a little bit out of the edge so that curves connect into int without visible seams
        float item_offset_x = style.ItemSpacing.x * gCanvas->zoom;
        if (!ImNodes::IsOutputSlotKind(kind))
            item_offset_x = -item_offset_x;
        ImGui::SetCursorScreenPos(ImGui::GetCursorScreenPos() + ImVec2{item_offset_x, 0});

        if (ImNodes::BeginSlot(slot_title, kind))
        {
            auto* draw_lists = ImGui::GetWindowDrawList();

            // Slot appearance can be altered depending on curve hovering state.
            bool is_active = ImNodes::IsSlotCurveHovered() ||
                (ImNodes::IsConnectingCompatibleSlot() && !IsAlreadyConnectedWithPendingConnection(slot_title, kind));

            ImColor color = gCanvas->colors[is_active ? ImNodes::ColConnectionActive : ImNodes::ColConnection];

            ImGui::PushStyleColor(ImGuiCol_Text, color.Value);

            if (ImNodes::IsOutputSlotKind(kind))
            {
                // Align output slots to the right edge of the node.
                output_max_title_width = ImMax(output_max_title_width, title_size.x);
                float offset = (output_max_title_width + style.ItemSpacing.x) - title_size.x;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

                ImGui::TextUnformatted(slot_title);
                ImGui::SameLine();
            }

            ImRect circle_rect{
                ImGui::GetCursorScreenPos(),
                ImGui::GetCursorScreenPos() + ImVec2{CIRCLE_RADIUS * 2, CIRCLE_RADIUS * 2}
            };
            // Vertical-align circle in the middle of the line.
            float circle_offset_y = title_size.y / 2.f - CIRCLE_RADIUS;
            circle_rect.Min.y += circle_offset_y;
            circle_rect.Max.y += circle_offset_y;
            draw_lists->AddCircleFilled(circle_rect.GetCenter(), CIRCLE_RADIUS, color);

            ImGui::ItemSize(circle_rect.GetSize());
            ImGui::ItemAdd(circle_rect, ImGui::GetID(title));

            if (ImNodes::IsInputSlotKind(kind))
            {
                ImGui::SameLine();
                ImGui::TextUnformatted(slot_title);
            }

            ImGui::PopStyleColor();
            ImNodes::EndSlot();

            // A dirty trick to place output slot circle on the border.
            ImGui::GetCurrentWindow()->DC.CursorMaxPos.x -= item_offset_x;
        }
    }

    /// Extra convenience - do not highlight slot which is already connected to current source slot of pending new connection.
    bool IsAlreadyConnectedWithPendingConnection(const char* current_slot_title, int current_slot_kind)
    {
        void* other_node_id;
        const char* other_slot_title;
        int other_slot_kind;
        if (ImNodes::GetPendingConnection(&other_node_id, &other_slot_title, &other_slot_kind))
        {
            assert(ImNodes::IsInputSlotKind(current_slot_kind) != ImNodes::IsInputSlotKind(other_slot_kind));

            if (ImNodes::IsInputSlotKind(other_slot_kind))
            {
                for (const auto& conn : connections)
                {
                    if (conn.input_node == other_node_id && strcmp(conn.input_slot, other_slot_title) == 0 &&
                        conn.output_node == this && strcmp(conn.output_slot, current_slot_title) == 0)
                        return true;
                }
            }
            else
            {
                for (const auto& conn : connections)
                {
                    if (conn.input_node == this && strcmp(conn.input_slot, current_slot_title) == 0 &&
                        conn.output_node == other_node_id && strcmp(conn.output_slot, other_slot_title) == 0)
                        return true;
                }
            }
        }
        return false;
    }

    /// Deletes connection from this node.
    void DeleteConnection(const Connection& connection)
    {
        for (auto it = connections.begin(); it != connections.end(); ++it)
        {
            if (connection == *it)
            {
                connections.erase(it);
                break;
            }
        }
    }
};

struct MyNode0 : BaseNode
{
    explicit MyNode0() : BaseNode("Node 0") { }
    bool checked = false;

    void RenderNodeSlots() override
    {
        const auto& style = ImGui::GetStyle();

        // Render input slots
        ImGui::BeginGroup();
        {
            RenderSlot("Position", ImNodes::InputSlotKind(NodeSlotPosition));
            RenderSlot("Rotation", ImNodes::InputSlotKind(NodeSlotRotation));
        }
        ImGui::EndGroup();

        ImGui::SetCursorScreenPos({ImGui::GetItemRectMax().x + style.ItemSpacing.x, ImGui::GetItemRectMin().y});
        ImGui::BeginGroup();
        {
            // Custom widgets can be rendered in the middle of node
            ImGui::Checkbox("node content", &checked);
        }
        ImGui::EndGroup();

        // Render output slots in the next column
        ImGui::SetCursorScreenPos({ImGui::GetItemRectMax().x + style.ItemSpacing.x, ImGui::GetItemRectMin().y});
        ImGui::BeginGroup();
        {
            RenderSlot("Matrix", ImNodes::OutputSlotKind(NodeSlotMatrix));
            RenderSlot("Rotation", ImNodes::OutputSlotKind(NodeSlotRotation));
        }
        ImGui::EndGroup();
    }
};

struct MyNode1 : BaseNode
{
    explicit MyNode1() : BaseNode("Node 1") { }
    bool checked = false;

    void RenderNodeSlots() override
    {
        const auto& style = ImGui::GetStyle();

        // Render input slots
        ImGui::BeginGroup();
        {
            RenderSlot("Matrix", ImNodes::InputSlotKind(NodeSlotMatrix));
            RenderSlot("Rotation", ImNodes::InputSlotKind(NodeSlotRotation));
        }
        ImGui::EndGroup();

        ImGui::SetCursorScreenPos({ImGui::GetItemRectMax().x + style.ItemSpacing.x, ImGui::GetItemRectMin().y});
        ImGui::BeginGroup();
        {
            // Custom widgets can be rendered in the middle of node
            ImGui::Checkbox("node content", &checked);
        }
        ImGui::EndGroup();

        // Render output slots in the next column
        ImGui::SetCursorScreenPos({ImGui::GetItemRectMax().x + style.ItemSpacing.x, ImGui::GetItemRectMin().y});
        ImGui::BeginGroup();
        {
            RenderSlot("Position", ImNodes::OutputSlotKind(NodeSlotPosition));
            RenderSlot("Rotation", ImNodes::OutputSlotKind(NodeSlotRotation));
        }
        ImGui::EndGroup();
    }
};

std::map<std::string, BaseNode*(*)()> available_nodes{
    {"Node 0", []() -> BaseNode* { return new MyNode0(); }},
    {"Node 1", []() -> BaseNode* { return new MyNode1(); }}
};
std::vector<BaseNode*> nodes;

namespace ImGui
{

void ShowDemoWindow(bool*)
{
    if (gCanvas == nullptr)
    {
        // Canvas must be created after ImGui initializes, because constructor accesses ImGui style to configure default
        // colors.
        gCanvas = new ImNodes::CanvasState();
    }

    const ImGuiStyle& style = ImGui::GetStyle();
    if (ImGui::Begin("ImNodes", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        // We probably need to keep some state, like positions of nodes/slots for rendering connections.
        ImNodes::BeginCanvas(gCanvas);
        for (auto it = nodes.begin(); it != nodes.end();)
        {
            BaseNode* node = *it;
            (*it)->RenderNode();

            if (node->selected && ImGui::IsKeyPressedMap(ImGuiKey_Delete))
            {
                for (auto& connection : node->connections)
                {
                    ((BaseNode*) connection.input_node)->DeleteConnection(connection);
                    ((BaseNode*) connection.output_node)->DeleteConnection(connection);
                }
                delete node;
                it = nodes.erase(it);
            }
            else
                ++it;
        }

        const ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsMouseReleased(1) && ImGui::IsWindowHovered() && !ImGui::IsMouseDragging(1))
        {
            ImGui::FocusWindow(ImGui::GetCurrentWindow());
            ImGui::OpenPopup("NodesContextMenu");
        }

        if (ImGui::BeginPopup("NodesContextMenu"))
        {
            for (const auto& desc : available_nodes)
            {
                if (ImGui::MenuItem(desc.first.c_str()))
                {
                    nodes.push_back(desc.second());
                    ImNodes::AutoPositionNode(nodes.back());
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Zoom"))
                gCanvas->zoom = 1;

            if (ImGui::IsAnyMouseDown() && !ImGui::IsWindowHovered())
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImNodes::EndCanvas();
    }
    ImGui::End();

}

}   // namespace ImGui
