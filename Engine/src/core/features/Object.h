#pragma once
#include <string>
#include "core/memory/MemoryTrackable.h"

class Object : public MemoryTrackable
{
public:
    std::string name();
    uint32_t id();

protected:
    Object() = default;
    ~Object() = default;

private:
    uint32_t m_id;
    std::string m_name;

};