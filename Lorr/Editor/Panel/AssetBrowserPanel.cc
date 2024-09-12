#include "Panels.hh"

#include "Engine/Memory/Stack.hh"

namespace lr {
AssetBrowserPanel::AssetBrowserPanel(std::string name_, bool open_)
    : PanelI(std::move(name_), open_) {
    this->refresh_file_tree();
}

void populate_directory(Directory &dir) {
    for (const auto &entry : fs::directory_iterator(dir.path)) {
        if (entry.is_directory()) {
            Directory subdir;
            subdir.path = entry.path();
            populate_directory(subdir);
            dir.subdirs.push_back(std::move(subdir));
        } else if (entry.is_regular_file()) {
            dir.files.push_back(entry.path());
        }
    }

    std::sort(dir.files.begin(), dir.files.end(), [](const fs::path &lhs, const fs::path &rhs) {  //
        return lhs.filename() < rhs.filename();
    });
}

void AssetBrowserPanel::refresh_file_tree(this AssetBrowserPanel &self) {
    self.asset_dir = { .path = fs::current_path() / "resources" };  // TODO: (project) replace later
    populate_directory(self.asset_dir);
}

void AssetBrowserPanel::draw_project_tree(this AssetBrowserPanel &self) {
    ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg;
    table_flags |= ImGuiTableFlags_ContextMenuInBody;
    table_flags |= ImGuiTableFlags_ScrollY;
    table_flags |= ImGuiTableFlags_NoPadInnerX;
    table_flags |= ImGuiTableFlags_NoPadOuterX;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 0, 0 });
    if (ImGui::BeginTable("project_tree", 1, table_flags)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        self.draw_file_tree(self.asset_dir);

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

void AssetBrowserPanel::draw_dir_contents(this AssetBrowserPanel &self) {
    if (!self.selected_dir) {
        return;
    }

    i32 table_flags = ImGuiTableFlags_ContextMenuInBody;
    table_flags |= ImGuiTableFlags_ScrollY;
    table_flags |= ImGuiTableFlags_PadOuterX;
    table_flags |= ImGuiTableFlags_SizingFixedFit;

    f32 padding = 3.0;
    ImVec2 button_size = { 100, 140 };
    auto avail_x = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ScrollbarSize;
    f32 tile_size = button_size.x + 2 * padding;
    i32 tile_count = static_cast<i32>(avail_x / tile_size);

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { padding, padding });
    if (ImGui::BeginTable("asset_browser", tile_count, table_flags)) {
        for (auto &v : self.selected_dir->files) {
            ImGui::TableNextColumn();

            auto file_name = v.filename().string();
            ImGui::Button(file_name.c_str(), button_size);
            if (ImGui::BeginDragDropSource()) {
                auto payload_data = v.string();
                ImGui::SetDragDropPayload("ASSET_PATH", payload_data.c_str(), payload_data.length());
                ImGui::EndDragDropSource();
            }
        }

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

void AssetBrowserPanel::draw_file_tree(this AssetBrowserPanel &self, Directory &root_dir) {
    ImGuiTreeNodeFlags tree_node_dir_flags = ImGuiTreeNodeFlags_None;
    tree_node_dir_flags |= ImGuiTreeNodeFlags_FramePadding;
    tree_node_dir_flags |= ImGuiTreeNodeFlags_SpanFullWidth;

    auto tree_node_file_flags = tree_node_dir_flags;
    tree_node_file_flags |= ImGuiTreeNodeFlags_Leaf;

    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    bool no_subdirs = root_dir.subdirs.empty();
    bool no_files = root_dir.files.empty();

    auto icon = Icon::fa::folder;
    if (no_subdirs && no_files) {
        icon = Icon::fa::folder_open;
    }

    auto cur_node_flags = no_subdirs ? tree_node_file_flags : tree_node_dir_flags;
    std::string file_name = root_dir.path.filename();
    if (ImGui::TreeNodeEx(root_dir.path.c_str(), cur_node_flags, "%s  %s", icon, file_name.c_str())) {
        if (ImGui::IsItemToggledOpen() || ImGui::IsItemClicked()) {
            self.selected_dir = &root_dir;
        }

        for (auto &dirs : root_dir.subdirs) {
            self.draw_file_tree(dirs);
        }

        ImGui::TreePop();
    }
}

void AssetBrowserPanel::update(this AssetBrowserPanel &self) {
    memory::ScopedStack stack;

    ImGui::Begin(self.name.data(), nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    auto avail_region = ImGui::GetContentRegionAvail();

    // HEADER
    if (ImGui::Button(Icon::fa::arrows_rotate)) {
        self.refresh_file_tree();
    }

    ImGui::SameLine();
    if (ImGui::Button(Icon::fa::house)) {
        self.selected_dir = &self.asset_dir;
    }

    if (self.selected_dir) {
        auto rel_path = fs::relative(self.selected_dir->path, self.asset_dir.path);

        for (const auto &v : rel_path) {
            auto dir_name = v.filename().string();

            ImGui::SameLine();
            ImGui::TextUnformatted("/");
            ImGui::SameLine();
            ImGui::Button(dir_name.c_str());
        }
    }

    // ACTUAL FILE TREE
    if (ImGui::BeginTable("asset_browser_context", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_ContextMenuInBody, avail_region)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        self.draw_project_tree();
        ImGui::TableNextColumn();
        self.draw_dir_contents();

        ImGui::EndTable();
    }
    ImGui::End();
}

}  // namespace lr
