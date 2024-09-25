#pragma once

#include <lua.h>
#include <luacode.h>
#include <lualib.h>

namespace lr {
namespace detail {
    struct state_deleter {
        void operator()(lua_State *L) const { lua_close(L); }
    };
}  // namespace detail

struct LuaState : public std::unique_ptr<lua_State, detail::state_deleter> {};
}  // namespace lr
