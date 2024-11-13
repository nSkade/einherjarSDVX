//#include "luaCompat53.hpp" //TODO(skade) remove

#include <cmath>

void luaL_requiref(lua_State *L, const char *modname, lua_CFunction openf, int glb) {
    lua_pushcfunction(L, openf);
    lua_pushstring(L, modname);
    lua_call(L, 1, 1);
    
    if (glb) {
        lua_pushvalue(L, -1);
        lua_setglobal(L, modname);
    }
    
    lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
    lua_pushvalue(L, -2);
    lua_setfield(L, -2, modname);
    lua_pop(L, 1);
}

const char *luaL_tolstring (lua_State *L, int idx, size_t *len) {
	return lua_tolstring(L,idx,len);
}

int lua_isinteger(lua_State *L, int index) {
    if (!lua_isnumber(L, index))
        return 0;
    
    double n = lua_tonumber(L, index);
    return (n == std::floor(n));
}

void lua_len(lua_State *L, int index) {
    if (lua_type(L, index) == LUA_TSTRING || lua_type(L, index) == LUA_TTABLE) {
        size_t len = lua_objlen(L, index);
        lua_pushnumber(L, (lua_Number)len);
    } else {
        // Check for __len metamethod
        if (luaL_callmeta(L, index, "__len")) {
            // __len metamethod exists and was called
        } else {
            // No __len metamethod, raise an error
            luaL_error(L, "attempt to get length of a %s value", 
                       lua_typename(L, lua_type(L, index)));
        }
    }
}

lua_Integer luaL_len (lua_State *L, int idx) {
  lua_Integer l;
  int isnum;
  lua_len(L, idx);
  l = lua_tointegerx(L, -1, &isnum);
  if (!isnum)
    luaL_error(L, "object length is not an integer");
  lua_pop(L, 1);  /* remove object */
  return l;
}

void lua_geti(lua_State *L, int index, lua_Integer i) {
    lua_pushinteger(L, i);
    lua_gettable(L, index < 0 ? index - 1 : index);
}

void lua_seti(lua_State *L, int index, lua_Integer i) {
    lua_pushinteger(L, i);
    lua_pushvalue(L, -2);
    lua_settable(L, index < 0 ? index - 2 : index);
    lua_pop(L, 1);
}
