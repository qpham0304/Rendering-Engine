#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include "core/features/Object.h"

struct ProfilerData {
	const char* name;
	double time;
};

class Profiler : public Object
{
public:
	~Profiler() = default;

	static void addTracker(ProfilerData&& data);
	static void addTracker(const ProfilerData& data);
	static void display();

private:
	Profiler() { setFootprint("Profiler", sizeof(Profiler), MemorySystem::Core); };
	
	static Profiler& _getInstance();
	void _addTracker(ProfilerData&& data);
	void _addTracker(const ProfilerData& data);
	void _display();

private:
	std::unordered_map<std::string, double> profileList;

	bool showDetail{ false };
};
