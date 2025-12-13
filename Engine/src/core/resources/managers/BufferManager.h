#pragma once

#include "Manager.h"

struct Vertex;

class BufferManager : public Manager
{
public:
    virtual ~BufferManager() = default;

    virtual int init() = 0;
    virtual int onClose() = 0;
    virtual void destroy(uint32_t id) = 0;

    virtual uint32_t createVertexBuffer(const Vertex* vertices, int size) = 0;
    virtual uint32_t createIndexBuffer(const uint16_t* indices, int size) = 0;
    

protected:
    BufferManager(std::string serviceName = "BufferManager") : Manager(serviceName) {};

private:

};