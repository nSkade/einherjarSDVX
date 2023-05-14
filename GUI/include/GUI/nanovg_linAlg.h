#pragma once
#include "nanovg.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "Shared/Transform.hpp"

/*
* Lua Bindings for Vector and Matrix classes.
*/

//TODO(skade) easier Bindings possible? See ShadedMesh.

//TODO(skade) put on a better place
static void dumpstack (lua_State *L) {
	int top=lua_gettop(L);
	for (int i=1; i <= top; i++) {
		printf("%d\t%s\t", i, luaL_typename(L,i));
		switch (lua_type(L, i)) {
		case LUA_TNUMBER:
			printf("%g\n",lua_tonumber(L,i));
			break;
		case LUA_TSTRING:
			printf("%s\n",lua_tostring(L,i));
			break;
		case LUA_TBOOLEAN:
			printf("%s\n", (lua_toboolean(L, i) ? "true" : "false"));
			break;
		case LUA_TNIL:
			printf("%s\n", "nil");
			break;
		default:
			printf("%p\n",lua_topointer(L,i));
			break;
		}
	}
}

static Transform readMat4(lua_State* L, int idx) {
	Transform t;
	for (uint32_t i=0;i<16;++i) {
		lua_rawgeti(L,idx,i+1);
		t[i*4%16+i/4] = luaL_checknumber(L,-1); // Convert row wise to column wise.
		lua_pop(L,1);
	}
	return t;
}

static Vector4 readVec4(lua_State* L, int idx) {
	Vector4 v(0.0);
	for (uint32_t i = 1; i <= 4; ++i) {
		lua_rawgeti(L,idx,i);
		v[i-1] = luaL_checknumber(L,-1);
		lua_pop(L,1);
	}
	return v;
}

static Vector3 readVec3(lua_State* L, int idx) {
	Vector3 v(0.0);
	for (uint32_t i = 1; i <= 3; ++i) {
		lua_rawgeti(L,idx,i);
		v[i-1] = luaL_checknumber(L,-1);
		lua_pop(L,1);
	}
	return v;
}

static void writeMat4(lua_State* L, const Transform& t) {
	lua_createtable(L,16,0);
	//lua_newtable(L);
	for (uint32_t i=0;i<16;++i) {
		lua_pushnumber(L, t[i*4%16+i/4]); // Convert index from column to row wise.
		lua_rawseti(L,-2,i+1);
	}
}

static void writeVec4(lua_State* L, const Vector4& v) {
	lua_createtable(L,4,0);
	//lua_newtable(L);
	for (uint32_t i=0;i<4;++i) {
		lua_pushnumber(L, v[i]);
		lua_rawseti(L,-2,i+1);
	}
}

static void writeVec3(lua_State* L, const Vector3& v) {
	lua_createtable(L,3,0);
	//lua_newtable(L);
	for (uint32_t i=0;i<3;++i) {
		lua_pushnumber(L, v[i]);
		lua_rawseti(L,-2,i+1);
	}
}

//TODO rename to multMat
static int lmultMat(lua_State* L) {
	int n = lua_gettop(L); // number of arguments
	Transform r;
	for (uint32_t i = 1; i <= n; i++) {
		if (!lua_istable(L, i)) {
			lua_pushstring(L, "incorrect argument");
			lua_error(L);
		}
		Transform t = readMat4(L,i);
		r = r*t;
	}
	writeMat4(L,r);
	return 1;
}

static int lmultMatVec(lua_State* L) {
	Transform t1 = readMat4(L,1);
	Vector4 t2 = readVec4(L,2);
	
	Vector4 res = t1*t2;
	writeVec4(L,res);
	return 1;
}

static int lgetRotMat(lua_State* L) {
	Vector3 r = readVec3(L,1);
	Transform t = Transform::Rotation(r);
	writeMat4(L,t);
	return 1;
}

static int lgetTransMat(lua_State* L) {
	Vector3 vt = readVec3(L,1);
	Transform t = Transform::Translation(vt);
	writeMat4(L,t);
	return 1;
}

static int lgetScaleMat(lua_State* L) {
	Vector3 vt = readVec3(L,1);
	Transform t = Transform::Scale(vt);
	writeMat4(L,t);
	return 1;
}

static int lgetInverse(lua_State* L) {
	Transform t = readMat4(L,1);
	t = Transform::Inverse(t);
	writeMat4(L,t);
	return 1;
}

