#include "Editor/EditorModule.hh"

#include "Editor/Window/AssetBrowserWindow.hh"
#include "Editor/Window/ConsoleWindow.hh"
#include "Editor/Window/InspectorWindow.hh"
#include "Editor/Window/SceneBrowserWindow.hh"
#include "Editor/Window/ViewportWindow.hh"

#include "Editor/Themes.hh"

#include "Engine/Core/App.hh"
#include "Engine/Graphics/ImGuiRenderer.hh"
#include "Engine/Memory/Stack.hh"
#include "Engine/OS/File.hh"
#include "Engine/Util/JsonWriter.hh"

#include "Engine/Util/Icons/IconsMaterialDesignIcons.hh"
#include "Engine/Window/Window.hh"

#include <implot.h>
#include <simdjson.h>
namespace sj = simdjson;

namespace led {
static auto read_project_file(const fs::path &path) -> ls::option<ProjectFileInfo> {
    ZoneScoped;

    lr::File file(path, lr::FileAccess::Read);
    if (!file) {
        return ls::nullopt;
    }

    auto json = sj::padded_string(file.size);
    file.read(json.data(), file.size);

    sj::ondemand::parser parser;
    auto doc = parser.iterate(json);
    if (doc.error()) {
        LOG_ERROR("Failed to parse project file! {}", sj::error_message(doc.error()));
        return ls::nullopt;
    }

    auto name_json = doc["name"].get_string();
    if (name_json.error()) {
        return ls::nullopt;
    }

    return ProjectFileInfo{ .name = std::string(name_json.value_unsafe()) };
}

auto EditorModule::load_editor_data(this EditorModule &self) -> void {
    ZoneScoped;

    auto &asset_man = lr::App::mod<lr::AssetManager>();
    auto data_file_path = asset_man.asset_root_path(lr::AssetType::Root) / "editor.json";
    if (!fs::exists(data_file_path)) {
        self.save_editor_data();
        return;
    }

    lr::File file(data_file_path, lr::FileAccess::Read);
    if (!file) {
        // Its not that important
        return;
    }

    auto json = sj::padded_string(file.size);
    file.read(json.data(), file.size);

    sj::ondemand::parser parser;
    auto doc = parser.iterate(json);
    if (doc.error()) {
        LOG_ERROR("Failed to parse editor data file! {}", sj::error_message(doc.error()));
        return;
    }

    auto recent_projects = doc["recent_projects"].get_array();
    for (auto project_info_json : recent_projects) {
        auto project_path = project_info_json.get_string();
        if (project_path.error()) {
            continue;
        }

        auto project_info = read_project_file(project_path.value_unsafe());
        if (!project_info) {
            LOG_ERROR("Failed to read project information for {}!", project_path.value_unsafe());
            continue;
        }

        self.recent_project_infos.emplace(project_path.value_unsafe(), project_info.value());
    }
}

auto EditorModule::save_editor_data(this EditorModule &self) -> void {
    ZoneScoped;

    auto &asset_man = lr::App::mod<lr::AssetManager>();
    auto data_file_path = asset_man.asset_root_path(lr::AssetType::Root) / "editor.json";
    lr::JsonWriter json;
    json.begin_obj();
    json["recent_projects"].begin_array();
    for (const auto &[path, info] : self.recent_project_infos) {
        const auto &path_str = path.string();
        json << path_str;
    }

    json.end_array();
    json.end_obj();

    auto json_str = json.stream.view();
    lr::File file(data_file_path, lr::FileAccess::Write);
    file.write(json_str.data(), json_str.length());
    file.close();
}

auto EditorModule::new_project(this EditorModule &self, const fs::path &root_path, const std::string &name) -> std::unique_ptr<Project> {
    ZoneScoped;

    if (!fs::is_directory(root_path)) {
        LOG_ERROR("New projects must be inside a directory.");
        return nullptr;
    }

    // root_dir = /path/to/my_projects
    // proj_root_dir = root_dir / <PROJECT_NAME>
    // proj_file = proj_root_dir / world.lrproj
    const auto &proj_root_path = root_path / name;
    const auto &proj_file_path = proj_root_path / "world.lrproj";

    // Project Name
    // - Audio
    // - Models
    // - Scenes
    // - Shaders
    // - Prefabs
    // world.lrproj

    // Fresh project, the dev has freedom to change file structure
    // but we prepare initial structure to ease out some stuff
    if (!fs::exists(proj_root_path)) {
        // /<root_dir>/<name>/[dir_tree*]
        // Create project root directory first
        std::error_code err;
        fs::create_directories(proj_root_path, err);
        if (err) {
            LOG_ERROR("Failed to create directory '{}'! {}", proj_root_path, err.message());
            return nullptr;
        }

        // clang-format off
        constexpr static std::string_view dir_tree[] = {
            "Assets",
            "Assets/Audio",
            "Assets/Scenes",
            "Assets/Shaders",
            "Assets/Models",
            "Assets/Textures",
        };
        // clang-format on
        for (const auto &dir : dir_tree) {
            fs::create_directories(proj_root_path / dir);
        }
    }

    lr::JsonWriter json;
    json.begin_obj();
    json["name"] = name;
    json.end_obj();

    lr::File file(proj_file_path, lr::FileAccess::Write);
    if (!file) {
        LOG_ERROR("Failed to open file {}!", proj_file_path);
        return nullptr;
    }

    auto json_str = json.stream.view();
    file.write(json_str.data(), json_str.length());
    file.close();

    auto project = std::make_unique<Project>(proj_root_path, name);
    self.recent_project_infos.emplace(proj_file_path, ProjectFileInfo{ .name = name });
    self.save_editor_data();

    return project;
}

auto EditorModule::open_project(this EditorModule &self, const fs::path &path) -> std::unique_ptr<Project> {
    ZoneScoped;

    const auto &proj_root_dir = path.parent_path();
    auto is_valid = fs::exists(path) && path.has_extension() && path.extension() == ".lrproj";
    if (!is_valid) {
        return nullptr;
    }

    auto project_info = read_project_file(path);
    auto project = std::make_unique<Project>(proj_root_dir, project_info.value());
    self.recent_project_infos.emplace(path, ProjectFileInfo{ .name = project->name });
    self.save_editor_data();

    return project;
}

auto EditorModule::save_project(this EditorModule &, std::unique_ptr<Project> &project) -> void {
    ZoneScoped;

    if (!project->active_scene_uuid) {
        return;
    }

    auto &asset_man = lr::App::mod<lr::AssetManager>();
    const auto &scene_uuid = project->active_scene_uuid;
    auto *scene_asset = asset_man.get_asset(scene_uuid);
    asset_man.export_asset(scene_uuid, scene_asset->path);
}

auto EditorModule::set_active_project(this EditorModule &self, std::unique_ptr<Project> &&project) -> void {
    ZoneScoped;

    self.active_project = std::move(project);
}

auto EditorModule::get_asset_texture(this EditorModule &self, lr::Asset *asset) -> lr::Texture * {
    ZoneScoped;

    return self.get_asset_texture(asset->type);
}

auto EditorModule::get_asset_texture(this EditorModule &self, lr::AssetType asset_type) -> lr::Texture * {
    ZoneScoped;

    auto &asset_man = lr::App::mod<lr::AssetManager>();
    switch (asset_type) {
        case lr::AssetType::Model:
            return asset_man.get_texture(self.editor_assets["model"]);
        case lr::AssetType::Texture:
            return asset_man.get_texture(self.editor_assets["texture"]);
        case lr::AssetType::Scene:
            return asset_man.get_texture(self.editor_assets["scene"]);
        case lr::AssetType::Directory:
            return asset_man.get_texture(self.editor_assets["dir"]);
        default:
            return asset_man.get_texture(self.editor_assets["file"]);
    }
}

bool EditorModule::init(this EditorModule &self) {
    ZoneScoped;

    self.load_editor_data();
    Theme::dark();

    auto add_texture = [&self](std::string name, const fs::path &path) {
        auto &asset_man = lr::App::mod<lr::AssetManager>();
        auto asset_uuid = asset_man.create_asset(lr::AssetType::Texture, asset_man.asset_root_path(lr::AssetType::Root) / "editor" / path);
        asset_man.load_texture(asset_uuid);
        self.editor_assets.emplace(name, asset_uuid);
    };

    add_texture("dir", "dir.png");
    add_texture("scene", "scene.png");
    add_texture("model", "model.png");
    add_texture("texture", "texture.png");
    add_texture("file", "file.png");

    return true;
}

bool EditorModule::update(this EditorModule &self, f64 delta_time) {
    ZoneScoped;

    auto &asset_man = lr::App::mod<lr::AssetManager>();
    auto &device = lr::App::mod<lr::Device>();
    auto &window = lr::App::mod<lr::Window>();
    auto &imgui_renderer = lr::App::mod<lr::ImGuiRenderer>();

    auto swapchain_attachment = device.new_frame(window.swap_chain.value());
    swapchain_attachment = vuk::clear_image(std::move(swapchain_attachment), vuk::Black<f32>);
    imgui_renderer.begin_frame(delta_time, swapchain_attachment->extent);

    if (self.active_project) {
        const auto &active_scene_uuid = self.active_project->active_scene_uuid;
        if (active_scene_uuid) {
            auto *active_scene = asset_man.get_scene(active_scene_uuid);
            active_scene->tick(static_cast<f32>(delta_time));
        }
    }

    self.render(window.swap_chain.value());

    self.frame_profiler.measure(&device, delta_time);

    swapchain_attachment = imgui_renderer.end_frame(std::move(swapchain_attachment));
    device.end_frame(std::move(swapchain_attachment));

    return true;
}

static auto setup_dockspace(EditorModule &self) -> void {
    ZoneScoped;

    auto [asset_browser_panel_id, asset_browser_panel] = self.add_window<AssetBrowserWindow>("Asset Browser", ICON_MDI_IMAGE_MULTIPLE);
    auto [console_panel_id, console_panel] = self.add_window<ConsoleWindow>("Console", ICON_MDI_INFORMATION_SLAB_CIRCLE);
    auto [inspector_panel_id, inspector_panel] = self.add_window<InspectorWindow>("Inspector", ICON_MDI_WRENCH);
    auto [scene_browser_panel_id, scene_browser_panel] = self.add_window<SceneBrowserWindow>("Scene Browser", ICON_MDI_FILE_TREE);
    auto [viewport_panel_id, viewport_panel] = self.add_window<ViewportWindow>("Viewport", ICON_MDI_EYE);

    ImGuiViewport *viewport = ImGui::GetMainViewport();
    i32 dock_node_flags = ImGuiDockNodeFlags_PassthruCentralNode;

    auto dockspace_id_tmp = ImGui::GetID("EngineDockSpace");
    self.dockspace_id = dockspace_id_tmp;

    ImGui::DockBuilderRemoveNode(dockspace_id_tmp);
    ImGui::DockBuilderAddNode(dockspace_id_tmp, ImGuiDockNodeFlags_DockSpace | dock_node_flags);
    ImGui::DockBuilderSetNodeSize(dockspace_id_tmp, viewport->Size);

    auto main_dock_id = dockspace_id_tmp;
    auto up_dock_id = ImGui::DockBuilderSplitNode(main_dock_id, ImGuiDir_Up, 0.05f, nullptr, &main_dock_id);
    auto down_dock_id = ImGui::DockBuilderSplitNode(main_dock_id, ImGuiDir_Down, 0.30f, nullptr, &main_dock_id);
    auto left_dock_id = ImGui::DockBuilderSplitNode(main_dock_id, ImGuiDir_Left, 0.20f, nullptr, &main_dock_id);
    auto right_dock_id = ImGui::DockBuilderSplitNode(main_dock_id, ImGuiDir_Right, 0.25f, nullptr, &main_dock_id);

    ImGui::DockBuilderDockWindow("###up_dock", up_dock_id);
    ImGui::DockBuilderDockWindow(viewport_panel->name.c_str(), main_dock_id);
    ImGui::DockBuilderDockWindow(console_panel->name.c_str(), down_dock_id);
    ImGui::DockBuilderDockWindow(asset_browser_panel->name.c_str(), down_dock_id);
    ImGui::DockBuilderDockWindow(scene_browser_panel->name.c_str(), left_dock_id);
    ImGui::DockBuilderDockWindow(inspector_panel->name.c_str(), right_dock_id);
    ImGui::DockBuilderFinish(dockspace_id_tmp);
}

static auto draw_menu_bar(EditorModule &self) -> void {
    ZoneScoped;

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save Project", nullptr, false, self.active_project != nullptr)) {
                self.save_project(self.active_project);
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Exit")) {
                lr::App::close();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Frame Profiler")) {
                self.show_profiler = !self.show_profiler;
                self.frame_profiler.reset();
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

static auto draw_welcome_popup(EditorModule &self) -> void {
    ZoneScoped;

    auto &window = lr::App::mod<lr::Window>();
    auto center_window = glm::vec2(window.get_size()) * 0.5f;
    ImGui::SetNextWindowSize({ 480.0f, 350.0f }, ImGuiCond_Always);
    ImGui::SetNextWindowPos({ center_window.x, center_window.y }, ImGuiCond_Always, { 0.5f, 0.5f });
    constexpr auto popup_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::BeginPopupModal("###welcome", nullptr, popup_flags)) {
        //  ── HEADERS ─────────────────────────────────────────────────────────
        ImGui::TextUnformatted("placeholder");
        ImGui::InvisibleButton("placeholder", { -FLT_MAX, 75.0f });

        //  ── SECTIONS ────────────────────────────────────────────────────────
        if (ImGui::BeginTabBar("project_guide")) {
            if (ImGui::BeginTabItem("Open Project")) {
                ImGui::SeparatorText("Recent projects");
                i32 button_id = 0;
                for (const auto &[project_path, project_info] : self.recent_project_infos) {
                    lr::memory::ScopedStack stack;

                    auto path_str = stack.format_char("{}", project_path);
                    ImGui::PushID(button_id++);
                    if (ImGui::Button(project_info.name.c_str(), { -1.0f, 35.0f })) {
                        auto project = self.open_project(project_path);
                        ImGui::CloseCurrentPopup();
                        self.set_active_project(std::move(project));
                        ImGui::PopID();
                        break;
                    }
                    ImGui::SetItemTooltip("%s", path_str);
                    ImGui::PopID();
                }

                ImGui::SeparatorText("");

                if (ImGui::Button("Import...", { -1.0f, 35.0f })) {
                    window.show_dialog(
                        { .kind = lr::DialogKind::OpenFile,
                          .user_data = &self,
                          .title = "Select a project...",
                          .callback = [](void *user_data, const c8 *const *files, i32) {
                              if (!files || !*files) {
                                  return;
                              }

                              auto *app = static_cast<EditorModule *>(user_data);
                              auto lrporj_file = fs::path(files[0]);
                              auto project_info = read_project_file(lrporj_file);
                              if (project_info.has_value()) {
                                  app->recent_project_infos.emplace(lrporj_file, project_info.value());
                                  app->save_editor_data();
                              }
                          } }
                    );
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Create Project")) {
                auto content_region = ImGui::GetContentRegionAvail();
                auto item_spacing = ImGui::GetStyle().ItemSpacing;
                auto child_width = content_region.x - item_spacing.x * 0.5f;

                static std::string project_dir = {};
                static std::string project_name = {};

                ImGui::SeparatorText("Project Directory");
                ImGui::SetNextItemWidth(child_width * 0.92f);
                ImGui::InputTextWithHint("##project_dir", "/path/to/new/project", &project_dir);
                ImGui::SameLine();
                if (ImGui::Button(ICON_MDI_FOLDER, { -1.0f, 0.0f })) {
                    window.show_dialog(
                        { .kind = lr::DialogKind::OpenFolder, .title = "Select a directory...", .callback = [](void *, const c8 *const *files, i32) {
                             if (!files || !*files) {
                                 return;
                             }

                             project_dir = files[0];
                         } }
                    );
                }

                ImGui::SeparatorText("Project Name");
                ImGui::SetNextItemWidth(child_width + ImGui::CalcTextSize(" ").x);
                ImGui::InputTextWithHint("##project_name", "Shooter Game", &project_name);

                ImGui::Spacing();

                auto fields_empty = project_dir.empty() || project_name.empty();
                ImGui::BeginDisabled(fields_empty);
                if (ImGui::Button("Create Project", { -1.0f, 35.0f })) {
                    auto new_project = self.new_project(project_dir, project_name);
                    if (new_project) {
                        project_dir.clear();
                        project_name.clear();
                        ImGui::CloseCurrentPopup();
                        self.set_active_project(std::move(new_project));
                    }
                }
                ImGui::EndDisabled();

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::EndPopup();
    }
}

static auto draw_profiler(EditorModule &self) -> void {
    ZoneScoped;

    if (!self.show_profiler) {
        return;
    }

    if (ImGui::Begin("Frame Profiler", &self.show_profiler)) {
        if (ImPlot::BeginPlot("###profiler")) {
            ImPlot::SetupAxes("Timeline", "GPU Time", 0, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit);
            ImPlot::SetupAxisFormat(ImAxis_X1, "%g s");
            ImPlot::SetupAxisFormat(ImAxis_Y1, "%g ms");
            ImPlot::SetupAxisLimits(ImAxis_X1, self.frame_profiler.accumulated_time - 5.0, self.frame_profiler.accumulated_time, ImGuiCond_Always);
            ImPlot::SetupLegend(ImPlotLocation_NorthWest, ImPlotLegendFlags_Outside);

            for (const auto &[name, stats] : self.frame_profiler.pass_stats) {
                ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 1.0f);
                ImPlot::PlotLine(
                    name.c_str(),
                    self.frame_profiler.accumulated_times.data.data(),
                    stats.measured_times.data.data(),
                    stats.measured_times.size,
                    0,
                    stats.measured_times.offset,
                    sizeof(f64)
                );
            }

            ImPlot::EndPlot();
        }
    }

    ImGui::End();
}

auto EditorModule::render(this EditorModule &self, vuk::Swapchain &swap_chain) -> bool {
    ZoneScoped;

    auto *viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.0f);

    i32 dock_window_flags = ImGuiWindowFlags_NoDocking;
    dock_window_flags |= ImGuiWindowFlags_NoTitleBar;
    dock_window_flags |= ImGuiWindowFlags_NoCollapse;
    dock_window_flags |= ImGuiWindowFlags_NoResize;
    dock_window_flags |= ImGuiWindowFlags_NoMove;
    dock_window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
    dock_window_flags |= ImGuiWindowFlags_NoNavFocus;
    dock_window_flags |= ImGuiWindowFlags_NoScrollbar;
    dock_window_flags |= ImGuiWindowFlags_NoScrollWithMouse;
    dock_window_flags |= ImGuiWindowFlags_NoBackground;
    dock_window_flags |= ImGuiWindowFlags_MenuBar;

    ImGui::Begin("EditorDock", nullptr, dock_window_flags);
    ImGui::PopStyleVar(2);

    if (!self.dockspace_id) {
        setup_dockspace(self);
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    i32 dock_node_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    ImGui::DockSpace(self.dockspace_id.value(), ImVec2(0.0f, 0.0f), dock_node_flags | ImGuiDockNodeFlags_NoWindowMenuButton);
    ImGui::PopStyleVar();

    draw_menu_bar(self);

    ImGui::End();

    for (auto &window : self.windows) {
        window->do_render(swap_chain);
    }

    if (!self.active_project) {
        ImGui::OpenPopup("###welcome");
    }

    draw_welcome_popup(self);
    draw_profiler(self);

    return true;
}

auto EditorModule::destroy(this EditorModule &self) -> void {
    ZoneScoped;

    auto &asset_man = lr::App::mod<lr::AssetManager>();
    for (const auto &[name, uuid] : self.editor_assets) {
        asset_man.unload_asset(uuid);
    }

    self.windows.clear();
    self.active_project.reset();
}

auto EditorModule::remove_window(this EditorModule &self, usize window_id) -> void {
    ZoneScoped;

    self.windows.erase(self.windows.begin() + static_cast<std::ranges::range_difference_t<decltype(self.windows)>>(window_id));
}

} // namespace led

static const u64 GDataTypeInfo[] = {
    sizeof(char), // ImGuiDataType_S8
    sizeof(unsigned char),
    sizeof(short), // ImGuiDataType_S16
    sizeof(unsigned short),
    sizeof(int), // ImGuiDataType_S32
    sizeof(unsigned int),
    sizeof(ImS64), // ImGuiDataType_S64
    sizeof(ImU64),
    sizeof(float), // ImGuiDataType_Float (float are promoted to double in va_arg)
    sizeof(double), // ImGuiDataType_Double
    sizeof(bool), // ImGuiDataType_Bool
};

bool ImGui::drag_vec(i32 id, void *data, usize components, ImGuiDataType data_type) {
    ImGuiWindow *window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    static ImU32 component_colors[] = {
        IM_COL32(255, 0, 0, 255),
        IM_COL32(0, 255, 0, 255),
        IM_COL32(0, 0, 255, 255),
        ImGui::GetColorU32(ImGuiCol_Text),
    };

    bool value_changed = false;
    ImGui::BeginGroup();

    ImGui::PushID(id);
    ImGui::PushMultiItemsWidths(static_cast<i32>(components), ImGui::GetContentRegionAvail().x);
    for (usize i = 0; i < components; i++) {
        if (i > 0) {
            ImGui::SameLine(0, GImGui->Style.ItemInnerSpacing.x);
        }

        ImGui::PushID(static_cast<i32>(i));
        value_changed |= ImGui::DragScalar("", data_type, data, 0.01f);
        ImGui::PopItemWidth();
        ImGui::PopID();

        auto rect_min = ImGui::GetItemRectMin();
        auto rect_max = ImGui::GetItemRectMax();
        auto spacing = ImGui::GetStyle().FramePadding.x / 2.0f;

        if (components > 1) {
            ImGui::GetWindowDrawList()->AddLine({ rect_min.x + spacing, rect_min.y }, { rect_min.x + spacing, rect_max.y }, component_colors[i], 4);
        }

        data = (void *)((char *)data + GDataTypeInfo[data_type]);
    }
    ImGui::PopID();
    ImGui::EndGroup();

    return value_changed;
}

void ImGui::center_text(std::string_view str) {
    auto window_size = ImGui::GetWindowSize();
    auto text_size = ImGui::CalcTextSize(str.data(), str.data() + str.length());

    ImGui::SetCursorPos({ (window_size.x - text_size.x) * 0.5f, (window_size.y - text_size.y) * 0.5f });
    ImGui::TextUnformatted(str.data(), str.data() + str.length());
}

bool ImGui::image_button(std::string_view text, ImTextureID texture_id, const ImVec2 &button_size) {
    lr::memory::ScopedStack stack;

    auto &style = ImGui::GetStyle();
    auto min_size = std::min(button_size.x, button_size.y);
    min_size -= style.FramePadding.x * 2.0f;
    auto new_size = ImVec2(min_size, min_size);

    ImGui::BeginGroup();

    auto cursor_pos = ImGui::GetCursorPos();
    cursor_pos.x += style.FramePadding.x;
    cursor_pos.y += style.FramePadding.y;
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, { 0.5, 0.9 });
    auto pressed = ImGui::Button(stack.null_terminate_cstr(text), button_size);
    ImGui::PopStyleVar();

    ImGui::SetNextItemAllowOverlap();
    ImGui::SetCursorPos(cursor_pos);
    ImGui::Image(texture_id, new_size);
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);

    ImGui::EndGroup();

    return pressed;
}

void ImGui::text_sv(std::string_view str) {
    ImGui::TextUnformatted(static_cast<const c8 *>(str.data()), static_cast<const c8 *>(str.data() + str.length()));
}
