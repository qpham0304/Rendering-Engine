#pragma once

#include <string>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <numeric>
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
#include <span>
#include <optional>
#include <variant>
#include <any>
#include <functional>
#include <random>
#include <cstdlib>
#include <cassert>
#include <typeindex>
#include <typeinfo>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
// #include <imgui.h>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>

#elif defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IPHONE || TARGET_OS_SIMULATOR
        // iOS headers
    #elif TARGET_OS_MAC
        // macOS headers
    #endif

#elif defined(__ANDROID__)
    #include <jni.h>
    #include <android/log.h>

#elif defined(__linux__)
    #include <unistd.h>
    #include <sys/sysinfo.h>
    #include <pthread.h>

#endif