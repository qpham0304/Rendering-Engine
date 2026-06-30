#include <string>
#include <unordered_map>
#include <typeindex>
#include <typeinfo>
#include "core/resources/Resource.h"

class ResourceManager 
{
public:


private:
    struct ResourceData {
        std::shared_ptr<Resource> resource;
        int refCount;
    };
    
    // Two-level storage system: organize by type first, then by unique identifier
    // This approach enables type-safe resource access while maintaining efficient lookup
    std::unordered_map<std::type_index, std::unordered_map<std::string, std::shared_ptr<Resource>>> resources;

    // Two-level reference counting system for automatic resource lifecycle management
    // First level maps resource type, second level maps resource IDs to their data
    std::unordered_map<std::type_index, std::unordered_map<std::string, ResourceData>> refCounts;
};