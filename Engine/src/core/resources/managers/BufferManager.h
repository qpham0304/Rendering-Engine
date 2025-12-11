#pragma once

#include "Manager.h"

class BufferManager : public Manager
{
public:
    virtual ~BufferManager() = default;

    virtual void init() = 0;
    virtual void shutdown() = 0;
    virtual void destroy(uint32_t id) = 0;

    // virtual uint32_t createVertexBuffer(const Vertex* vertices, int size) = 0;
    // virtual uint32_t createIndexBuffer(uint32_t* indices, int size) = 0;
    

protected:
    BufferManager() = default;

private:

};