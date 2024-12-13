#include "Engine/Asset/Identifier.hh"

#include <random>

namespace lr {
Identifier Identifier::random() {
    thread_local std::random_device random_device;
    thread_local std::mt19937_64 random_engine(random_device());
    thread_local std::uniform_int_distribution<u64> uniform_dist;

    auto name = std::format("R{}", uniform_dist(random_engine));

    return { name };
}

Identifier Identifier::invalid() {
    return "INVALID";
}
}  // namespace lr
