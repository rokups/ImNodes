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
#include <imgui.h>
#include <SDL_mouse.h>
#include "ImNodes.h"

namespace ImGui
{


struct MyNode
{
    ImNodes::NodeState state;
    std::vector<ImNodes::SlotDesc> inputs;
    std::vector<ImNodes::SlotDesc> outputs;
    std::vector<ImNodes::Connection> connections;
};

std::vector<MyNode> available_nodes{
    {
        // State
        {
            "Node 0",
            {20, 20}
        },
        // Inputs
        {
            // Input
            {
                "Input 0",
                0
            },
            {
                "Input longer",
                0
            }
        },
        // Outputs
        {
            // Output
            {
                "Output 0",
                0
            },
            {
                "Output longer",
                0
            }
        }
    },
    {
        // State
        {
            "Node 1",
            {0, 0}
        },
        // Inputs
        {
            // Input
            {
                "Input 0",
                0
            }
        },
        // Outputs
        {
            // Output
            {
                "Output longer",
                0
            }
        }
    }
};
std::vector<MyNode> nodes;
ImNodes::CanvasState canvas{};
bool init = false;

void ShowDemoWindow(bool*)
{
    if (ImGui::Begin("ImNodes"))
    {
        // We probably need to keep some state, like positions of nodes/slots for rendering connections.
        ImNodes::BeginCanvas(&canvas);
        for (auto& node : nodes)
        {
            if (ImNodes::BeginNode(&node, node.state,
                &node.inputs[0], node.inputs.size(),
                &node.outputs[0], node.outputs.size(),
                &node.connections[0], node.connections.size()))
            {
                // Custom widgets can be rendered in the middle of node
                ImGui::TextUnformatted("node content");

                // Store new connections
                ImNodes::Connection new_connection;
                if (ImNodes::GetNewConnection(&new_connection))
                {
                    ((MyNode*) new_connection.input_node)->connections.push_back(new_connection);
                    ((MyNode*) new_connection.output_node)->connections.push_back(new_connection);
                }

                // Remove deleted connections
                if (ImNodes::Connection* connection = ImNodes::GetDeleteConnection())
                {
                    auto delete_connection = [](ImNodes::Connection* connection, std::vector<ImNodes::Connection>& connections)
                    {
                        for (auto it = connections.begin(); it != connections.end(); ++it)
                        {
                            if (*connection == *it)
                            {
                                connections.erase(it);
                                break;
                            }
                        }
                    };
                    delete_connection(connection, ((MyNode*) connection->input_node)->connections);
                    delete_connection(connection, ((MyNode*) connection->output_node)->connections);
                }

                ImNodes::EndNode();
            }
        }

        const ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsMouseReleased(1) && io.MouseDragMaxDistanceSqr[1] < (io.MouseDragThreshold * io.MouseDragThreshold))
            ImGui::OpenPopup("NodesContextMenu");

        if (ImGui::BeginPopup("NodesContextMenu"))
        {
            for (const auto& node_template : available_nodes)
            {
                if (ImGui::MenuItem(node_template.state.title))
                {
                    nodes.emplace_back(node_template);
                    nodes.back().state.pos = ImGui::GetMousePos() - ImGui::GetCurrentWindow()->ParentWindow->Pos - canvas.offset;
                }
            }
            ImGui::EndPopup();
        }

        ImNodes::EndCanvas();
    }
    ImGui::End();

}

}
