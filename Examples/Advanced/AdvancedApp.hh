#include "Engine/Core/Application.hh"

struct AdvancedApp : lr::Application {
    lr::ImageID depth_attachment_id = lr::ImageID::Invalid;
    lr::ImageViewID depth_attachment_view_id = lr::ImageViewID::Invalid;
    lr::TaskImageID depth_image_id = lr::TaskImageID::Invalid;
    lr::ModelID model_id = lr::ModelID::Invalid;

    bool prepare(this AdvancedApp &self);
    bool update(this AdvancedApp &self, f32 delta_time);

    bool do_prepare() override { return prepare(); }
    bool do_update(f32 delta_time) override { return update(delta_time); }
};
