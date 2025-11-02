#pragma once

#include <unordered_map>
#include <string>
#include <stdexcept>

// might want to use enum over string service
// but then how to extends?
enum class AvailableService {
    
};

class ServiceLocator {
public:
    ServiceLocator() = default;

    template<typename T>
    void Register(std::string serviceName, T& serviceRef) {
        if (!hasService(serviceName)) {
            services[serviceName] = &serviceRef;
        }
        else {
            throw std::runtime_error("Service already registered: " + serviceName);
        }
    }

    template<typename T>
    T& Get(std::string serviceName) {
        auto it = services.find(serviceName);
        if (it != services.end()) {
            return *static_cast<T*>(it->second);
        }
        throw std::runtime_error("Service not found: " + serviceName);
    }

private:
    bool hasService(std::string_view) const;


private:
    std::unordered_map<std::string, void*>  services;
};
