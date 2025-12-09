#pragma once

#include <vector>
#include <string>
#include <unordered_map>

struct ProfilerData {
	const char* name;
	double time;
};

class Profiler
{
public:
	~Profiler() = default;

	static void addTracker(ProfilerData&& data);
	static void addTracker(const ProfilerData& data);
	static void display();

private:
	Profiler() = default;
	
	static Profiler& _getInstance();
	void _addTracker(ProfilerData&& data);
	void _addTracker(const ProfilerData& data);
	void _display();

private:
	std::unordered_map<std::string, double> profileList;

};
