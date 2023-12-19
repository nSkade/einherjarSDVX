#include "stdafx.h"
#include "Background.hpp"
#include "Application.hpp"
#include "GameConfig.hpp"
#include "Background.hpp"
#include "ShadedMesh.hpp"
#include "Game.hpp"
#include "Track.hpp"
#include "Camera.hpp"
#include "lua.hpp"
#include "Gauge.hpp"
#include "Shared/LuaBindable.hpp"

#include "GUI/nanovg_linAlg.h"

/* Background template for fullscreen effects */
class FullscreenBackground : public Background
{
public:
	~FullscreenBackground()
	{
		if (lua) {
			// check for cleanup function and call on exit
			lua_settop(lua, 0);
			lua_getglobal(lua, "cleanup");
			if (lua_isfunction(lua, -1))
			{
				if (lua_pcall(lua, 0, 0, 0) != 0)
				{
					//TODO(skade) check lua_tostring nullptr before constructing String
					Logf("Lua error: %s", Logger::Severity::Error, lua_tostring(lua, -1));
					g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(lua, -1), 0);
					errored = true;
				}
			}
			lua_settop(lua, 0);
		}
		if (bindable)
		{
			delete bindable;
			bindable = nullptr;
		}
		if (trackBindable)
		{
			delete trackBindable;
			trackBindable = nullptr;
		}
		//TODO(skade) delete modsBindable?
		if (lua)
		{
			g_application->DisposeLua(lua);
			lua = nullptr;
		}
	}

	virtual bool Init(bool foreground) override
	{
		fullscreenMesh = MeshGenerators::Quad(g_gl, Vector2(-1.0f), Vector2(2.0f));
		this->foreground = foreground;
		return true;
	}
	void UpdateRenderState(float deltaTime)
	{
		renderState = g_application->GetRenderStateBase();
	}
	virtual void Render(float deltaTime, int fgl) override
	{
		assert(fullscreenMaterial);

		// Render a fullscreen quad
		RenderQueue rq(g_gl, renderState);
		rq.Draw(Transform(), fullscreenMesh, fullscreenMaterial, fullscreenMaterialParams);
		rq.Process();
	}

	bool hasFG() {
		return this->hasFGbind;
	}

	bool hasFFG() {
		return this->hasFFGbind;
	}

protected:
	RenderState renderState;
	Mesh fullscreenMesh;
	Material fullscreenMaterial;
	Map<String, Texture> textures;
	Texture frameBufferTexture = nullptr;
	MaterialParameterSet fullscreenMaterialParams;
	float clearTransition = 0.0f;
	float offsyncTimer = 0.0f;
	float speedMult = 1.0f;
	bool hasFGbind = false;
	bool hasFFGbind = false;
	bool foreground = false;
	bool errored = false;
	Vector<String> defaultBGs;
	LuaBindable *bindable = nullptr;
	LuaBindable *trackBindable = nullptr;
	LuaBindable *modsBindable = nullptr;
	String folderPath;
	lua_State *lua = nullptr;
	Vector3 timing;
	Vector2 tilt;
};

