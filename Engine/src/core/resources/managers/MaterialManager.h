#pragma once

#include "Manager.h"

class MaterialManager : public Manager
{
public:
    ~MaterialManager();

    virtual int init() = 0;
    virtual int onClose() = 0;
    virtual void destroy(uint32_t id) = 0;

private:
    MaterialManager(std::string serviceName = "MaterialManager") : Manager(serviceName) {};
    
};