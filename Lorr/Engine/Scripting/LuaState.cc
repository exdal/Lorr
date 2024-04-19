#include "LuaState.hh"

#include <sol/sol.hpp>

namespace lr {
static sol::state lua_state;
bool scripting::init_lua()
{
    lua_state.open_libraries(sol::lib::base, sol::lib::math, sol::lib::io, sol::lib::os, sol::lib::string);
    return true;
}

}  // namespace lr
