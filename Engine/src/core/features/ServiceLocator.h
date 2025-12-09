#pragma once

#include <unordered_map>
#include <string>
#include <stdexcept>
#include <functional>
#include <memory>

// might want to use enum over string name for each supported service
// but then how to extend the support for them later on? (prob never)
// aka adding more services from elsewhere without touching this enum
// benefit? prevent client from string typos
enum class AvailableServices {
    
};

class ServiceLocator {

public:
    static void setContext(ServiceLocator* other);
    static void supportingServices();

public:
    ServiceLocator() = default;

    template<typename T>
    void Register(std::string_view serviceName, T& serviceRef) {
        if (!hasService(serviceName)) {
            services[serviceName.data()] = &serviceRef;
        }
        else {
            throw std::runtime_error("Service already registered: " + std::string(serviceName));
        }
    }

    template<typename T>
    T& Get(std::string_view serviceName) {
        auto it = services.find(serviceName.data());
        if (it != services.end()) {
            return *static_cast<T*>(it->second);
        }
        throw std::runtime_error("Service not found: " + std::string(serviceName));
    }

    template<typename T>
    static T& GetService(std::string_view serviceName) {
        return instance->Get<T>(serviceName);
    }


private:
    void listServices();
    bool hasService(std::string_view) const;

private:
    static ServiceLocator* instance;

    std::unordered_map<std::string, void*>  services;
};
