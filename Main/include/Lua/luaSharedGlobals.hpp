// based on https://stackoverflow.com/questions/51335428/sharing-global-variables-between-different-lua-states-through-require

#include <string>
#include <unordered_map>
#include <variant>

#include "lua.hpp"

enum class nil {};
using Variant = std::variant<nil, bool, int, double, std::string>;

class SharedGlobalsLua {
public:
	static int setglobal(lua_State *L) {
		void *p = luaL_checkudata(L, 1, "globals_meta");
		luaL_argcheck(L, p != nullptr, 1, "invalid userdata");

		std::string key = luaL_tolstring(L, 2, nullptr);

		auto &globals = *static_cast<std::unordered_map<std::string, Variant> *>(
			lua_touserdata(L, lua_upvalueindex(1)));
		Variant &v = globals[key];

		switch (lua_type(L, 3)) {
		case LUA_TNIL:
			v = nil{};
			break;
		case LUA_TBOOLEAN:
			v = static_cast<bool>(lua_toboolean(L, 3));
			lua_pop(L, 1);
			break;
		case LUA_TNUMBER:
			if (lua_isinteger(L, 3)) {
				v = static_cast<int>(luaL_checkinteger(L, 3));
			} else {
				v = static_cast<double>(luaL_checknumber(L, 3));
			}
			lua_pop(L, 1);
			break;
		case LUA_TSTRING:
			v = std::string(lua_tostring(L, 3));
			lua_pop(L, 1);
			break;
		default:
			std::string error = "Unsupported global type: ";
			error.append(lua_typename(L, lua_type(L, 3)));
			lua_pushstring(L, error.c_str());
			lua_error(L);
			break;
		}
		return 0;
	}

	static int getglobal(lua_State *L) {
		void *p = luaL_checkudata(L, 1, "globals_meta");
		luaL_argcheck(L, p != nullptr, 1, "invalid userdata");

		std::string key = luaL_tolstring(L, 2, nullptr);

		auto globals = *static_cast<std::unordered_map<std::string, Variant> *>(
			lua_touserdata(L, lua_upvalueindex(1)));
		lua_pop(L, 1);
		auto search = globals.find(key);
		if (search == globals.end()) {
			//lua_pushstring(L, ("unknown global: " + key).c_str());
			//lua_error(L);
			return 0;
		}
		Variant const &v = search->second;

		switch (v.index()) {
		case 0:
			lua_pushnil(L);
			break;
		case 1:
			lua_pushboolean(L, std::get<bool>(v));
			break;
		case 2:
			lua_pushinteger(L, std::get<int>(v));
			break;
		case 3:
			lua_pushnumber(L, std::get<double>(v));
			break;
		case 4:
			lua_pushstring(L, std::get<std::string>(v).c_str());
			break;
		default: // Can't happen
			std::abort();
			break;
		}

		return 1;
	}

	static constexpr struct luaL_Reg globals_meta[] = {
		{"__newindex", setglobal},
		{"__index", getglobal},
		{nullptr, nullptr} // sentinel
	};

	void addLuaState(lua_State* L) {
		luaL_newmetatable(L, "globals_meta");
		lua_pushlightuserdata(L, &globals);
		luaL_setfuncs(L, globals_meta, 1);

		lua_newuserdata(L, 0);
		luaL_getmetatable(L, "globals_meta");
		lua_setmetatable(L, -2);
		lua_setglobal(L, "globals");
	}

	void clearGlobals() {
		globals.clear();
	}

private:
	std::unordered_map<std::string, Variant> globals;
};

//int main() {
//	//globals["num"] = 2;
//
//	// script A
//
//	lua_State *L1 = luaL_newstate();
//	//luaL_openlibs(L1);
//
//	luaL_newmetatable(L1, "globals_meta");
//	lua_pushlightuserdata(L1, &globals);
//	luaL_setfuncs(L1, globals_meta, 1);
//
//	lua_newuserdata(L1, 0);
//	luaL_getmetatable(L1, "globals_meta");
//	lua_setmetatable(L1, -2);
//	lua_setglobal(L1, "globals");
//
//	if (luaL_dostring(L1, "print('Script A: ' .. globals.num)\n"
//						  "globals.num = 5") != 0) {
//		std::cerr << "L1:" << lua_tostring(L1, -1) << '\n';
//		lua_pop(L1, 1);
//	}
//
//	// script B
//
//	lua_State *L2 = luaL_newstate();
//	//luaL_openlibs(L2);
//
//	luaL_newmetatable(L2, "globals_meta");
//	lua_pushlightuserdata(L2, &globals);
//	luaL_setfuncs(L2, globals_meta, 1);
//
//	lua_newuserdata(L2, 0);
//	luaL_getmetatable(L2, "globals_meta");
//	lua_setmetatable(L2, -2);
//	lua_setglobal(L2, "globals");
//
//	//if (luaL_dostring(L2, "print('Script B: ' .. globals.num)") != 0) {
//	//	std::cerr << "L1:" << lua_tostring(L2, -1) << '\n';
//	//	lua_pop(L2, 1);
//	//}
//
//	lua_close(L1);
//	lua_close(L2);
//}
