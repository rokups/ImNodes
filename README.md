ImNodes
=======

A standalone [Dear ImGui](https://github.com/ocornut/imgui) node graph implementation.

![image](https://user-images.githubusercontent.com/19151258/59259827-23a19400-8c43-11e9-9fdb-a3e4465a98e5.png)

Library provides core features needed to create a node graph, while leaving it to the user to define content of node.
Node layouting is left to the user, however comprehensible example is available which can be used as a base.

## A trivial example

```cpp
static ImNodes::CanvasState canvas;

if (ImGui::Begin("ImNodes", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
{
    // We probably need to keep some state, like positions of nodes/slots for rendering connections.
    ImNodes::BeginCanvas(&canvas);

    struct Node
    {
        ImVec2 pos{};
        bool selected{};
    };

    static Node nodes[3] = {
        {{100, 100}, false},
        {{300, 50}, false},
        {{300, 150}, false},
    };

    for (Node& node : nodes)
    {
        if (ImNodes::BeginNode(&node, &node.pos, &node.selected))
        {
            ImGui::TextUnformatted("Node Title");

            if (ImNodes::BeginInputSlot("-> In", 1))
            {
                ImGui::TextUnformatted("-> In");
                ImNodes::EndSlot();
            }
            ImGui::SameLine();
            ImNodes::BeginOutputSlot("Out ->", 1);
            {
                ImGui::TextUnformatted("Out ->");
                ImNodes::EndSlot();
            }
            ImNodes::EndNode();
        }
    }

    ImNodes::Connection(&nodes[1], "-> In", &nodes[0], "Out ->");
    ImNodes::Connection(&nodes[2], "-> In", &nodes[0], "Out ->");
    
    ImNodes::EndCanvas();
}
ImGui::End();
```