class TestBackground : public FullscreenBackground
{
private:
	bool m_init(String path)
	{
		if (luaL_dofile(lua, Path::Normalize(path + ".lua").c_str()))
		{
			Logf("Lua error: %s", Logger::Severity::Warning, lua_tostring(lua, -1));
			return false;
		}
		String matPath = path + ".fs";

		CheckedLoad(fullscreenMaterial = LoadBackgroundMaterial(matPath));
		fullscreenMaterial->opaque = false;

		if (fullscreenMaterial->HasUniform("texFrameBuffer"))
			frameBufferTexture = TextureRes::CreateFromFrameBuffer(g_gl, g_resolution);
		else
			frameBufferTexture = nullptr;

		return true;
	}

public:
	virtual bool Init(bool foreground) override
	{
		if (!FullscreenBackground::Init(foreground))
			return false;

		defaultBGs = Path::GetSubDirs(Path::Normalize(
			Path::Absolute("skins/" + g_application->GetCurrentSkin() + "/backgrounds/")));

		String skin = g_gameConfig.GetString(GameConfigKeys::Skin);

		String matPath = "";
		String fname = foreground ? "fg" : "bg";
		String kshLayer = game->GetBeatmap()->GetMapSettings().foregroundPath;
		String layer;

		if (!kshLayer.Split(";", &layer, nullptr))
		{
			layer = kshLayer;
		}
		
		//TODO(skade) try to load chart BG first.
		if (defaultBGs.Contains(layer))
		{
			//default bg: load from skin path
			folderPath = "skins/" +
						 g_application->GetCurrentSkin() + Path::sep +
						 "backgrounds" + Path::sep +
						 layer +
						 Path::sep;
			folderPath = Path::Absolute(folderPath);
		}
		else
		{
			//if skin doesn't have it, try loading from chart folder
			folderPath = game->GetChartRootPath() + Path::sep +
						 layer +
						 Path::sep;
			folderPath = Path::Absolute(folderPath);
		}

		String path = Path::Normalize(folderPath + fname);

		lua = luaL_newstate();
		luaL_openlibs(lua);

		//void Application::SetScriptPath(lua_State *s)
		{
			//Set path for 'require' (https://stackoverflow.com/questions/4125971/setting-the-global-lua-path-variable-from-c-c?lq=1)
			String lua_path = Path::Normalize(
				Path::Absolute(folderPath + "?.lua;") +
				Path::Absolute(folderPath + "?"));

			lua_getglobal(lua, "package");
			lua_getfield(lua, -1, "path");				// get field "path" from table at top of stack (-1)
			std::string cur_path = lua_tostring(lua, -1); // grab path string from top of stack
			cur_path.append(";");						// do your path magic here
			cur_path.append(lua_path.c_str());
			lua_pop(lua, 1);						 // get rid of the string on the stack e just pushed on line 5
			lua_pushstring(lua, cur_path.c_str()); // push the new one
			lua_setfield(lua, -2, "path");		 // set the field "path" in table at -2 with value at top of stack
			lua_pop(lua, 1);						 // get rid of package table from top of stack
		}

		auto openLib = [this](const char *name, lua_CFunction lib) {
			luaL_requiref(lua, name, lib, 1);
			lua_pop(lua, 1);
		};

		auto errorOnLib = [this](const char *name) {
			luaL_dostring(lua, (String(name) + " = {}; setmetatable(" + String(name) + ", {__index = function() error(\"Song background cannot access the '" + name + "' library\") end})").c_str());
		};

		//open libs
		//TODO: not sure which should be included
		openLib("_G", luaopen_base);
		openLib(LUA_LOADLIBNAME, luaopen_package);
		openLib(LUA_TABLIBNAME, luaopen_table);
		openLib(LUA_STRLIBNAME, luaopen_string);
		openLib(LUA_MATHLIBNAME, luaopen_math);
		openLib(LUA_DBLIBNAME, luaopen_math); //TODO(skade) only open on debug command argument

		// Add error messages to libs which are not allowed
		errorOnLib(LUA_COLIBNAME);
		errorOnLib(LUA_IOLIBNAME);
		errorOnLib(LUA_OSLIBNAME);
		errorOnLib(LUA_UTF8LIBNAME);
		//errorOnLib(LUA_DBLIBNAME);

		// Clean up the 'package' library so we can't load dlls
		lua_getglobal(lua, "package");

		// Remove C searchers so we can't load dlls
		lua_getfield(lua, -1, "searchers"); // Get the searcher list (-1)
		lua_pushnil(lua);
		lua_rawseti(lua, -2, 4); // C root
		lua_pushnil(lua);
		lua_rawseti(lua, -2, 3); // C path
		lua_pop(lua, 1);		 /* remove searchers */

		// Remove loadlib so we can't load dlls
		lua_pushnil(lua);
		lua_setfield(lua, -2, "loadlib");

		// Remove cpath so we won't try and load anything from it
		lua_pushstring(lua, "");
		lua_setfield(lua, -2, "cpath");

		lua_pop(lua, 1); /* remove package */

		g_application->SetLuaBindings(lua);
		game->SetInitialGameplayLua(lua);
		game->SetInitialModsLua(lua);
		// We have to do this seperately bc package is already defined
		luaL_dostring(lua, "setmetatable(package, {__index = function() error(\"Song background cannot access the 'package' library\") end})");

		String bindName = foreground ? "foreground" : "background";

		bindable = new LuaBindable(lua, bindName);
		bindable->AddFunction("LoadTexture", this, &TestBackground::LoadTexture);
		bindable->AddFunction("SetParami", this, &TestBackground::SetParami);
		bindable->AddFunction("SetParamf", this, &TestBackground::SetParamf);
		bindable->AddFunction("SetParam2f", this, &TestBackground::SetParam2f);
		bindable->AddFunction("SetParam3f", this, &TestBackground::SetParam3f);
		bindable->AddFunction("SetParam4f", this, &TestBackground::SetParam4f);
		bindable->AddFunction("SetParam4x4f", this, &TestBackground::SetParam4x4f);
		bindable->AddFunction("DrawShader", this, &TestBackground::DrawShader);
		bindable->AddFunction("GetPath", this, &TestBackground::GetPath);
		bindable->AddFunction("SetSpeedMult", this, &TestBackground::SetSpeedMult);
		bindable->AddFunction("GetTiming", this, &TestBackground::GetTiming);
		bindable->AddFunction("GetBeat", this, &TestBackground::GetBeat); //TODO(skade) Binding also for skin?
		bindable->AddFunction("GetTime", this, &TestBackground::GetTime); //TODO(skade) Binding also for skin?
		bindable->AddFunction("GetBarTime", this, &TestBackground::GetBarTime);
		bindable->AddFunction("GetTimeByBeat", this, &TestBackground::GetTimeFromBeat);
		bindable->AddFunction("GetBeatByTime", this, &TestBackground::GetBeatFromTime);
		bindable->AddFunction("GetViewRange", this, &TestBackground::GetViewRange);
		bindable->AddFunction("GetViewRangeMeasure", this, &TestBackground::GetViewRangeMeasure);
		bindable->AddFunction("GetTilt", this, &TestBackground::GetTilt);
		bindable->AddFunction("GetScreenCenter", this, &TestBackground::GetScreenCenter);
		bindable->AddFunction("GetClearTransition", this, &TestBackground::GetClearTransition);

		bindable->AddFunction("SetMaterialDepthTest", this, &TestBackground::SetMaterialDepthTest);
		bindable->AddFunction("SetMaterialOpaque", this, &TestBackground::SetMaterialOpaque);
		bindable->AddFunction("SetMaterialBlendMode", this, &TestBackground::SetMaterialBlendMode);

		bindable->Push();
		lua_settop(lua, 0);

		trackBindable = game->MakeTrackLuaBindable(lua);
		trackBindable->Push();
		modsBindable = game->MakeModsLuaBindable(lua);
		modsBindable->Push();

		game->m_sharedGlobalsLua.addLuaState(lua);
		
		bool suc = m_init(path);
		
		if (game->IsPracticeMode() && g_gameConfig.GetBool(GameConfigKeys::SkinDevMode)) {
			while (!suc) {
				String luaMsg;
				//TODO(skade) nullptr happens on frag error (non lua), somehow fetch shader error msg?
				if (lua_tostring(lua,-1))
					luaMsg = String(lua_tostring(lua,-1));
				else
					luaMsg = "Non lua error, probably shader related. Check Log or enable console\n";

				g_gameWindow->ShowMessageBox("Loading BGA error\n", luaMsg, 0);
				bool reload = g_gameWindow->ShowYesNoMessage("Loading BGA error\n", "Reload BG?\n");
				if (!reload)
					break;
				suc = m_init(path);
			}
		}

		lua_settop(lua, 0);
		// Look for render_fg in bg and vice versa
		lua_getglobal(lua, "render_fg");
		if (lua_isfunction(lua, -1))
			hasFGbind = true;
		lua_settop(lua, 0);
		lua_getglobal(lua, "render_ffg");
		if (lua_isfunction(lua, -1))
			hasFFGbind = true;
		lua_settop(lua, 0);
		
		if (suc)
			return true;

		Logf("Failed to load %s at path: \"%s\" Attempting to load fallback instead.", Logger::Severity::Warning, foreground ? "foreground" : "background", folderPath);
		path = Path::Absolute("skins/" + skin + "/backgrounds/fallback/");
		folderPath = path;
		path = Path::Normalize(path + fname);
		return m_init(path);
	}
	virtual void Render(float deltaTime, int fgl) override
	{
		if (errored && game->IsPracticeMode()) {
			this->Init(this->foreground);
			this->errored = false;
		} else if (errored)
			return;
		UpdateRenderState(deltaTime);
		game->SetGameplayLua(lua);
		game->SetModsLua(lua);
		const TimingPoint &tp = game->GetPlayback().GetCurrentTimingPoint();
		timing.x = game->GetPlayback().GetBeatTime();
		timing.z = game->GetPlayback().GetLastTime() / 1000.0f;
		offsyncTimer += (speedMult * deltaTime / tp.beatDuration) * 1000.0 * game->GetPlaybackSpeed();
		offsyncTimer = fmodf(offsyncTimer, 1.0f);
		timing.y = offsyncTimer;

		float clearBorder = 0.70f;
		if (game->GetScoring().GetTopGauge()->GetType() != GaugeType::Normal)
		{
			clearBorder = 0.30f;
		}

		bool cleared = game->GetScoring().GetTopGauge()->GetValue() >= clearBorder;

		if (cleared)
			clearTransition += deltaTime / tp.beatDuration * 1000;
		else
			clearTransition -= deltaTime / tp.beatDuration * 1000;

		clearTransition = Math::Clamp(clearTransition, 0.0f, 1.0f);

		Vector2i screenCenter = game->GetCamera().GetScreenCenter();

		tilt = {game->GetCamera().GetActualRoll(), game->GetCamera().GetBackgroundSpin()};
		fullscreenMaterialParams.SetParameter("clearTransition", clearTransition);
		fullscreenMaterialParams.SetParameter("tilt", tilt);
		fullscreenMaterialParams.SetParameter("screenCenter", screenCenter);
		fullscreenMaterialParams.SetParameter("timing", timing);
		if (foreground && frameBufferTexture)
		{
			frameBufferTexture->SetFromFrameBuffer();
			fullscreenMaterialParams.SetParameter("texFrameBuffer", frameBufferTexture);
		}

		//TODO(skade) cleanup
		if (hasFGbind && fgl==1)
			lua_getglobal(lua, "render_fg");
		else if (!foreground && fgl==0)
			lua_getglobal(lua, "render_bg");
		else if (hasFFGbind && fgl==2)
			lua_getglobal(lua, "render_ffg");
		else {
			// Requested BG layer func not found.
			lua_settop(lua, 0);
			return;
		}

		if (lua_isfunction(lua, -1))
		{
			lua_pushnumber(lua, deltaTime); //TODO(skade) push table where bg and skin read and write data
			if (lua_pcall(lua, 1, 0, 0) != 0)
			{
				//TODO(skade) check lua_tostring nullptr before constructing String
				Logf("Lua error: %s", Logger::Severity::Error, lua_tostring(lua, -1));
				g_gameWindow->ShowMessageBox("Lua Error", lua_tostring(lua, -1), 0);
				errored = true;
			}
		}
		lua_settop(lua, 0);
		g_application->ForceRender();
	}

