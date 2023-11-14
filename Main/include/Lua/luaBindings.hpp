#pragma once
#include "nanovg.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

//TODO(skade) move into cpp
//static int lGetMousePos(lua_State *L);
//static int lGetResolution(lua_State *L);
//static int lGetLaserColor(lua_State *L /*int laser*/);
//static int lLog(lua_State *L);
//static int lPrint(lua_State *L);
//static int lGetButton(lua_State *L /* int button */);
//static int lGetKnob(lua_State *L /* int knob */);
//static int lgetupdateavailable(lua_state *l);
//static int lcreateskinimage(lua_state *l /*const char* filename, int imageflags */);
//static int lLoadSkinAnimation(lua_State *L);
//static int lLoadSkinFont(lua_State *L /*const char* name */);
//static int lLoadSkinSample(lua_State *L /*char* name */);
//static int lIsSamplePlaying(lua_State *L /* char* name */);
//static int lStopSample(lua_State *L /* char* name */);
//static int lPathAbsolute(lua_State *L /* string path */);
//static int lForceRender(lua_State *L);
//static int lLoadImageJob(lua_State *L /* char* path, int placeholder, int w = 0, int h = 0 */);
//static int lLoadWebImageJob(lua_State *L /* char* url, int placeholder, int w = 0, int h = 0 */);
//static int lWarnGauge(lua_State *L);
//static int lGetSkin(lua_State *L);
//static int lSetSkinSetting(lua_State *L /*String key, Any value*/);
//static int lGetSkinSetting(lua_State *L /*String key*/);
//int lLoadSharedTexture(lua_State* L);
//int lLoadSharedSkinTexture(lua_State* L);
//int lGetSharedTexture(lua_State* L) {;

static int lGetMousePos(lua_State *L)
{
	Vector2i pos = g_gameWindow->GetMousePos();
	float left = g_gameWindow->GetWindowSize().x / 2 - g_resolution.x / 2;
	lua_pushnumber(L, pos.x - left);
	lua_pushnumber(L, pos.y);
	return 2;
}

static int lGetResolution(lua_State *L)
{
	lua_pushnumber(L, g_resolution.x);
	lua_pushnumber(L, g_resolution.y);
	return 2;
}

static int lGetLaserColor(lua_State *L /*int laser*/)
{
	int laser = luaL_checkinteger(L, 1);
	float laserHues[2] = {0.f};
	laserHues[0] = g_gameConfig.GetFloat(GameConfigKeys::Laser0Color);
	laserHues[1] = g_gameConfig.GetFloat(GameConfigKeys::Laser1Color);
	Colori c = Color::FromHSV(laserHues[laser], 1.0, 1.0).ToRGBA8();
	lua_pushnumber(L, c.x);
	lua_pushnumber(L, c.y);
	lua_pushnumber(L, c.z);
	return 3;
}

static int lLog(lua_State *L)
{
	String msg = luaL_checkstring(L, 1);
	int severity = luaL_checkinteger(L, 2);
	Log(msg, (Logger::Severity)severity);
	return 0;
}

static int lPrint(lua_State *L)
{
	String msg = luaL_checkstring(L, 1);
	printf(msg.c_str());
	printf("\n");
	return 0;
}

static int lGetButton(lua_State *L /* int button */)
{
	int button = luaL_checkinteger(L, 1);
	if (g_application->autoplayInfo
		&& (g_application->autoplayInfo->IsAutoplayButtons() || g_application->autoplayInfo->IsReplayingButtons())
		&& button < 6)
		lua_pushboolean(L, g_application->autoplayInfo->buttonAnimationTimer[button] > 0);
	else
		lua_pushboolean(L, g_input.GetButton((Input::Button)button));
	return 1;
}

static int lGetKnob(lua_State *L /* int knob */)
{
	int knob = luaL_checkinteger(L, 1);
	lua_pushnumber(L, g_input.GetAbsoluteLaser(knob));
	return 1;
}

static int lGetUpdateAvailable(lua_State *L)
{
	Vector<String> info = g_application->GetUpdateAvailable();
	if (info.empty())
	{
		return 0;
	}

	lua_pushstring(L, *info[0]);
	lua_pushstring(L, *info[1]);
	return 2;
}

