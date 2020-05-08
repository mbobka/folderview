#pragma once

#include "targetver.h"
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>
#include <shellapi.h>
#include <objidl.h>
#include <gdiplus.h>
using namespace Gdiplus;
#pragma comment (lib,"Gdiplus.lib")

#undef min
#undef max
#include <iostream>
#include <filesystem>
#include <string>
#include <sstream>
