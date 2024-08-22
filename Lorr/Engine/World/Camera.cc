#include "Camera.hh"

#include <glm/gtx/transform.hpp>

namespace lr {
void PerspectiveCamera::calc_proj_matrix(this PerspectiveCamera &self) {
    ZoneScoped;

    self.projection_matrix = glm::perspectiveLH(glm::radians(self.fov), self.aspect_ratio, 0.1f, 1000000.0f);
    self.projection_matrix[1][1] *= -1;
}

glm::mat4 PerspectiveCamera::calc_view_matrix(this PerspectiveCamera &self) {
    ZoneScoped;

    self.yaw = std::fmod(self.yaw, 360.0f);
    self.pitch = glm::clamp(self.pitch, -89.0f, 89.0f);

    self.orientation = glm::angleAxis(glm::radians(self.yaw), glm::vec3(0.0f, -1.0f, 0.0f));
    self.orientation = glm::angleAxis(glm::radians(self.pitch), glm::vec3(1.0f, 0.0f, 0.0f)) * self.orientation;
    self.orientation = glm::normalize(self.orientation);

    glm::mat4 rotation = glm::toMat4(self.orientation);
    glm::mat4 translation = glm::translate(glm::mat4(1.f), -self.position);

    return rotation * translation;
}

void PerspectiveCamera::update(this PerspectiveCamera &self, f32 delta_time) {
    ZoneScoped;

    auto inv_orient = glm::conjugate(self.orientation);
    self.position += glm::vec3(inv_orient * glm::vec4(self.velocity * delta_time, 0.0f));

    self.calc_proj_matrix();
    self.calc_view_matrix();

    self.velocity = {};
}
}  // namespace lr