static int lCreateSkinImage(lua_State *L /*const char* filename, int imageflags */)
{
	const char *filename = luaL_checkstring(L, 1);
	int imageflags = luaL_checkinteger(L, 2);
	String path = "skins/" + g_application->GetCurrentSkin() + "/textures/" + filename;
	path = Path::Absolute(path);
	int handle = nvgCreateImage(g_guiState.vg, path.c_str(), imageflags);
	if (handle != 0)
	{
		g_guiState.vgImages[L].Add(handle);
		lua_pushnumber(L, handle);
		return 1;
	}
	return 0;
}

static int lLoadSkinAnimation(lua_State *L)
{
	const char *p;
	float frametime;
	int loopcount = 0;
	bool compressed = false;

	p = luaL_checkstring(L, 1);
	frametime = luaL_checknumber(L, 2);
	if (lua_gettop(L) == 3)
	{
		loopcount = luaL_checkinteger(L, 3);
	}
	else if (lua_gettop(L) == 4)
	{
		loopcount = luaL_checkinteger(L, 3);
		compressed = lua_toboolean(L, 4) == 1;
	}

	String path = "skins/" + g_application->GetCurrentSkin() + "/textures/" + p;
	path = Path::Absolute(path);
	int result = LoadAnimation(L, *path, frametime, loopcount, compressed);
	if (result == -1)
		return 0;

	lua_pushnumber(L, result);
	return 1;
}

static int lLoadSkinFont(lua_State *L /*const char* name */)
{
	const char *name = luaL_checkstring(L, 1);
	String path = "skins/" + g_application->GetCurrentSkin() + "/fonts/" + name;
	path = Path::Absolute(path);
	return LoadFont(name, path.c_str(), L);
}

static int lLoadSkinSample(lua_State *L /*char* name */)
{
	const char *name = luaL_checkstring(L, 1);
	Sample newSample = g_application->LoadSample(name);
	if (!newSample)
	{
		lua_Debug ar;
		lua_getstack(L, 1, &ar);
		lua_getinfo(L, "Snl", &ar);
		String luaFilename;
		Path::RemoveLast(ar.source, &luaFilename);
		lua_pushstring(L, *Utility::Sprintf("Failed to load sample \"%s\" at line %d in \"%s\"", name, ar.currentline, luaFilename));
		return lua_error(L);
	}
	g_application->StoreNamedSample(name, newSample);
	return 0;
}

static int lLoadSample(lua_State *L /*char* name */)
{
	const char *name = luaL_checkstring(L, 1);
	Sample newSample = g_application->LoadSample(name,true);
	if (!newSample)
	{
		lua_Debug ar;
		lua_getstack(L, 1, &ar);
		lua_getinfo(L, "Snl", &ar);
		String luaFilename;
		Path::RemoveLast(ar.source, &luaFilename);
		lua_pushstring(L, *Utility::Sprintf("Failed to load sample \"%s\" at line %d in \"%s\"", name, ar.currentline, luaFilename));
		return lua_error(L);
	}
	g_application->StoreNamedSample(name, newSample);
	return 0;
}

static int lPlaySample(lua_State *L /*char* name, bool loop */)
{
	const char *name = luaL_checkstring(L, 1);
	bool loop = false;
	if (lua_gettop(L) == 2)
	{
		loop = lua_toboolean(L, 2) == 1;
	}

	g_application->PlayNamedSample(name, loop);
	return 0;
}

static int lIsSamplePlaying(lua_State *L /* char* name */)
{
	const char *name = luaL_checkstring(L, 1);
	int res = g_application->IsNamedSamplePlaying(name);
	if (res == -1)
		return 0;

	lua_pushboolean(L, res);
	return 1;
}

static int lStopSample(lua_State *L /* char* name */)
{
	const char *name = luaL_checkstring(L, 1);
	g_application->StopNamedSample(name);
	return 0;
}

static int lPathAbsolute(lua_State *L /* string path */)
{
	const char *path = luaL_checkstring(L, 1);
	lua_pushstring(L, *Path::Absolute(path));
	return 1;
}

