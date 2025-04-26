#include "Editor/Window/AssetBrowserWindow.hh"

#include "Editor/EditorApp.hh"
#include "Engine/Util/Icons/IconsMaterialDesignIcons.hh"

namespace led {
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
    void (*on_new_asset)(void *user_data, lr::UUID &asset_uuid) = nullptr;
};

auto populate_directory(AssetDirectory *dir, const AssetDirectoryCallbacks &callbacks) -> void {
    for (const auto &entry : fs::directory_iterator(dir->path)) {
        const auto &path = entry.path();
        if (entry.is_directory()) {
            AssetDirectory *cur_subdir = nullptr;
            auto dir_it = std::ranges::find_if(dir->subdirs, [&](const auto &v) { return path == v->path; });
            if (dir_it == dir->subdirs.end()) {
                auto *new_dir = dir->add_subdir(path);
                if (callbacks.on_new_directory) {
                    callbacks.on_new_directory(callbacks.user_data, new_dir);
                }

                cur_subdir = new_dir;
            } else {
                cur_subdir = dir_it->get();
            }

            populate_directory(cur_subdir, callbacks);
        } else if (entry.is_regular_file()) {
            auto new_asset_uuid = dir->add_asset(path);
            if (callbacks.on_new_asset) {
                callbacks.on_new_asset(callbacks.user_data, new_asset_uuid);
            }
        }
    }
}

AssetDirectory::AssetDirectory(fs::path path_, AssetDirectory *parent_) : path(std::move(path_)), parent(parent_) {}

AssetDirectory::~AssetDirectory() {
    auto &app = lr::Application::get();
    for (const auto &asset_uuid : this->asset_uuids) {
        if (app.asset_man.get_asset(asset_uuid)) {
            app.asset_man.delete_asset(asset_uuid);
        }
    }
}

auto AssetDirectory::add_subdir(this AssetDirectory &self, const fs::path &path) -> AssetDirectory * {
    auto dir = std::make_unique<AssetDirectory>(path, &self);

    return self.add_subdir(std::move(dir));
}

auto AssetDirectory::add_subdir(this AssetDirectory &self, std::unique_ptr<AssetDirectory> &&directory) -> AssetDirectory * {
    auto *ptr = directory.get();
    self.subdirs.push_back(std::move(directory));

    return ptr;
}

auto AssetDirectory::add_asset(this AssetDirectory &self, const fs::path &path) -> lr::UUID {
    auto &app = lr::Application::get();
    auto asset_uuid = app.asset_man.import_asset(path);
    if (!asset_uuid) {
        return lr::UUID(nullptr);
    }

    self.asset_uuids.emplace(asset_uuid);

    return asset_uuid;
}

auto AssetDirectory::refresh(this AssetDirectory &self) -> void {
    populate_directory(&self, {});
}

AssetBrowserWindow::AssetBrowserWindow(std::string name_, bool open_) : IWindow(std::move(name_), open_) {}

auto AssetBrowserWindow::add_directory(this AssetBrowserWindow &self, const fs::path &path) -> std::unique_ptr<AssetDirectory> {
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
        auto dir = std::make_unique<AssetDirectory>(path, parent);
        populate_directory(dir.get(), {});
        parent->add_subdir(std::move(dir));
        return nullptr;
    }

    auto dir = std::make_unique<AssetDirectory>(path, nullptr);
    populate_directory(dir.get(), {});

    return dir;
}

auto AssetBrowserWindow::remove_directory(this AssetBrowserWindow &self, const fs::path &path) -> void {
    auto *dir = self.find_directory(path);
    if (dir == nullptr) {
        return;
    }

    self.remove_directory(dir);
}

auto AssetBrowserWindow::remove_directory(this AssetBrowserWindow &self, AssetDirectory *directory) -> void {
    auto *parent_dir = directory->parent;
    const auto &path = directory->path;
    fs::remove_all(path);

    // If we are inside, set cur dir to closest alive dir
    if (is_subpath(path, self.current_dir->path)) {
        self.current_dir = parent_dir;
    }

    // Remove found directory from its parent
    if (parent_dir) {
        std::erase_if(parent_dir->subdirs, [directory](const auto &v) { //
            return v->path == directory->path;
        });
    }
}

