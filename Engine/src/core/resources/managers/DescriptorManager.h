#pragma once

#include "Manager.h"

struct Vertex;

class DescriptorManager : public Manager
{
public:
    virtual ~DescriptorManager() = default;

    virtual int init(WindowConfig config) = 0;
    virtual int onClose() = 0;
    virtual void destroy(uint32_t id) = 0;
    virtual std::vector<uint32_t> listIDs() const = 0;
    virtual std::vector<uint32_t> listLayoutIDs() const = 0;
    virtual std::vector<uint32_t> listPoolIDs() const = 0;

protected:
    DescriptorManager(std::string serviceName = "DescriptorManager") : Manager(serviceName) {};

private:

};