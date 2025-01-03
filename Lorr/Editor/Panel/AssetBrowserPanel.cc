#include "Editor/Panel/AssetBrowserPanel.hh"

#include "Editor/EditorApp.hh"

namespace lr {
auto is_inside(const fs::path &root, const fs::path &path) -> bool {
    const auto &root_prev = std::prev(root.end());
    return std::mismatch(root.begin(), root_prev, path.begin()).first == root_prev;
}

auto populate_directory(AssetDirectory *dir, AssetBrowserPanel &abp) -> void {
    for (const auto &entry : fs::directory_iterator(dir->path)) {
        const auto &path = entry.path();
        if (entry.is_directory()) {
            abp.add_directory(path);
        } else if (entry.is_regular_file()) {
            dir->add_asset(path);
        }
    }
}

auto AssetDirectory::add_subdir(this AssetDirectory &self, const fs::path &path) -> void {
    self.subdirs.push_back(path);
}

auto AssetDirectory::add_asset(this AssetDirectory &self, const fs::path &path) -> bool {
    if (path.extension() != ".lrasset") {
        return false;
    }

    auto &app = EditorApp::get();
    auto *asset = app.asset_man.add_asset(path);
    if (!asset) {
        return false;
    }

    LOG_TRACE("New asset added from '{}'", asset->path);
    self.asset_uuids.push_back(asset->uuid);

    return true;
}

AssetBrowserPanel::AssetBrowserPanel(std::string name_, bool open_)
    : PanelI(std::move(name_), open_) {
    auto &app = EditorApp::get();
    this->home_dir = app.active_project->root_dir;
    this->current_dir = this->home_dir;
    this->add_directory(this->current_dir);
}

auto AssetBrowserPanel::add_directory(this AssetBrowserPanel &self, const fs::path &path) -> AssetDirectory * {
    auto [dir_it, inserted] = self.dirs.try_emplace(path);
    if (!inserted) {
        return nullptr;
    }

    // IMPORTANT: This is some deep recursion.
    // This entire function is so shit, please dont use `directory`
    // after `populate_directory`, or any other iterators
    // this is so lethal.
    //
    auto &directory = dir_it->second;
    directory.watch_descriptor = self.file_watcher.watch_dir(path);
    directory.path = path;
    populate_directory(&directory, self);

    if (path.has_parent_path()) {
        auto parent_dir = self.find_directory(path.parent_path());
        if (parent_dir) {
            parent_dir->add_subdir(path);
        }
    }

    return &directory;
}

auto AssetBrowserPanel::remove_directory(this AssetBrowserPanel &self, const fs::path &path) -> void {
    if (is_inside(path, self.current_dir)) {
        auto closest_dir = path.parent_path();
        while (is_inside(self.home_dir, path)) {
            if (self.find_directory(closest_dir)) {
                break;
            }

            closest_dir = closest_dir.parent_path();
        }

        self.current_dir = closest_dir;
    }

    LOG_TRACE("Removing {}", path);
    fs::remove_all(path);

    // Remove found directory from its parent
    const auto &parent_dir_path = path.parent_path();
    auto parent_dir_it = self.dirs.find(parent_dir_path);
    if (parent_dir_it != self.dirs.end()) {
        auto &parent_dir = parent_dir_it->second;
        std::erase_if(parent_dir.subdirs, [path](const auto &v) { return v == path; });
    }

    for (auto it = self.dirs.begin(); it != self.dirs.end();) {
        auto &dir = it->second;
        if (is_inside(path, dir.path)) {
            self.file_watcher.remove_dir(dir.watch_descriptor);
            it = self.dirs.erase(it);
            continue;
        }
        it++;
    }
}

auto AssetBrowserPanel::find_directory(this AssetBrowserPanel &self, const fs::path &path) -> AssetDirectory * {
    auto dir_it = self.dirs.find(path);
    if (dir_it != self.dirs.end()) {
        return &dir_it->second;
    }

    return nullptr;
}

void AssetBrowserPanel::draw_dir_contents(this AssetBrowserPanel &self) {
    auto &app = EditorApp::get();

    i32 table_flags = ImGuiTableFlags_ContextMenuInBody;
    table_flags |= ImGuiTableFlags_ScrollY;
    table_flags |= ImGuiTableFlags_PadOuterX;
    table_flags |= ImGuiTableFlags_SizingFixedFit;

    f32 padding = 3.0;
    ImVec2 button_size = { 100, 140 };
    auto avail_x = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ScrollbarSize;
    f32 tile_size = button_size.x + 2 * padding;
    i32 tile_count = static_cast<i32>(avail_x / tile_size);

    bool open_create_dir_popup = false;
    bool open_create_scene_popup = false;
    bool open_import_model_popup = false;

    auto *dir_texture = app.asset_man.get_texture(app.layout.editor_assets["dir"]);
    auto dir_image = app.imgui_renderer.add_image(dir_texture->image_view);
    auto *scene_texture = app.asset_man.get_texture(app.layout.editor_assets["scene"]);
    auto scene_image = app.imgui_renderer.add_image(scene_texture->image_view);

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { padding, padding });
    if (ImGui::BeginTable("asset_browser", tile_count, table_flags)) {
        auto *cur_dir = self.find_directory(self.current_dir);
        for (auto &subdir_path : cur_dir->subdirs) {
            ImGui::TableNextColumn();

            const auto &file_name = subdir_path.filename().string();
            if (ImGuiLR::image_button(file_name, dir_image, button_size)) {
                self.current_dir = subdir_path;
            }
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Delete")) {
                    self.remove_directory(subdir_path);
                }

                ImGui::EndPopup();
            }
        }

        for (const auto &uuid : cur_dir->asset_uuids) {
            ImGui::TableNextColumn();

            auto *asset = app.asset_man.get_asset(uuid);
            if (!asset) {
                continue;
            }

            const auto &file_name = asset->path.filename().string();
            ImGuiLR::image_button(file_name, scene_image, button_size);
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("ASSET_BY_UUID", &asset->uuid, sizeof(UUID));
                ImGui::EndDragDropSource();
            }
        }

        if (ImGui::BeginPopupContextWindow(
                "asset_browser_ctx",
                ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverExistingPopup)) {
            if (ImGui::BeginMenu("Create...")) {
                open_create_dir_popup = ImGui::MenuItem("Directory");
                ImGui::Separator();
                open_create_scene_popup = ImGui::MenuItem("Scene");

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Import...")) {
                open_import_model_popup = ImGui::MenuItem("Model", nullptr, false);

                ImGui::EndMenu();
            }

            ImGui::EndPopup();
        }

        if (open_create_dir_popup) {
            ImGui::OpenPopup("###create_dir_popup");
        }

        if (open_create_scene_popup) {
            ImGui::OpenPopup("###create_scene_popup");
        }

        if (ImGui::BeginPopupModal("Create Directory...###create_dir_popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            constexpr static auto default_dir_name = "New Directory";
            static std::string new_dir_name = default_dir_name;
            ImGui::InputText("", &new_dir_name);

            if (new_dir_name.empty()) {
                ImGui::BeginDisabled();
            }

            if (ImGui::Button("OK")) {
                fs::create_directory(self.current_dir / new_dir_name);
                ImGui::CloseCurrentPopup();
                new_dir_name = default_dir_name;
            }

            if (new_dir_name.empty()) {
                ImGui::EndDisabled();
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("Create Scene...###create_scene_popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            constexpr static auto default_scene_name = "New Scene";
            static std::string new_scene_name = default_scene_name;
            ImGui::InputText("", &new_scene_name);

            if (new_scene_name.empty()) {
                ImGui::BeginDisabled();
            }

            if (ImGui::Button("OK")) {
                auto &app = EditorApp::get();
                app.asset_man.create_scene(new_scene_name, self.current_dir / (new_scene_name + ".json"));
                ImGui::CloseCurrentPopup();
                new_scene_name = default_scene_name;
            }

            if (new_scene_name.empty()) {
                ImGui::EndDisabled();
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::EndTable();
    }

    ImGui::PopStyleVar();
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

        self.draw_file_tree(self.home_dir);

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

void AssetBrowserPanel::draw_file_tree(this AssetBrowserPanel &self, const fs::path &path) {
    ImGuiTreeNodeFlags tree_node_dir_flags = ImGuiTreeNodeFlags_None;
    tree_node_dir_flags |= ImGuiTreeNodeFlags_FramePadding;
    tree_node_dir_flags |= ImGuiTreeNodeFlags_SpanFullWidth;

    auto tree_node_file_flags = tree_node_dir_flags;
    tree_node_file_flags |= ImGuiTreeNodeFlags_Leaf;

    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    auto *directory = self.find_directory(path);
    for (auto &subdir_path : directory->subdirs) {
        auto *subdir = self.find_directory(subdir_path);
        bool no_subdirs = subdir->subdirs.empty();
        bool no_files = subdir->asset_uuids.empty();
        auto cur_node_flags = no_subdirs ? tree_node_file_flags : tree_node_dir_flags;

        auto icon = Icon::fa::folder;
        if (no_subdirs && no_files) {
            icon = Icon::fa::folder_open;
        }

        const auto &file_name = subdir_path.filename().string();
        if (ImGui::TreeNodeEx(file_name.c_str(), cur_node_flags, "%s  %s", icon, file_name.c_str())) {
            if (ImGui::IsItemToggledOpen() || ImGui::IsItemClicked()) {
                self.current_dir = subdir_path;
            }

            self.draw_file_tree(subdir_path);

            ImGui::TreePop();
        }
    }
}

auto AssetBrowserPanel::poll_watch_events(this AssetBrowserPanel &self) -> void {
    while (true) {
        auto file_event = self.file_watcher.peek();
        if (!file_event.has_value()) {
            break;
        }

        const auto &event_path = self.file_watcher.get_path(file_event->watch_descriptor);
        const auto &full_path = event_path / file_event->file_name;
        if (file_event->action_mask & FileActionMask::Directory) {
            if (file_event->action_mask & FileActionMask::Create) {
                self.add_directory(full_path);
            }

            if (file_event->action_mask & FileActionMask::Delete) {
                self.remove_directory(full_path);
            }
        } else {
            if (file_event->action_mask & FileActionMask::Create) {
            }
        }
    }
}

void AssetBrowserPanel::update(this AssetBrowserPanel &self) {
    self.poll_watch_events();

    ImGui::Begin(self.name.data(), nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    auto avail_region = ImGui::GetContentRegionAvail();

    ImGui::SameLine();
    if (ImGui::Button(Icon::fa::house)) {
        self.current_dir = self.home_dir;
    }

    const auto &cur_rel_to_home = fs::relative(self.current_dir, self.home_dir);
    for (const auto &v : cur_rel_to_home) {
        if (v == ".") {
            continue;
        }

        auto dir_name = v.filename().string();

        ImGui::SameLine();
        ImGui::TextUnformatted("/");
        ImGui::SameLine();
        ImGui::TextUnformatted(dir_name.c_str());
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
