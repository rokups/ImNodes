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

struct StyleVarMod
{
    ImNodesStyleVar Index;
    float Value[2];
    StyleVarMod(ImNodesStyleVar idx, float val)          { Index = idx; Value[0] = val; }
    StyleVarMod(ImNodesStyleVar idx, const ImVec2 &val)  { Index = idx; Value[0] = val.x; Value[1] = val.y; }
};

// TODO: move these to a stackable state variable.
static Style GStyle;
static ImVector<StyleVarMod> GStyleVarStack;
static ImDrawListSplitter GSplitter;
static float GBodyPosY;
static bool *GNodeSelected;

bool BeginNode(void* node_id, const char* title, ImVec2* pos, bool* selected)
{
    ImGuiStorage *storage = ImGui::GetStateStorage();

    GNodeSelected = selected;

    auto draw_list = ImGui::GetWindowDrawList();
    GSplitter.Clear();
    GSplitter.Split(draw_list, 2);
    GSplitter.SetCurrentChannel(draw_list, 1);

    bool result = ImNodes::BeginNode(node_id, pos, selected);

    ImVec2 title_size = ImGui::CalcTextSize(title);
    ImVec2 title_pos = ImGui::GetCursorScreenPos();
    GBodyPosY = title_pos.y + title_size.y + gCanvas->Style.NodeSpacing.y * gCanvas->Zoom;
    ImVec2 input_pos = ImVec2{title_pos.x, GBodyPosY + gCanvas->Style.NodeSpacing.y * gCanvas->Zoom};

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

        body_width += 2*GStyle.ItemSpacing.x * gCanvas->Zoom;
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

        float content_x = input_pos.x + input_width + GStyle.ItemSpacing.x * gCanvas->Zoom + body_spacing;
        float output_x = content_x + content_width + GStyle.ItemSpacing.x * gCanvas->Zoom + body_spacing;
        storage->SetFloat(ImGui::GetID("content-x"), content_x);
        storage->SetFloat(ImGui::GetID("output-x"), output_x);
        storage->SetFloat(ImGui::GetID("body-y"), input_pos.y);
    }

    // Render node title
    ImGui::TextUnformatted(title);

    ImGui::SetCursorScreenPos(input_pos);

    return result;
}

void EndNode()
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    auto* canvas = gCanvas;

    // Inhibit node rendering in ImNodes::EndNode() by setting colors with alpha as 0.
    ImColor activebg = gCanvas->Colors[ColNodeActiveBg];
    ImColor inactivebg = gCanvas->Colors[ColNodeBg];
    gCanvas->Colors[ColNodeActiveBg] = IM_COL32(0,0,0,0);
    gCanvas->Colors[ColNodeBg] = IM_COL32(0,0,0,0);

    ImNodes::EndNode();

    // Restore colors.
    gCanvas->Colors[ColNodeActiveBg] = activebg;
    gCanvas->Colors[ColNodeBg] = inactivebg;

    ImRect node_rect{
        ImGui::GetItemRectMin(),
        ImGui::GetItemRectMax()
    };

    ImVec2 titlebar_end = ImVec2{node_rect.Max.x, GBodyPosY};
    ImVec2 body_pos = ImVec2{node_rect.Min.x, GBodyPosY};

    GSplitter.SetCurrentChannel(draw_list, 0);

    // Render title bar background
    // TODO: add configurable title color separate from body color.
    ImColor node_color = canvas->Colors[*GNodeSelected ? ColNodeActiveBg : ColNodeBg];
    draw_list->AddRectFilled(node_rect.Min, titlebar_end, node_color, canvas->Style.NodeRounding, ImDrawCornerFlags_Top);

    // Render body background
    node_color = canvas->Colors[*GNodeSelected ? ColNodeActiveBg : ColNodeBg];
    draw_list->AddRectFilled(body_pos, node_rect.Max, node_color, canvas->Style.NodeRounding, ImDrawCornerFlags_Bot);

    // Render outlines
    draw_list->AddRect(node_rect.Min, node_rect.Max, canvas->Colors[ColNodeBorder], canvas->Style.NodeRounding);
    draw_list->AddLine(body_pos, titlebar_end, canvas->Colors[ColNodeBorder]);

    GSplitter.Merge(draw_list);
}

