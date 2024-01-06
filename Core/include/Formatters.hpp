#pragma once

#include <unordered_map>
#include <vector>
#include <tuple>
#include <utility>

#include <fmt/core.h>

template <class Key, class Value> struct fmt::formatter<std::unordered_map<Key, Value>> : formatter<const char*> {
    auto format(const std::unordered_map<Key, Value>& umap, format_context& ctx) const
        -> decltype(ctx.out()) {
        auto it = ctx.out();
        format_to(it, "{{");

        size_t count = 0;
        size_t total = umap.size();
        for (const auto& p : umap) {
            it = format_to(it, "{}: {}", p.first, p.second);
            if (++count < total) {
                it = format_to(it, ", ");
            }
        }

        return format_to(it, "}}");
    }
};