auto AssetBrowserWindow::find_directory(this AssetBrowserWindow &self, const fs::path &path) -> AssetDirectory * {
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

static auto draw_dir_contents(AssetBrowserWindow &self) -> void {
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

    auto *dir_texture = app.get_asset_texture(lr::AssetType::Directory);
    auto dir_image = app.imgui_renderer.add_image(dir_texture->image_view);

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { padding, padding });
    if (ImGui::BeginTable("asset_browser", tile_count, table_flags)) {
        std::vector<AssetDirectory *> deleting_dirs = {}; // this is to avoid iterator corruption
        for (const auto &subdir : self.current_dir->subdirs) {
            ImGui::TableNextColumn();

            const auto &file_name = subdir->path.filename().string();
            if (ImGui::image_button(file_name, dir_image, button_size)) {
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
            self.home_dir->refresh();
        }

        for (const auto &uuid : self.current_dir->asset_uuids) {
            ImGui::TableNextColumn();

            auto *asset = app.asset_man.get_asset(uuid);
            if (!asset) {
                continue;
            }

            const auto &file_name = asset->path.filename().string();
            auto *asset_texture = app.get_asset_texture(asset);
            auto asset_image = app.imgui_renderer.add_image(asset_texture->image_view);
            ImGui::image_button(file_name, asset_image, button_size);
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("ASSET_BY_UUID", &asset->uuid, sizeof(lr::UUID));
                ImGui::EndDragDropSource();
            }
        }

        if (ImGui::BeginPopupContextWindow(
                "asset_browser_ctx",
                ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverExistingPopup
            ))
        {
            if (ImGui::BeginMenu("Create...")) {
                open_create_dir_popup = ImGui::MenuItem("Directory");
                ImGui::Separator();
                open_create_scene_popup = ImGui::MenuItem("Scene");

                ImGui::EndMenu();
            }

            ImGui::EndPopup();
        }

        {
            static bool new_dir_err = false;
            static std::string new_dir_name = {};

            if (open_create_dir_popup) {
                new_dir_err = false;
                new_dir_name = "New Directory";
                ImGui::OpenPopup("###create_dir_popup");
            }

            if (ImGui::BeginPopupModal("Create Directory...###create_dir_popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::InputText("", &new_dir_name);

                if (new_dir_err) {
                    ImGui::TextColored({ 1.0, 0.0, 0.0, 1.0 }, "Cannot create directory with this name.");
                }

                if (new_dir_name.empty()) {
                    ImGui::BeginDisabled();
                }

                if (ImGui::Button("OK")) {
                    auto new_dir_path = self.current_dir->path / new_dir_name;
                    new_dir_err = !fs::create_directory(new_dir_path);
                    if (!new_dir_err) {
                        self.current_dir->add_subdir(new_dir_path);
                        self.current_dir->refresh();

                        ImGui::CloseCurrentPopup();
                    }
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
        }

        {
            static bool new_scene_err = false;
            static std::string new_scene_name = {};

            if (open_create_scene_popup) {
                new_scene_err = false;
                new_scene_name = "New Scene";
                ImGui::OpenPopup("###create_scene_popup");
            }

            if (ImGui::BeginPopupModal("Create Scene...###create_scene_popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::InputText("", &new_scene_name);

                if (new_scene_err) {
                    ImGui::TextColored({ 1.0, 0.0, 0.0, 1.0 }, "Cannot create scene with this name.");
                }

                if (new_scene_name.empty()) {
                    ImGui::BeginDisabled();
                }

                if (ImGui::Button("OK")) {
                    auto new_scene_path = self.current_dir->path / (new_scene_name + ".json");
                    if (!fs::exists(new_scene_path)) {
                        new_scene_err = false;
                        auto new_scene_uuid = app.asset_man.create_asset(lr::AssetType::Scene, new_scene_path);
                        new_scene_err |= !app.asset_man.init_new_scene(new_scene_uuid, new_scene_name);
                        new_scene_err |= !app.asset_man.export_asset(new_scene_uuid, new_scene_path);
                        if (!new_scene_err) {
                            app.asset_man.unload_scene(new_scene_uuid);
                            self.current_dir->refresh();
                            ImGui::CloseCurrentPopup();
                        }
                    } else {
                        new_scene_err = true;
                    }
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
        }

        ImGui::EndTable();
    }

    ImGui::PopStyleVar();
}

static auto draw_file_tree(AssetBrowserWindow &self, AssetDirectory *directory) -> void {
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

            draw_file_tree(self, subdir.get());

            ImGui::TreePop();
        }
    }
}

static auto draw_project_tree(AssetBrowserWindow &self) -> void {
    ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg;
    table_flags |= ImGuiTableFlags_ContextMenuInBody;
    table_flags |= ImGuiTableFlags_ScrollY;
    table_flags |= ImGuiTableFlags_NoPadInnerX;
    table_flags |= ImGuiTableFlags_NoPadOuterX;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 0, 0 });
    if (ImGui::BeginTable("project_tree", 1, table_flags)) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        draw_file_tree(self, self.home_dir.get());

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

static auto draw_file_paths(AssetBrowserWindow &self) -> void {
    ZoneScoped;

    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_RELOAD)) {
        self.home_dir->refresh();
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
}

void AssetBrowserWindow::render(this AssetBrowserWindow &self) {
    ZoneScoped;

    auto &app = EditorApp::get();

    // If we just opened the project and home dir is missing
    if (app.active_project && self.home_dir == nullptr) {
        auto new_home_dir = self.add_directory(app.active_project->root_dir);
        self.current_dir = new_home_dir.get();
        self.home_dir = std::move(new_home_dir);
    }

    if (ImGui::Begin(self.name.data(), nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        if (app.active_project) {
            draw_file_paths(self);
        }

        // ACTUAL FILE TREE
        auto avail_region = ImGui::GetContentRegionAvail();
        if (ImGui::BeginTable("asset_browser_context", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_ContextMenuInBody, avail_region)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            if (app.active_project) {
                draw_project_tree(self);
            }
            ImGui::TableNextColumn();
            if (app.active_project) {
                draw_dir_contents(self);
            }

            ImGui::EndTable();
        }
    }

    ImGui::End();
}

} // namespace led
