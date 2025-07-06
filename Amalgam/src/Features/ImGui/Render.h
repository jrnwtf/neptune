#pragma once
#include "../../SDK/SDK.h"

// Dear ImGui main header – always included to share core definitions (ImColor, ImFont, etc.)
#include <ImGui/imgui.h>

#ifndef TEXTMODE
#  include <ImGui/imgui_impl_dx9.h>
#else
// When building in TEXTMODE we don't have access to DirectX headers, so just forward-declare the type
struct IDirect3DDevice9;
#endif

class CRender
{
public:
	void Render(IDirect3DDevice9* pDevice);
	void Initialize(IDirect3DDevice9* pDevice);

	void LoadColors();
	void LoadFonts();
	void LoadStyle();

	int Cursor = 2;

	// Colors
	ImColor Accent = {};
	ImColor Background0 = {};
	ImColor Background0p5 = {};
	ImColor Background1 = {};
	ImColor Background1p5 = {};
	ImColor Background1p5L = {};
	ImColor Background2 = {};
	ImColor Inactive = {};
	ImColor Active = {};

	// Fonts
	ImFont* FontSmall = nullptr;
	ImFont* FontRegular = nullptr;
	ImFont* FontBold = nullptr;
	ImFont* FontLarge = nullptr;
	ImFont* FontMono = nullptr;

	ImFont* IconFont = nullptr;
};

ADD_FEATURE(CRender, Render);