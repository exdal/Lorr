#include "Panels.hh"

namespace lr {
AssetBrowserPanel::AssetBrowserPanel(std::string_view name_, bool open_)
    : PanelI(name_, open_) {
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

    ImGuiTreeNodeFlags tree_node_flags = ImGuiTreeNodeFlags_OpenOnArrow;
    tree_node_flags |= ImGuiTreeNodeFlags_FramePadding;
    tree_node_flags |= ImGuiTreeNodeFlags_SpanFullWidth;
    tree_node_flags |= ImGuiTreeNodeFlags_DefaultOpen;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 0, 0 });
    if (ImGui::BeginTable("project_tree", 1, table_flags)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        if (ImGui::TreeNodeEx("Assets", tree_node_flags)) {
            self.draw_file_tree(self.asset_dir);
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
    tree_node_file_flags |= ImGuiTreeNodeFlags_NoTreePushOnOpen;

    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    if (ImGui::TreeNodeEx(root_dir.path.c_str(), tree_node_dir_flags, "\uf07b  %s", root_dir.path.filename().c_str())) {
        for (auto &dirs : root_dir.subdirs) {
            self.draw_file_tree(dirs);
        }

        for (auto &f : root_dir.files) {
            ImGui::TreeNodeEx(f.c_str(), tree_node_file_flags, "\uf15b  %s", f.filename().c_str());
        }
        ImGui::TreePop();
    }
}

void AssetBrowserPanel::update(this AssetBrowserPanel &self) {
    ImGui::Begin(self.name.data());
    auto avail_region = ImGui::GetContentRegionAvail();
    // HEADER
    if (ImGui::Button("\uf021")) {
        self.refresh_file_tree();
    }

    // ACTUAL FILE TREE
    if (ImGui::BeginTable("asset_browser_context", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_ContextMenuInBody, avail_region)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        self.draw_project_tree();
        ImGui::TableNextColumn();

        ImGui::EndTable();
    }
    ImGui::End();
}

}  // namespace lr
