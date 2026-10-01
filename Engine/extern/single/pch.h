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
#include <shared_mutex>
#include <thread>
#include <atomic>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <variant>
#include <any>
#include <functional>

#define PLATFORM_WINDOWS

#ifdef PLATFORM_WINDOWS
	#include <windows.h>
#elif PLATFORM_LINUX
#endif
