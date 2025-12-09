#pragma once

#include <functional>
#include <iostream>
#include <any>
#include <variant>
#include <stdexcept>
#include "src/graphics/utils/Utils.h"
#include "src/core/features/Timer.h"
#include "./Event.h"

class EventListener 
{
private:
	std::any callback;
	
	template<typename... Args>
	void setCallback(std::function<void(Args ...)> cb) {
		callback = cb;
	}

	template<typename... Args>
	bool isCallbackValid() const {
		if (callback.type() == typeid(std::function<void(Args...)>)) {
			return true;
		}
		return false;
	}


public:
	template<typename... Args>
	EventListener(std::function<void(Args... args)> cb) {
		setCallback(cb);
	}

	template<typename... Args>
	EventListener(std::function<void(Args... args)>&& cb) {
		setCallback(cb);
	}

	//overhead might be high
	//need to check if client provides wrong arguments when publishing the event otherwise the app would crash
	template<typename... Args>
	void onEvent(Args... args) { 
		if (callback.has_value()) {
			if (isCallbackValid<Args...>()) {	// this can kinda avoid the crash
				auto& cb = std::any_cast<std::function<void(Args...)>&>(callback);
				cb(std::forward<Args>(args)...);
			}
			else {
				Console::error("Invalid callback provided\n");
			}
		}
		else {
			Console::error("Callback handled or Error retriving erased type\n");
		}
	};
};