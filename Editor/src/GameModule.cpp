#include "GameModule.h"
#include "Logging/Logger.h"
#include "core/features/ServiceLocator.h"
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace fs = std::filesystem;

namespace {
	constexpr auto pollInterval = std::chrono::milliseconds(250);
	constexpr auto settleTime = std::chrono::milliseconds(500);	// linker writes dll and pdb in steps, wait for it to finish

	Logger& log()
	{
		return ServiceLocator::GetService<Logger>("Engine_LoggerSPD");
	}

	void* openLibrary(const fs::path& path)
	{
#ifdef _WIN32
		return LoadLibraryW(path.wstring().c_str());
#else
		return dlopen(path.string().c_str(), RTLD_NOW);
#endif
	}

	void* findSymbol(void* handle, const char* name)
	{
#ifdef _WIN32
		return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name));
#else
		return dlsym(handle, name);
#endif
	}

	void closeLibrary(void* handle)
	{
#ifdef _WIN32
		FreeLibrary(static_cast<HMODULE>(handle));
#else
		dlclose(handle);
#endif
	}
}

GameModule::GameModule(Engine& engine, fs::path modulePath)
	: engine(engine),
	modulePath(std::move(modulePath)),
	shadowRoot(this->modulePath.parent_path() / "hotreload")
{
	std::error_code ec;
	fs::remove_all(shadowRoot, ec);	// copies from a previous session, can fail if another editor still holds them
}

GameModule::~GameModule()
{
	unload();
}

bool GameModule::load()
{
	std::error_code ec;
	fs::file_time_type writeTime = fs::last_write_time(modulePath, ec);
	if (ec) {
		log().error("GameModule: module not found {}", modulePath.string());
		return false;
	}

	// stage a copy first, if the linker still holds the file keep the current module and retry next poll
	fs::path stageDir = shadowRoot / std::to_string(loadCount);
	fs::create_directories(stageDir, ec);
	fs::path stagedModule = stageDir / modulePath.filename();
	if (!fs::copy_file(modulePath, stagedModule, fs::copy_options::overwrite_existing, ec)) {
		return false;
	}
	fs::path pdb = fs::path(modulePath).replace_extension(".pdb");
	fs::copy_file(pdb, stageDir / pdb.filename(), fs::copy_options::overwrite_existing, ec);	// missing debug info is not fatal
	loadCount++;

	unload();
	loadedWriteTime = writeTime;	// even if loading fails, don't retry the same broken build every poll

	handle = openLibrary(stagedModule);
	if (!handle) {
		log().error("GameModule: failed to load {}", stagedModule.string());
		return false;
	}

	GameFn loadFn = reinterpret_cast<GameFn>(findSymbol(handle, "wkGameLoad"));
	unloadFn = reinterpret_cast<GameFn>(findSymbol(handle, "wkGameUnload"));
	if (!loadFn || !unloadFn) {
		log().error("GameModule: {} is missing wkGameLoad/wkGameUnload", modulePath.filename().string());
		closeLibrary(handle);
		handle = nullptr;
		unloadFn = nullptr;
		return false;
	}

	loadFn(engine);
	log().info("GameModule: loaded {}", stagedModule.string());
	return true;
}

// game removes its layers
// the dll is never freed the os releases it on exit: engine side objects the game created,
// like entt component pools or callbacks, can still point into its code after the layer is gone
void GameModule::unload()
{
	if (!handle) {
		return;
	}
	unloadFn(engine);	
	
	handle = nullptr;
	unloadFn = nullptr;
}

void GameModule::reloadIfChanged()
{
	Clock::time_point now = Clock::now();
	if (now - lastPoll < pollInterval) {
		return;
	}
	lastPoll = now;

	std::error_code ec;
	fs::file_time_type writeTime = fs::last_write_time(modulePath, ec);
	if (ec || writeTime == loadedWriteTime) {
		return;
	}

	if (writeTime != pendingWriteTime) {
		pendingWriteTime = writeTime;
		pendingSince = now;
		return;
	}

	if (now - pendingSince >= settleTime) {
		load();
	}
}
