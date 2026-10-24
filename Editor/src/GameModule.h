#pragma once

#include <filesystem>
#include <chrono>
#include <cstdint>

class Engine;

// loads a game module dll from a private copy so the original can be relinked while the editor runs,
// and swaps the new build in once the output stops changing. replaced copies stay loaded until exit
class GameModule
{
public:
	GameModule(Engine& engine, std::filesystem::path modulePath);
	~GameModule();

	GameModule(const GameModule&) = delete;
	GameModule& operator=(const GameModule&) = delete;

	bool load();
	void unload();
	void reloadIfChanged();	// once per frame, outside layer iteration

private:
	using GameFn = void(*)(Engine&);
	using Clock = std::chrono::steady_clock;

	Engine& engine;
	std::filesystem::path modulePath;
	std::filesystem::path shadowRoot;
	std::filesystem::file_time_type loadedWriteTime{};
	std::filesystem::file_time_type pendingWriteTime{};
	Clock::time_point pendingSince{};
	Clock::time_point lastPoll{};
	void* handle = nullptr;
	GameFn unloadFn = nullptr;
	uint32_t loadCount = 0;
};
