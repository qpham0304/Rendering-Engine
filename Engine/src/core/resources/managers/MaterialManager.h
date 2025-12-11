#pragma once

#include "Manager.h"

class MaterialManager : public Manager
{
public:
    MaterialManager();
    ~MaterialManager();

    virtual void init() = 0;
    virtual void shutdown() = 0;
    virtual void destroy(uint32_t id) = 0;

private:
    
};