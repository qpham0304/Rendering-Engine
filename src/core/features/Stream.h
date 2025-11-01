#pragma once

#include <nlohmann/json.hpp>

using json = nlohmann::json;

class Stream
{
public:
	Stream(std::string_view path);
	~Stream() = default;

	bool contains(std::string_view path);
	//void pushNode(std::string_view key);
	//void pushNode(std::string_view key);

private:
	json jsonData;

};

