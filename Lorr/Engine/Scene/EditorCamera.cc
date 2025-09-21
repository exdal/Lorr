#include "Engine/Scene/EditorCamera.hh"

namespace lr {
auto EditorCamera::update(this EditorCamera &self, f32 delta_time, const glm::vec3 &target_velocity) -> void {
    ZoneScoped;

    auto direction = self.direction();
    auto up = glm::vec3(0.0f, 1.0f, 0.0f);
    auto right = glm::normalize(glm::cross(direction, up));

    auto cur_speed = glm::length(target_velocity);
    auto acceleration_rate = cur_speed > 0.0f ? self.accel_speed : self.decel_speed;
    self.velocity = glm::mix(self.velocity, target_velocity, glm::min(1.0f, acceleration_rate * delta_time));
    auto world_velocity = self.velocity.x * direction + self.velocity.y * up + self.velocity.z * right;
    self.position += world_velocity * delta_time;

    auto projection_mat = glm::perspectiveRH_ZO(glm::radians(self.fov), self.aspect_ratio(), self.far_clip, self.near_clip);
    projection_mat[1][1] *= -1.0f;

    auto view_mat = glm::lookAt(self.position, self.position + direction, up);
    auto projection_view_mat = projection_mat * view_mat;
    self.projection_mat = projection_mat;
    self.inv_projection_mat = glm::inverse(projection_mat);
    self.view_mat = view_mat;
    self.inv_view_mat = glm::inverse(view_mat);
    self.projection_view_mat = projection_mat * view_mat;
    self.inv_projection_view_mat = glm::inverse(projection_view_mat);
    self.acceptable_lod_error = 2.0f;
}

auto EditorCamera::direction(this EditorCamera &self) -> glm::vec3 {
    ZoneScoped;

    auto direction = glm::vec3(
        glm::cos(glm::radians(self.yaw)) * glm::cos(glm::radians(self.pitch)),
        glm::sin(glm::radians(self.pitch)),
        glm::sin(glm::radians(self.yaw)) * glm::cos(glm::radians(self.pitch))
    );
    return glm::normalize(direction);
}
} // namespace lr
