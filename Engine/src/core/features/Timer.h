#pragma once

#include <chrono>
class Timer
{
public:
	Timer();
	Timer(const char* label, const bool guiOn = false);
	Timer(std::string_view label, const bool guiOn);
	~Timer();

private:
	std::chrono::steady_clock::time_point start;
	std::chrono::steady_clock::time_point end;
	std::chrono::duration<float> duration;
	const char* label;
	bool guiOn;

	void Start();
	float Stop();
};