bool Slot(const char* title, int kind, ImVec2 &pos)
{
    auto* storage = ImGui::GetStateStorage();
    const float CIRCLE_RADIUS = GStyle.SlotRadius * gCanvas->Zoom;
    ImVec2 title_size = ImGui::CalcTextSize(title);
    // Pull entire slot a little bit out of the edge so that curves connect into it without visible seams
    float item_offset_x = gCanvas->Style.NodeSpacing.x + CIRCLE_RADIUS;
    if (!ImNodes::IsOutputSlotKind(kind))
        item_offset_x = -item_offset_x;
    ImGui::SetCursorScreenPos(pos + ImVec2{item_offset_x, 0 });

    pos.y += ImMax(title_size.y, 2*CIRCLE_RADIUS) + GStyle.ItemSpacing.y;

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
            ImGui::SameLine(0.0f, GStyle.ItemSpacing.x);
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
            ImGui::SameLine(0.0f, GStyle.ItemSpacing.x);
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

    PushStyleVar(ImNodesStyleVar_ItemSpacing, GStyle.ItemSpacing * gCanvas->Zoom);
    PushStyleVar(ImNodesStyleVar_NodeSpacing, gCanvas->Style.NodeSpacing * gCanvas->Zoom);

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

    PopStyleVar(2);

    // Begin region for node content
    ImGui::BeginGroup();
}

void OutputSlots(const SlotInfo* slots, int snum)
{
    ImGuiStorage *storage = ImGui::GetStateStorage();

    // End region of node content
    ImGui::EndGroup();

    PushStyleVar(ImNodesStyleVar_ItemSpacing, GStyle.ItemSpacing * gCanvas->Zoom);
    PushStyleVar(ImNodesStyleVar_NodeSpacing, gCanvas->Style.NodeSpacing * gCanvas->Zoom);

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

    PopStyleVar(2);
}

void PushStyleVar(ImNodesStyleVar idx, float val)
{
    IM_ASSERT(gCanvas != nullptr);
    float *var;
    switch (idx)
    {
    case ImNodesStyleVar_GridSpacing: var = &gCanvas->Style.GridSpacing; break;
    case ImNodesStyleVar_CurveThickness: var = &gCanvas->Style.CurveThickness; break;
    case ImNodesStyleVar_CurveStrength: var = &gCanvas->Style.CurveStrength; break;
    case ImNodesStyleVar_SlotRadius: var = &GStyle.SlotRadius; break;
    case ImNodesStyleVar_NodeRounding: var = &gCanvas->Style.NodeRounding; break;
    default: IM_ASSERT(0 && "Called PushStyleVar() float variant but variable is not a float!");
    }
    GStyleVarStack.push_back(StyleVarMod(idx, *var));
    *var = val;
}

void PushStyleVar(ImNodesStyleVar idx, const ImVec2& val)
{
    IM_ASSERT(gCanvas != nullptr);
    ImVec2 *var;
    switch (idx)
    {
    case ImNodesStyleVar_NodeSpacing: var = &gCanvas->Style.NodeSpacing; break;
    case ImNodesStyleVar_ItemSpacing: var = &GStyle.ItemSpacing; break;
    default: IM_ASSERT(0 && "Called PushStyleVar() ImVec2 variant but variable is not a ImVec2!");
    }
    GStyleVarStack.push_back(StyleVarMod(idx, *var));
    *var = val;
}

void PopStyleVar(int count)
{
    IM_ASSERT(gCanvas != nullptr);
    IM_ASSERT(GStyleVarStack.size() >= count);
    while (count > 0)
    {
        StyleVarMod& backup = GStyleVarStack.back();
        switch (backup.Index)
        {
        case ImNodesStyleVar_GridSpacing: gCanvas->Style.GridSpacing = backup.Value[0]; break;
        case ImNodesStyleVar_CurveThickness: gCanvas->Style.CurveThickness = backup.Value[0]; break;
        case ImNodesStyleVar_CurveStrength: gCanvas->Style.CurveStrength = backup.Value[0]; break;
        case ImNodesStyleVar_SlotRadius: GStyle.SlotRadius = backup.Value[0]; break;
        case ImNodesStyleVar_NodeRounding: gCanvas->Style.NodeRounding = backup.Value[0]; break;
        case ImNodesStyleVar_NodeSpacing: gCanvas->Style.NodeSpacing = ImVec2{backup.Value[0], backup.Value[1]}; break;
        case ImNodesStyleVar_ItemSpacing: GStyle.ItemSpacing = ImVec2{backup.Value[0], backup.Value[1]}; break;
        default: IM_ASSERT(0);
        }
        GStyleVarStack.pop_back();
        count--;
    }
}

}

}
