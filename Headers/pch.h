#pragma once

#include <string>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <future>
#include <mutex>
#include <thread>
#include <cstring>

#define PLATFORM_WINDOWS

#ifdef PLATFORM_WINDOWS
	//#include <windows.h>
	#include "../src/core/Application.h"
#endif
