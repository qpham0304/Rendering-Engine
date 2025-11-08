#pragma once

#include <unordered_map>
#include <string>
#include <stdexcept>
#include <functional>
#include <memory>

// might want to use enum over string service
// but then how to extends?
enum class AvailableServices {
    
};

class ServiceLocator {
public:
    ServiceLocator() = default;

    template<typename T>
    void Register(std::string&& serviceName, T& serviceRef) {
        if (!hasService(serviceName)) {
            services[serviceName] = &serviceRef;
        }
        else {
            throw std::runtime_error("Service already registered: " + serviceName);
        }
    }

    template<typename T>
    T& Get(std::string&& serviceName) {
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



//TODO ideally want to move to factory
template<typename Interface, typename PlatformEnum>
class ConstructorRegistry {
public:
    typedef std::function<std::unique_ptr<Interface>()> Constructor;
    std::unordered_map<PlatformEnum, Constructor> constructors;

    template<typename Impl, typename... Args>
    void Register(PlatformEnum key, ServiceLocator& locator, std::string_view serviceName, Args&&... args) {
        constructors[key] = [&locator, serviceName, args_tuple = std::make_tuple(std::forward<Args>(args)...)]() mutable {
            auto instance = std::apply([](auto&&... unpackedArgs) {
                return std::make_unique<Impl>(std::forward<decltype(unpackedArgs)>(unpackedArgs)...);
                }, std::move(args_tuple));

            locator.Register(serviceName.data(), *instance);
            return instance;
        };
    }

    std::unique_ptr<Interface> Create(PlatformEnum platform) {
        auto it = constructors.find(platform);
        if (it == constructors.end()) {
            throw std::runtime_error("No constructor registered for the given platform.");
        }
        return it->second();
    }
};