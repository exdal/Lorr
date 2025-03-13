#include "Editor/Panel/AssetBrowserPanel.hh"

#include "Editor/EditorApp.hh"
#include "Engine/Util/Icons/IconsMaterialDesignIcons.hh"

namespace lr {
auto is_subpath(const fs::path &parent, const fs::path &child) -> bool {
    auto parent_abs = fs::weakly_canonical(parent);
    auto child_abs = fs::weakly_canonical(child);

    auto parent_iter = parent_abs.begin();
    auto child_iter = child_abs.begin();

    for (; parent_iter != parent_abs.end(); ++parent_iter, ++child_iter) {
        if (child_iter == child_abs.end() || *parent_iter != *child_iter) {
            return false;
        }
    }

    return true;
}

struct AssetDirectoryCallbacks {
    void *user_data = nullptr;
    void (*on_new_directory)(void *user_data, AssetDirectory *directory) = nullptr;
    void (*on_new_asset)(void *user_data, UUID &asset_uuid) = nullptr;
};

auto populate_directory(AssetDirectory *dir, const AssetDirectoryCallbacks &callbacks) -> void {
    for (const auto &entry : fs::directory_iterator(dir->path)) {
        const auto &path = entry.path();
        if (entry.is_directory()) {
            auto *new_dir = dir->add_subdir(path);
            if (callbacks.on_new_directory) {
                callbacks.on_new_directory(callbacks.user_data, new_dir);
            }

            populate_directory(new_dir, callbacks);
        } else if (entry.is_regular_file()) {
            auto new_asset_uuid = dir->add_asset(path);
            if (callbacks.on_new_asset) {
                callbacks.on_new_asset(callbacks.user_data, new_asset_uuid);
            }
        }
    }
}

AssetDirectory::AssetDirectory(fs::path path_, FileWatcher *file_watcher_, AssetDirectory *parent_)
    : path(std::move(path_)),
      file_watcher(file_watcher_),
      parent(parent_) {
    this->watch_descriptor = file_watcher_->watch_dir(this->path);
}

AssetDirectory::~AssetDirectory() {
    auto &app = EditorApp::get();
    this->file_watcher->remove_dir(this->watch_descriptor);
    for (const auto &asset_uuid : this->asset_uuids) {
        app.asset_man.delete_asset(asset_uuid);
    }
}

auto AssetDirectory::add_subdir(this AssetDirectory &self, const fs::path &path) -> AssetDirectory * {
    auto dir = std::make_unique<AssetDirectory>(path, self.file_watcher, &self);

    return self.add_subdir(std::move(dir));
}

auto AssetDirectory::add_subdir(this AssetDirectory &self, std::unique_ptr<AssetDirectory> &&directory) -> AssetDirectory * {
    auto *ptr = directory.get();
    self.subdirs.push_back(std::move(directory));

    return ptr;
}

auto AssetDirectory::add_asset(this AssetDirectory &self, const fs::path &path) -> UUID {
    auto &app = Application::get();
    auto asset_uuid = app.asset_man.import_asset(path);
    if (!asset_uuid) {
        return UUID(nullptr);
    }

    self.asset_uuids.emplace(asset_uuid);

    return asset_uuid;
}

AssetBrowserPanel::AssetBrowserPanel(std::string name_, bool open_)
    : PanelI(std::move(name_), open_) {
    auto &app = EditorApp::get();

    this->file_watcher.init(app.active_project->root_dir);
    this->home_dir = this->add_directory(app.active_project->root_dir);
    this->current_dir = this->home_dir.get();
}

auto AssetBrowserPanel::add_directory(this AssetBrowserPanel &self, const fs::path &path) -> std::unique_ptr<AssetDirectory> {
    AssetDirectory *parent = nullptr;
    if (path.has_parent_path()) {
        parent = self.find_directory(path.parent_path());
    }

    if (parent) {
        for (const auto &v : parent->subdirs) {
            if (v->path == path) {
                return nullptr;
            }
        }

        // NOTE: If there is parent, we are not creating new root directory.
        // So just return nullptr, instead of another owning directory.
        // This is intentional. You should find this new child directory
        // through `::find_directory`.
        auto dir = std::make_unique<AssetDirectory>(path, &self.file_watcher, parent);
        populate_directory(dir.get(), {});
        parent->add_subdir(std::move(dir));
        return nullptr;
    }

    auto dir = std::make_unique<AssetDirectory>(path, &self.file_watcher, nullptr);
    populate_directory(dir.get(), {});

    return dir;
}

auto AssetBrowserPanel::remove_directory(this AssetBrowserPanel &self, const fs::path &path) -> void {
    auto *dir = self.find_directory(path);
    if (dir == nullptr) {
        return;
    }

    self.remove_directory(dir);
}

auto AssetBrowserPanel::remove_directory(this AssetBrowserPanel &self, AssetDirectory *directory) -> void {
    auto *parent_dir = directory->parent;
    const auto &path = directory->path;
    fs::remove_all(path);

    // If we are inside, set cur dir to closest alive dir
    if (is_subpath(path, self.current_dir->path)) {
        self.current_dir = parent_dir;
    }

    // Remove found directory from its parent
    if (parent_dir) {
        std::erase_if(parent_dir->subdirs, [directory](const auto &v) {  //
            return v->path == directory->path;
        });
    }
}

auto AssetBrowserPanel::find_directory(this AssetBrowserPanel &self, const fs::path &path) -> AssetDirectory * {
    if (!self.home_dir) {
        return nullptr;
    }

    AssetDirectory *cur_dir = self.home_dir.get();
    const auto &relative_to_home = fs::relative(path, self.home_dir->path);
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

    auto *dir_texture = app.asset_man.get_texture(app.layout.editor_assets["dir"]);
    auto dir_image = app.imgui_renderer.add_image(dir_texture->image_view);

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { padding, padding });
    if (ImGui::BeginTable("asset_browser", tile_count, table_flags)) {
        std::vector<AssetDirectory *> deleting_dirs = {};  // this is to avoid iterator corruption
        for (const auto &subdir : self.current_dir->subdirs) {
            ImGui::TableNextColumn();

            const auto &file_name = subdir->path.filename().string();
            if (ImGuiLR::image_button(file_name, dir_image, button_size)) {
                self.current_dir = subdir.get();
            }
            if (ImGui::BeginPopupContextItem(file_name.c_str())) {
                if (ImGui::MenuItem("Delete")) {
                    deleting_dirs.push_back(subdir.get());
                }

                ImGui::EndPopup();
            }
        }

        for (auto *dir : deleting_dirs) {
            self.remove_directory(dir);
        }

        for (const auto &uuid : self.current_dir->asset_uuids) {
            ImGui::TableNextColumn();

            auto *asset = app.asset_man.get_asset(uuid);
            if (!asset) {
                continue;
            }

            const auto &file_name = asset->path.filename().string();
            auto *asset_texture = app.layout.get_asset_texture(asset);
            auto asset_image = app.imgui_renderer.add_image(asset_texture->image_view);
            ImGuiLR::image_button(file_name, asset_image, button_size);
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
                auto new_dir_path = self.current_dir->path / new_dir_name;
                fs::create_directory(new_dir_path);
                self.current_dir->add_subdir(new_dir_path);

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
                auto new_scene_path = self.current_dir->path / (new_scene_name + ".json");
                auto new_scene_uuid = app.asset_man.create_asset(AssetType::Scene, new_scene_path);

                app.asset_man.init_new_scene(new_scene_uuid, new_scene_name);
                app.asset_man.export_asset(new_scene_uuid, new_scene_path);
                app.asset_man.unload_scene(new_scene_uuid);

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

    for (const auto &subdir : directory->subdirs) {
        bool no_subdirs = subdir->subdirs.empty();
        bool no_files = subdir->asset_uuids.empty();
        auto cur_node_flags = no_subdirs ? tree_node_file_flags : tree_node_dir_flags;

        auto icon = ICON_MDI_FOLDER;
        if (no_subdirs && no_files) {
            icon = ICON_MDI_FOLDER_OPEN;
        }

        const auto &file_name = subdir->path.filename().string();
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
                self.add_directory(full_path);
            }

            if (file_event->action_mask & FileActionMask::Delete) {
                self.remove_directory(full_path);
            }
        } else {
            if (file_event->action_mask & FileActionMask::Create) {
                auto *event_dir = self.find_directory(event_path);
                if (!event_dir) {
                    continue;
                }

                event_dir->add_asset(full_path);
            }
        }
    }
}

void AssetBrowserPanel::render(this AssetBrowserPanel &self) {
    self.poll_watch_events();

    ImGui::Begin(self.name.data(), nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    auto avail_region = ImGui::GetContentRegionAvail();

    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_RELOAD)) {
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_HOME)) {
        self.current_dir = self.home_dir.get();
    }

    const auto &cur_rel_to_home = fs::relative(self.current_dir->path, self.home_dir->path);
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
