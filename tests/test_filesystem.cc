#include <gtest/gtest.h>

TEST(filesystem, directory_iterator) {
    auto root = fs::temp_directory_path();
    fs::path resources = root / "resources" / "models" / "city" / "lamps" / "lamp1.gltf";
    fs::path rel_resources = fs::relative(resources, root);

    for (const auto &v : rel_resources) {
        std::println("{}", v.filename().string());
    }
}