static int lForceRender(lua_State *L)
{
	g_application->ForceRender();
	return 0;
}

static int lLoadImageJob(lua_State *L /* char* path, int placeholder, int w = 0, int h = 0 */)
{
	const char *path = luaL_checkstring(L, 1);
	int fallback = luaL_checkinteger(L, 2);
	int w = 0, h = 0;
	if (lua_gettop(L) == 4)
	{
		w = luaL_checkinteger(L, 3);
		h = luaL_checkinteger(L, 4);
	}
	lua_pushinteger(L, g_application->LoadImageJob(path, {w, h}, fallback));
	return 1;
}

static int lLoadWebImageJob(lua_State *L /* char* url, int placeholder, int w = 0, int h = 0 */)
{
	const char *url = luaL_checkstring(L, 1);
	int fallback = luaL_checkinteger(L, 2);
	int w = 0, h = 0;
	if (lua_gettop(L) == 4)
	{
		w = luaL_checkinteger(L, 3);
		h = luaL_checkinteger(L, 4);
	}
	lua_pushinteger(L, g_application->LoadImageJob(url, {w, h}, fallback, true));
	return 1;
}

static int lWarnGauge(lua_State *L)
{
	g_application->WarnGauge();
	return 0;
}

static int lGetSkin(lua_State *L)
{
	lua_pushstring(L, *g_application->GetCurrentSkin());
	return 1;
}

static int lSetSkinSetting(lua_State *L /*String key, Any value*/)
{
	String key = luaL_checkstring(L, 1);
	IConfigEntry *entry = g_skinConfig->GetEntry(key);
	if (!entry) //just set depending on value type
	{
		if (lua_isboolean(L, 2))
		{
			bool value = luaL_checknumber(L, 2) == 1;
			g_skinConfig->Set(key, value);
		}
		else if (lua_isnumber(L, 2)) //no good way to know if int or not
		{
			float value = luaL_checknumber(L, 2);
			g_skinConfig->Set(key, value);
		}
		else if (lua_isstring(L, 2))
		{
			String value = luaL_checkstring(L, 2);
			g_skinConfig->Set(key, value);
		}
	}
	else
	{
		if (entry->GetType() == IConfigEntry::EntryType::Boolean)
		{
			bool value = luaL_checknumber(L, 2) == 1;
			g_skinConfig->Set(key, value);
		}
		else if (entry->GetType() == IConfigEntry::EntryType::Float)
		{
			float value = luaL_checknumber(L, 2);
			g_skinConfig->Set(key, value);
		}
		else if (entry->GetType() == IConfigEntry::EntryType::Integer)
		{
			int value = luaL_checkinteger(L, 2);
			g_skinConfig->Set(key, value);
		}
		else if (entry->GetType() == IConfigEntry::EntryType::String)
		{
			String value = luaL_checkstring(L, 2);
			g_skinConfig->Set(key, value);
		}
	}
	return 0;
}

static int lLoadFile(lua_State* L) {
	
	std::string path = luaL_checkstring(L,1);
	
	std::string content;
	File f;
	f.OpenRead(path);
	content.resize(f.GetSize());
	f.Read(content.data(),f.GetSize());
	f.Close();

	lua_pushlstring(L,content.data(),content.length());
	
	return 1;
}

static int lGetSkinSetting(lua_State *L /*String key*/)
{
	String key = luaL_checkstring(L, 1);
	IConfigEntry *entry = g_skinConfig->GetEntry(key);
	if (!entry)
	{
		return 0;
	}

	if (entry->GetType() == IConfigEntry::EntryType::Boolean)
	{
		lua_pushboolean(L, entry->As<BoolConfigEntry>()->data);
		return 1;
	}
	else if (entry->GetType() == IConfigEntry::EntryType::Float)
	{
		lua_pushnumber(L, entry->As<FloatConfigEntry>()->data);
		return 1;
	}
	else if (entry->GetType() == IConfigEntry::EntryType::Integer)
	{
		lua_pushnumber(L, entry->As<IntConfigEntry>()->data);
		return 1;
	}
	else if (entry->GetType() == IConfigEntry::EntryType::String)
	{
		lua_pushstring(L, entry->As<StringConfigEntry>()->data.c_str());
		return 1;
	}
	else if (entry->GetType() == IConfigEntry::EntryType::Color)
	{
		Colori data = entry->As<ColorConfigEntry>()->data.ToRGBA8();
		lua_pushnumber(L, data.x);
		lua_pushnumber(L, data.y);
		lua_pushnumber(L, data.z);
		lua_pushnumber(L, data.w);
		return 4;
	}
	else
	{
		return 0;
	}
}