	int LoadTexture(lua_State *L /*String uniformName, String filename*/)
	{
		String uniformName(luaL_checkstring(L, 2));
		String filename(luaL_checkstring(L, 3));
		filename = Path::Normalize(folderPath + Path::sep + filename);
		auto texture = g_application->LoadTexture(filename, true);
		if (texture)
		{
			textures.Add(uniformName, texture);
		}
		else
		{
			Logf("Failed to load texture at: %s", Logger::Severity::Warning, filename);
		}
		return 0;
	}

	int GetTiming(lua_State *L)
	{
		lua_pushnumber(L, timing.x);
		lua_pushnumber(L, timing.y);
		lua_pushnumber(L, timing.z);
		return 3;
	}

	int GetTime(lua_State *L)
	{
		lua_pushnumber(L,timing.z);
		return 1;
	}

	int GetBeat(lua_State* L)
	{
		lua_pushnumber(L, game->GetPlayback().GetCurrBeat());
		return 1;
	}

	int GetBarTime(lua_State* L)
	{
		lua_pushnumber(L,game->GetPlayback().GetBarTime());
		return 1;
	}

	int GetTimeFromBeat(lua_State* L)
	{
		int measure = luaL_checknumber(L,2);
		lua_pushnumber(L,game->GetPlayback().GetTimeByBeat(measure)*0.001);
		return 1;
	}

