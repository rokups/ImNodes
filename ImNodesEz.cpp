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
#include "ImNodesEz.h"

#include <imgui_internal.h>

namespace ImNodes
{

extern CanvasState* gCanvas;

namespace Ez
{

bool BeginNode(void* node_id, const char* title, ImVec2* pos, bool* selected)
{
    ImGuiStorage *storage = ImGui::GetStateStorage();
    ImGuiStyle &style = ImGui::GetStyle();

    bool result = ImNodes::BeginNode(node_id, pos, selected);

    ImVec2 title_size = ImGui::CalcTextSize(title);
    ImVec2 input_pos = ImGui::GetCursorScreenPos() + ImVec2{0, title_size.y + style.ItemSpacing.y * gCanvas->Zoom};

    // Get widths from previous frame rendering.
    float input_width = storage->GetFloat(ImGui::GetID("input-width"));
    float content_width = storage->GetFloat(ImGui::GetID("content-width"));
    float output_width = storage->GetFloat(ImGui::GetID("output-width"));
    float body_width = input_width + content_width + output_width;

    // Ignore this the first time the node is rendered since we don't know any widths yet.
    if (body_width > 0)
    {
        float output_max_title_width_next = storage->GetFloat(ImGui::GetID("output-max-title-width-next"));
        storage->SetFloat(ImGui::GetID("output-max-title-width"), output_max_title_width_next);
        storage->SetFloat(ImGui::GetID("output-max-title-width-next"), 0);

        body_width += 2*style.ItemSpacing.x * gCanvas->Zoom;
        float body_spacing = 0;

        if (body_width > title_size.x)
        {
            // Center node title
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + body_width*0.5f - title_size.x*0.5f);
        }
        else
        {
            // Calculate extra node body spacing, i.e. around content, needed due to wider title.
            body_spacing = ((title_size.x - body_width)*0.5f);
        }

        float content_x = input_pos.x + input_width + style.ItemSpacing.x * gCanvas->Zoom + body_spacing;
        float output_x = content_x + content_width + style.ItemSpacing.x * gCanvas->Zoom + body_spacing;
        storage->SetFloat(ImGui::GetID("content-x"), content_x);
        storage->SetFloat(ImGui::GetID("output-x"), output_x);
        storage->SetFloat(ImGui::GetID("body-y"), input_pos.y);
    }

    // Render node title
    ImGui::TextUnformatted(title);

    ImGui::SetCursorScreenPos(input_pos);

    ImGui::BeginGroup();
    return result;
}

void EndNode()
{
    ImGui::EndGroup();
    ImNodes::EndNode();
}

bool Slot(const char* title, int kind, ImVec2 &pos)
{
    auto* storage = ImGui::GetStateStorage();
    const auto& style = ImGui::GetStyle();
    const float CIRCLE_RADIUS = 5.f * gCanvas->Zoom;
    ImVec2 title_size = ImGui::CalcTextSize(title);
    // Pull entire slot a little bit out of the edge so that curves connect into it without visible seams
    float item_offset_x = style.ItemInnerSpacing.x + CIRCLE_RADIUS;
    if (!ImNodes::IsOutputSlotKind(kind))
        item_offset_x = -item_offset_x;
    ImGui::SetCursorScreenPos(pos + ImVec2{item_offset_x, 0 });

    pos.y += ImMax(title_size.y, 2*CIRCLE_RADIUS) + style.ItemSpacing.y;

    if (ImNodes::BeginSlot(title, kind))
    {
        auto* draw_lists = ImGui::GetWindowDrawList();

        // Slot appearance can be altered depending on curve hovering state.
        bool is_active = ImNodes::IsSlotCurveHovered() ||
                         (ImNodes::IsConnectingCompatibleSlot() /*&& !IsAlreadyConnectedWithPendingConnection(title, kind)*/);

        ImColor color = gCanvas->Colors[is_active ? ImNodes::ColConnectionActive : ImNodes::ColConnection];

        ImGui::PushStyleColor(ImGuiCol_Text, color.Value);

        // Compensate for large slot circles.
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImMax(CIRCLE_RADIUS - title_size.y*0.5f, 0.0f));

        if (ImNodes::IsOutputSlotKind(kind))
        {
            float *max_width_next = storage->GetFloatRef(ImGui::GetID("output-max-title-width-next"));
            *max_width_next = ImMax(*max_width_next, title_size.x);

            float offset = storage->GetFloat(ImGui::GetID("output-max-title-width"), title_size.x) - title_size.x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

            ImGui::TextUnformatted(title);
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
            ImGui::TextUnformatted(title);
        }

        ImGui::PopStyleColor();
        ImNodes::EndSlot();

        // A dirty trick to place output slot circle on the border.
        ImGui::GetCurrentWindow()->DC.CursorMaxPos.x -= item_offset_x;
        return true;
    }
    return false;
}

void InputSlots(const SlotInfo* slots, int snum)
{
    ImGuiStorage *storage = ImGui::GetStateStorage();
    const auto& style = ImGui::GetStyle();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, style.ItemSpacing * gCanvas->Zoom);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, style.ItemInnerSpacing * gCanvas->Zoom);

    // Get cursor screen position to be updated by slots as they are rendered.
    ImVec2 pos = ImGui::GetCursorScreenPos();

    // Render input slots
    ImGui::BeginGroup();
    {
        for (int i = 0; i < snum; i++)
            ImNodes::Ez::Slot(slots[i].title, ImNodes::InputSlotKind(slots[i].kind), pos);
    }
    ImGui::EndGroup();

    storage->SetFloat(ImGui::GetID("input-width"), ImGui::GetItemRectSize().x);

    // Move cursor to the next column
    ImGui::SetCursorScreenPos(ImVec2{storage->GetFloat(ImGui::GetID("content-x")), storage->GetFloat(ImGui::GetID("body-y"))});

    ImGui::PopStyleVar(2);

    // Begin region for node content
    ImGui::BeginGroup();
}

void OutputSlots(const SlotInfo* slots, int snum)
{
    ImGuiStorage *storage = ImGui::GetStateStorage();
    const auto& style = ImGui::GetStyle();

    // End region of node content
    ImGui::EndGroup();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, style.ItemSpacing * gCanvas->Zoom);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, style.ItemInnerSpacing * gCanvas->Zoom);

    storage->SetFloat(ImGui::GetID("content-width"), ImGui::GetItemRectSize().x);

    // Get cursor screen position to be updated by slots as they are rendered.
    ImVec2 pos = ImVec2{storage->GetFloat(ImGui::GetID("output-x")), storage->GetFloat(ImGui::GetID("body-y"))};

    // Set cursor screen position as it is recorded as the starting point in BeginGroup() for the item rect size.
    ImGui::SetCursorScreenPos(pos);

    // Render output slots in the next column
    ImGui::BeginGroup();
    {
        for (int i = 0; i < snum; i++)
            ImNodes::Ez::Slot(slots[i].title, ImNodes::OutputSlotKind(slots[i].kind), pos);
    }
    ImGui::EndGroup();

    storage->SetFloat(ImGui::GetID("output-width"), ImGui::GetItemRectSize().x);

    ImGui::PopStyleVar(2);
}

}

}
