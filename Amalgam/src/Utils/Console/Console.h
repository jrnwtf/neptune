#pragma once

#include "../Feature/Feature.h"

class CConsole
{
public:
	void Initialize(const char* title);
	void Unload();
	void Log(const char* text, ...);
};

ADD_FEATURE_CUSTOM(CConsole, Console, U);