	int GetBeatFromTime(lua_State* L)
	{
		float time = luaL_checknumber(L,2)*1000.f;
		lua_pushnumber(L,game->GetPlayback().GetBeatByTime(time));
		return 1;
	}

	int GetViewRangeMeasure(lua_State* L)
	{
		lua_pushnumber(L,game->GetTrack().GetViewRange()/4.f);
		return 1;
	}

	int GetViewRange(lua_State* L)
	{
		lua_pushnumber(L,game->GetTrack().GetViewRange());
		return 1;
	}

	int GetTilt(lua_State *L)
	{
		lua_pushnumber(L, tilt.x);
		lua_pushnumber(L, tilt.y);
		return 2;
	}

	int GetScreenCenter(lua_State *L)
	{
		auto c = game->GetCamera().GetScreenCenter();
		lua_pushnumber(L, c.x);
		lua_pushnumber(L, c.y);
		return 2;
	}

	int GetClearTransition(lua_State *L)
	{
		lua_pushnumber(L, clearTransition);
		return 1;
	}

	int SetParami(lua_State *L /*String param, int v*/)
	{
		String param(luaL_checkstring(L, 2));
		int v(luaL_checkinteger(L, 3));
		fullscreenMaterialParams.SetParameter(param, v);
		return 0;
	}

