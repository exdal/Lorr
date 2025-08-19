#include "Engine/Scene/EditorCamera.hh"

namespace lr {
auto EditorCamera::update(this EditorCamera &self, f32 delta_time, const glm::vec3 &target_velocity) -> void {
    ZoneScoped;

    auto inv_orient = glm::conjugate(lr::Math::quat_dir(self.rotation));
    auto acceleration_rate = glm::length(target_velocity) > 0.0f ? self.accel_speed : self.decel_speed;
    self.velocity = glm::mix(self.velocity, target_velocity, glm::min(1.0f, acceleration_rate * delta_time));
    self.position += inv_orient * self.velocity * delta_time;

    auto projection_mat = glm::perspectiveRH_ZO(glm::radians(self.fov), self.aspect_ratio(), self.far_clip, self.near_clip);
    projection_mat[1][1] *= -1.0f;

    auto translation_mat = glm::translate(glm::mat4(1.0f), -self.position);
    auto rotation_mat = glm::mat4_cast(lr::Math::quat_dir(self.rotation));
    auto view_mat = rotation_mat * translation_mat;
    auto projection_view_mat = projection_mat * view_mat;

    self.frustum_projection_view_mat = self.projection_view_mat;
    self.projection_mat = projection_mat;
    self.view_mat = view_mat;
    self.projection_view_mat = projection_mat * view_mat;
    self.inv_view_mat = glm::inverse(view_mat);
    self.inv_projection_view_mat = glm::inverse(projection_view_mat);
    self.acceptable_lod_error = 2.0f;
}
} // namespace lr