int lCreatefbTexture(lua_State* L) {
	const auto key = luaL_checkstring(L, 1);
	int32_t width = luaL_checknumber(L,2);
	int32_t height = luaL_checknumber(L,3);

	Texture t = TextureRes::CreateFromFrameBuffer(g_gl,{width,height});
	g_application->fbTextures.Add(key,t);
	return 0;
}

int lSetfbTexture(lua_State* L) {
	const auto key = luaL_checkstring(L, 1);
	int32_t x = luaL_checknumber(L,2);
	int32_t y = luaL_checknumber(L,3);
	g_application->fbTextures.at(key).get()->SetFromFrameBuffer({x,y});
	return 0;
}

int lCreatefbTextureSkin(lua_State* L) {
	const auto key = luaL_checkstring(L, 1);
	int32_t width = luaL_checknumber(L,2);
	int32_t height = luaL_checknumber(L,3);

	Texture t = TextureRes::CreateFromFrameBuffer(g_gl,{width,height});
	g_application->fbTexturesSkin.Add(key,t);
	return 0;
}

int lSetfbTextureSkin(lua_State* L) {
	const auto key = luaL_checkstring(L, 1);
	int32_t x = luaL_checknumber(L,2);
	int32_t y = luaL_checknumber(L,3);
	g_application->fbTexturesSkin.at(key).get()->SetFromFrameBuffer({x,y});
	return 0;
}

//TODO(skade) needed?
//int lClearfbTexture(lua_State* L) {
//	const auto key = luaL_checkstring(L, 1);
//	g_application->fbTextures.at(key).get()->SetData();
//	return 0;
//}

int lLoadSharedTexture(lua_State* L) {
	Ref<SharedTexture> newTexture = Utility::MakeRef(new SharedTexture());


	const auto key = luaL_checkstring(L, 1);
	const auto path = luaL_checkstring(L, 2);
	int imageflags = 0;
	if (lua_isinteger(L, 3)) {
		imageflags = luaL_checkinteger(L, 3);
	}

	newTexture->nvgTexture = nvgCreateImage(g_guiState.vg, path, imageflags);
	newTexture->texture = g_application->LoadTexture(path, true);

	if (newTexture->Valid())
	{
		g_application->sharedTextures.Add(key, newTexture);
	}
	else {
		lua_pushstring(L, *Utility::Sprintf("Failed to load shared texture with path: '%s', key: '%s'", path, key));
		return lua_error(L);
	}
	
	return 0;
}

int lLoadSharedSkinTexture(lua_State* L) {
	Ref<SharedTexture> newTexture = Utility::MakeRef(new SharedTexture());
	const auto key = luaL_checkstring(L, 1);
	const auto filename = luaL_checkstring(L, 2);
	int imageflags = 0;
	if (lua_isinteger(L, 3)) {
		imageflags = luaL_checkinteger(L, 3);
	}


	String path = "skins/" + g_application->GetCurrentSkin() + "/textures/" + filename;
	path = Path::Absolute(path);

	newTexture->nvgTexture = nvgCreateImage(g_guiState.vg, path.c_str(), imageflags);
	newTexture->texture = g_application->LoadTexture(filename, false);

	if (newTexture->Valid())
	{
		g_application->sharedTextures.Add(key, newTexture);
	}
	else {
		return luaL_error(L, "Failed to load shared texture with path: '%s', key: '%s'", *path, *key);
	}

	return 0;
}

int lGetSharedTexture(lua_State* L) {
	const auto key = luaL_checkstring(L, 1);

	if (g_application->sharedTextures.Contains(key))
	{
		auto& t = g_application->sharedTextures.at(key);
		lua_pushnumber(L, t->nvgTexture);
		return 1;
	}

	
	return 0;
}