	int SetParamf(lua_State *L /*String param, float v*/)
	{
		String param(luaL_checkstring(L, 2));
		float v(luaL_checknumber(L, 3));
		fullscreenMaterialParams.SetParameter(param, v);
		return 0;
	}
	int SetParam2f(lua_State *L /*String param, float v*/)
	{
		String param(luaL_checkstring(L, 2));
		Vector2 v = Vector2(luaL_checknumber(L,3),luaL_checknumber(L,4));
		fullscreenMaterialParams.SetParameter(param, v);
		return 0;
	}
	int SetParam3f(lua_State *L /*String param, float v*/)
	{
		String param(luaL_checkstring(L, 2));
		Vector3 v = Vector3(luaL_checknumber(L,3),luaL_checknumber(L,4),luaL_checknumber(L,5));
		fullscreenMaterialParams.SetParameter(param, v);
		return 0;
	}
	int SetParam4f(lua_State *L /*String param, float v*/)
	{
		String param(luaL_checkstring(L, 2));
		Vector4 v = Vector4(luaL_checknumber(L,3),luaL_checknumber(L,4),luaL_checknumber(L,5),luaL_checknumber(L,6));
		fullscreenMaterialParams.SetParameter(param, v);
		return 0;
	}
	int SetParam4x4f(lua_State* L) {
		String param(luaL_checkstring(L, 2));
		Transform t = readMat4(L,3);
		fullscreenMaterialParams.SetParameter(param, t);
		return 0;
	}
	int DrawShader(lua_State *L)
	{
		for (auto &texParam : textures)
		{
			fullscreenMaterialParams.SetParameter(texParam.first, texParam.second);
		}

		g_application->ForceRender();
		FullscreenBackground::Render(0,this->foreground);
		return 0;
	}
	int SetSpeedMult(lua_State *L)
	{
		speedMult = luaL_checknumber(L, 2);
		return 0;
	}

	int GetPath(lua_State *L)
	{
		lua_pushstring(L, *folderPath);
		return 1;
	}

	int SetMaterialDepthTest(lua_State* L) {
		bool enable = lua_toboolean(L,2);
		if (fullscreenMaterial)
			fullscreenMaterial->depthTest = enable;
		return 0;
	}

	int SetMaterialOpaque(lua_State* L) {
		bool enable = lua_toboolean(L,2);
		if (fullscreenMaterial)
			fullscreenMaterial->opaque = enable;
		return 0;
	}

	int SetMaterialBlendMode(lua_State* L) {
		MaterialBlendMode bm = (MaterialBlendMode) luaL_checkinteger(L,2);
		if (fullscreenMaterial)
			fullscreenMaterial->blendMode = bm;
		return 0;
	}

	Material LoadBackgroundMaterial(const String &path)
	{
		String skin = g_gameConfig.GetString(GameConfigKeys::Skin);
		String pathV = Path::Absolute(String("skins/" + skin + "/shaders/") + "background" + ".vs");
		String pathF = Path::Absolute(path);
		String pathG = Path::Absolute(String("skins/" + skin + "/shaders/") + "background" + ".gs");
		Material ret = MaterialRes::Create(g_gl, pathV, pathF);
		// Additionally load geometry shader
		if (Path::FileExists(pathG))
		{
			Shader gshader = ShaderRes::Create(g_gl, ShaderType::Geometry, pathG);
			assert(gshader);
			ret->AssignShader(ShaderType::Geometry, gshader);
		}
		return ret;
	}

	Texture LoadBackgroundTexture(const String &path)
	{
		Texture ret = TextureRes::Create(g_gl, ImageRes::Create(path));
		return ret;
	}
};

Background *CreateBackground(class Game *game, bool foreground /* = false*/)
{
	Background *bg = new TestBackground();
	bg->game = game;
	if (!bg->Init(foreground))
	{
		delete bg;
		return nullptr;
	}
	return bg;
}