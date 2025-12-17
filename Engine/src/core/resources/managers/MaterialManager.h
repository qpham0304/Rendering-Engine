#pragma once

#include "Manager.h"
#include "core/features/Material.h"

class MaterialDesc;

class MaterialManager : public Manager
{
public:
    virtual ~MaterialManager() = default;

    virtual int init(WindowConfig config) = 0;
    virtual int onClose() = 0;
    virtual void destroy(uint32_t id) = 0;
    virtual std::vector<uint32_t> listIDs() const = 0;
    virtual uint32_t createMaterial(const MaterialDesc& material) = 0;
    virtual void bindMaterial(const uint32_t& id, void* cmdBuffer = nullptr) = 0;
    virtual MaterialDesc getMaterial(const uint32_t& id) = 0;
    virtual void* getMaterialLayout() = 0;

protected:
    MaterialManager(std::string serviceName = "MaterialManager") : Manager(serviceName) {};

};