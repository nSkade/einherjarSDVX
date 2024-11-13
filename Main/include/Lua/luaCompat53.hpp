#pragma once
#include "lua.hpp"

void luaL_requiref(lua_State *L, const char *modname, lua_CFunction openf, int glb);

const char *luaL_tolstring (lua_State *L, int idx, size_t *len);

int lua_isinteger(lua_State *L, int index);

void lua_len(lua_State *L, int index);

lua_Integer luaL_len (lua_State *L, int idx);

void lua_geti(lua_State *L, int index, lua_Integer i);

void lua_seti(lua_State *L, int index, lua_Integer i);
