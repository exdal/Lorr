#include "Editor/Panel/AssetBrowserPanel.hh"

#include "Editor/EditorApp.hh"

namespace lr {
auto populate_directory(AssetDirectory *dir, FileWatcher &file_watcher) -> void {
    for (const auto &entry : fs::directory_iterator(dir->path)) {
        const auto &path = entry.path();
        if (entry.is_directory()) {
            auto subdir = AssetDirectory::create(path);
            subdir->watch(file_watcher);
            populate_directory(subdir.get(), file_watcher);

            dir->add_subdir(std::move(subdir));
        } else if (entry.is_regular_file()) {
            dir->add_asset(path);
        }
    }
}

auto AssetDirectory::create(const fs::path &path_) -> std::unique_ptr<AssetDirectory> {
    auto self = std::make_unique<AssetDirectory>();
    self->path = path_;

    return self;
}

auto AssetDirectory::watch(FileWatcher &watcher) -> void {
    this->watch_descriptor = watcher.watch_dir(this->path);
}

auto AssetDirectory::add_subdir(std::unique_ptr<AssetDirectory> &&dir_) -> void {
    dir_->parent = this;
    this->subdirs.push_back(std::move(dir_));
}

auto AssetDirectory::add_asset(const fs::path &path_) -> bool {
    if (path_.extension() != ".lrasset") {
        return false;
    }

    auto &app = EditorApp::get();
    auto *asset = app.asset_man.add_asset(path_);
    if (!asset) {
        return false;
    }

    LOG_TRACE("New asset added from '{}'", asset->path);
    this->assets.push_back(asset);

    return true;
}

AssetBrowserPanel::AssetBrowserPanel(std::string name_, bool open_)
    : PanelI(std::move(name_), open_) {
    auto &app = EditorApp::get();
    this->home_dir = this->add_directory(app.active_project->root_dir);
    this->current_dir = this->home_dir.get();
}

auto AssetBrowserPanel::add_directory(this AssetBrowserPanel &self, const fs::path &root_dir) -> std::unique_ptr<AssetDirectory> {
    auto dir = AssetDirectory::create(root_dir);
    dir->watch(self.file_watcher);
    populate_directory(dir.get(), self.file_watcher);

    return dir;
}

auto AssetBrowserPanel::find_directory_by_path(this AssetBrowserPanel &self, const fs::path &path) -> AssetDirectory * {
    const auto &relative_to_home = fs::relative(path, self.home_dir->path);

    AssetDirectory *cur_dir = self.home_dir.get();
    for (const auto &depth_path : relative_to_home) {
        if (depth_path == "/" || depth_path == "." || depth_path.empty()) {
            continue;
        }

        bool found = false;
        for (const auto &subdir : cur_dir->subdirs) {
            if (subdir->path.filename() == depth_path) {
                cur_dir = subdir.get();
                found = true;
                break;
            }
        }

        if (!found) {
            return nullptr;
        }
    }

    return cur_dir;
}

auto AssetBrowserPanel::find_closest_directory_by_path(this AssetBrowserPanel &self, const fs::path &path) -> AssetDirectory * {
    auto cur_dir_path = fs::relative(path, self.home_dir->path).parent_path();
    while (!cur_dir_path.has_parent_path() && cur_dir_path != ".") {
        auto dir = self.find_directory_by_path(cur_dir_path);
        if (dir) {
            return dir;
        }

        cur_dir_path = cur_dir_path.parent_path();
    }

    return nullptr;
}

void AssetBrowserPanel::draw_dir_contents(this AssetBrowserPanel &self) {
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

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { padding, padding });
    if (ImGui::BeginTable("asset_browser", tile_count, table_flags)) {
        for (auto &dir : self.current_dir->subdirs) {
            ImGui::TableNextColumn();

            const auto &file_name = dir->path.filename().string();
            if (ImGui::Button(file_name.c_str(), button_size)) {
                self.current_dir = dir.get();
            }
        }

        for (const auto *asset : self.current_dir->assets) {
            ImGui::TableNextColumn();

            const auto &file_name = asset->path.filename().string();
            ImGui::Button(file_name.c_str(), button_size);
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("ASSET_BY_UUID", &asset->uuid, sizeof(UUID));
                ImGui::EndDragDropSource();
            }
        }

        if (ImGui::BeginPopupContextWindow("asset_browser_ctx", ImGuiPopupFlags_AnyPopup | ImGuiPopupFlags_MouseButtonRight)) {
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
                fs::create_directory(self.current_dir->path / new_dir_name);
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
                app.asset_man.create_scene(new_scene_name, self.current_dir->path / (new_scene_name + ".json"));
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

        self.draw_file_tree(self.home_dir.get());

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

void AssetBrowserPanel::draw_file_tree(this AssetBrowserPanel &self, AssetDirectory *directory) {
    ImGuiTreeNodeFlags tree_node_dir_flags = ImGuiTreeNodeFlags_None;
    tree_node_dir_flags |= ImGuiTreeNodeFlags_FramePadding;
    tree_node_dir_flags |= ImGuiTreeNodeFlags_SpanFullWidth;

    auto tree_node_file_flags = tree_node_dir_flags;
    tree_node_file_flags |= ImGuiTreeNodeFlags_Leaf;

    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    for (auto &subdir : directory->subdirs) {
        const auto &file_name = subdir->path.filename().string();
        bool no_subdirs = subdir->subdirs.empty();
        bool no_files = subdir->assets.empty();
        auto cur_node_flags = no_subdirs ? tree_node_file_flags : tree_node_dir_flags;

        auto icon = Icon::fa::folder;
        if (no_subdirs && no_files) {
            icon = Icon::fa::folder_open;
        }

        if (ImGui::TreeNodeEx(file_name.c_str(), cur_node_flags, "%s  %s", icon, file_name.c_str())) {
            if (ImGui::IsItemToggledOpen() || ImGui::IsItemClicked()) {
                self.current_dir = subdir.get();
            }

            self.draw_file_tree(subdir.get());

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
                auto *parent_dir = self.find_directory_by_path(event_path);
                auto dir = AssetDirectory::create(full_path);
                dir->watch(self.file_watcher);
                populate_directory(dir.get(), self.file_watcher);
                parent_dir->add_subdir(std::move(dir));
            }

            if (file_event->action_mask & FileActionMask::Delete) {
                auto *dir = self.find_directory_by_path(event_path / file_event->file_name);
                auto *parent_dir = dir->parent;
                const auto &relative_to_cur = fs::relative(dir->path, self.current_dir->path);
                if (self.current_dir->path < dir->path) {
                    self.current_dir = self.home_dir.get();
                }

                self.file_watcher.remove_dir(dir->watch_descriptor);
                std::erase_if(parent_dir->subdirs, [dir](const auto &v) { return v.get() == dir; });
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
        self.current_dir = self.home_dir.get();
    }

    if (self.current_dir != self.home_dir.get()) {
        auto rel_path = fs::relative(self.current_dir->path, self.home_dir->path);
        for (const auto &v : rel_path) {
            auto dir_name = v.filename().string();

            ImGui::SameLine();
            ImGui::TextUnformatted("/");
            ImGui::SameLine();
            ImGui::TextUnformatted(dir_name.c_str());
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
