#pragma once

#include <string>

class System
{
protected:
	System(const std::string& name);

	std::string name;

public:
	virtual ~System() = default;

	const std::string getName() const;

	virtual bool init() = 0;
	virtual void update() = 0;
	virtual void render() = 0;
	virtual void read() = 0;
};

