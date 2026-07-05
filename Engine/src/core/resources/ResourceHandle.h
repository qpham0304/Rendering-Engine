#include <string>
#include "core/resources/managers/Manager.h"

template <typename T>
class ResourceHandle 
{
public:
    ResourceHandle() : manager(nullptr) {}
    ResourceHandle(std::string name, uint32_t id) 
                : m_name(name), m_id(id), m_manager(manager) {};

    T* get() const {
        if(!m_manager) {
            return nullptr;
        }
        return m_manager->get<T>(m_id);
    }

    // bool isValid() const {
    //     return manager && manager->get<T>(id)
    // }

    std::string name() { return m_name; }
    std::string id() { return m_id; }

    T* operator->() const {
        return get();
    }

    T& operator* const {
        return *get();
    }

    // operator bool() const {
    //     reurn isValid;
    // }

private:
    std::string m_name;
    uint32_t m_id
    Manager* m_manager;
};