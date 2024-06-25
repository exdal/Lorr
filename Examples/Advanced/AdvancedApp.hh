#include "Engine/Core/Application.hh"

struct AdvancedApp : lr::Application {
    bool prepare(this AdvancedApp &self);
    bool update(this AdvancedApp &self, f32 delta_time);

    bool do_prepare() override { return prepare(); }
    bool do_update(f32 delta_time) override { return update(delta_time); }
};
