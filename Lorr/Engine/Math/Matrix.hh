#pragma once

namespace lr::Math {
inline auto calc_frustum_planes(glm::mat4 &view_proj_mat, glm::vec4 (&planes)[6]) -> void {
    ZoneScoped;

    for (auto i = 0; i < 4; ++i) {
        planes[0][i] = view_proj_mat[i][3] + view_proj_mat[i][0];
    }
    for (auto i = 0; i < 4; ++i) {
        planes[1][i] = view_proj_mat[i][3] - view_proj_mat[i][0];
    }
    for (auto i = 0; i < 4; ++i) {
        planes[2][i] = view_proj_mat[i][3] + view_proj_mat[i][1];
    }
    for (auto i = 0; i < 4; ++i) {
        planes[3][i] = view_proj_mat[i][3] - view_proj_mat[i][1];
    }
    for (auto i = 0; i < 4; ++i) {
        planes[4][i] = view_proj_mat[i][3] + view_proj_mat[i][2];
    }
    for (auto i = 0; i < 4; ++i) {
        planes[5][i] = view_proj_mat[i][3] - view_proj_mat[i][2];
    }

    for (auto &plane : planes) {
        plane /= glm::length(glm::vec3(plane));
        plane.w = -plane.w;
    }
}
} // namespace lr::Math
