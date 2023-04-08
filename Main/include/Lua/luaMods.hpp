#pragma once
#include "nanovg.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "GUI/nanovg_ehj.h"

static int lehjGScale(lua_State *L)
{
	float scale = luaL_checknumber(L, 1);
	
	g_scale = scale;

	return 1;
}
