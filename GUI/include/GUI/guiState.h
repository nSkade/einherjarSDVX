#pragma once
#include "nanovg.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "Graphics/Font.hpp"
#include "Graphics/RenderQueue.hpp"
#include "Shared/Transform.hpp"
#include "Shared/Files.hpp"
#include "Shared/Thread.hpp"
#include <atomic>
#include <mutex>

struct Label
{
	Text text;
	int size;
	float scale;
	FontRes::TextOptions opt;
	Graphics::Font font;
	String content;
};


struct ImageAnimation
{
	int FrameCount;
	std::atomic<int32_t> CurrentFrame;
	int TimesToLoop;
	int LoopCounter;
	int w;
	int h;
	float SecondsPerFrame;
	float Timer;
	std::atomic<bool> Compressed;
	std::atomic<bool> LoadComplete;
	std::atomic<bool> Cancelled;
	std::mutex LoadMutex;
	Vector<Graphics::Image> Frames;
	Vector<Buffer> FrameData; //for storing the file contents of the compressed frames
	Image CurrentImage; //the current uncompressed frame in use
	Image NextImage;
	Thread* JobThread;
	lua_State* State;
};

struct GUIState
{
	NVGcontext* vg;
	RenderQueue* rq;
	Transform t;
	Map<lua_State*, Map<int, Label>> textCache;
	Map<lua_State*, Map<int, NVGpaint>> paintCache;
	Map<lua_State*, int> nextTextId;
	Map<lua_State*, int> nextPaintId;
	Map<String, Graphics::Font> fontCahce;
	Map<lua_State*, Set<int>> vgImages;
	Graphics::Font currentFont;
	Vector4 fillColor;
	int textAlign;
	int fontSize;
	Material* fontMaterial;
	Material* fillMaterial;
	NVGcolor otrColor; //outer color
	NVGcolor inrColor; //inner color
	NVGcolor imageTint;
	float hueShift;
	Rect scissor;
	Vector2i resolution;
	Map<int, Ref<ImageAnimation>> animations;
	int scissorOffset;
	Vector<Transform> transformStack;
	Vector<int> nvgFonts;

	Transform modMatChart;
	Transform modMatSkin;
	Transform projMatChart;
	Transform projMatSkin;
	//Transform prevMatChart; //TODO(skade) could this be useful?
	//Transform prevMatSkin;
};
