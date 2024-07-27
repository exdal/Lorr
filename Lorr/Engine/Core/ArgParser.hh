#pragma once

#include "Engine/Memory/Hasher.hh"

#include <ls/string.hh>

namespace lr {
struct ArgParser {
private:
    struct Arg {
        u64 param_hash = 0;
        std::string value = {};
    };
    std::vector<Arg> args = {};

public:
    ArgParser(ls::span<c8 *> args_) {
        for (usize i = 0; i < args_.size(); i++) {
            std::string_view param_sv(args_[i]);
            u64 h = fnv64_str(param_sv);

            if (param_sv.starts_with("--") && i + 1 < args_.size()) {
                std::string_view value_sv(args_[i + 1]);
                auto value_str = ls::escape_str(value_sv);
                if (!value_str.starts_with("--")) {
                    args.emplace_back(h, value_str);
                    i++;
                }
            } else {
                // It is not param, count it as a value
                args.emplace_back(h, ls::escape_str(param_sv));
            }
        }
    }

    constexpr std::optional<std::string_view> operator[](std::string_view arg) {
        ZoneScoped;

        u64 hash = fnv64_str(arg);
        for (auto &[h, v] : args) {
            if (h == hash) {
                return v;
            }
        }

        return std::nullopt;
    }

    constexpr std::optional<std::string_view> operator[](usize i) {
        ZoneScoped;

        if (i >= args.size()) {
            return std::nullopt;
        }

        return args[i].value;
    }
};

}  // namespace lr
