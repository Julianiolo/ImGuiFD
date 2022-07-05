# ImGuiFD
A Dear ImGui based File Dialog without any extra dependencies (not even STL!).

## Usage
ImGui like design:
```cpp

if(...) {
  ImGuiFD::OpenDirDialog("Choose Dir", ".");
}

std::string path;
if (ImGuiFD::BeginDialog("Choose Dir")) {
    if (ImGuiFD::ActionDone()) {
        if (ImGuiFD::SelectionMade()) {
            path = ImGuiFD::GetSelectionPathString(0);
        }
        ImGuiFD::CloseCurrentDialog();
    }

    ImGuiFD::EndDialog();
}
```

