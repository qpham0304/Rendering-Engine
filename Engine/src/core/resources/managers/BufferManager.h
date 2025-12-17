#pragma once

#include "Manager.h"

struct Vertex;
struct Buffer;

class BufferManager : public Manager
{
public:
    virtual ~BufferManager() = default;

    virtual int init(WindowConfig config) = 0;
    virtual int onClose() = 0;
    virtual void destroy(uint32_t id) = 0;
    virtual std::vector<uint32_t> listIDs() const = 0;



    virtual uint32_t createVertexBuffer(const Vertex* vertices, int size) = 0;
    virtual uint32_t createIndexBuffer(const uint16_t* indices, int size) = 0;
    virtual void bind(uint32_t id) = 0;
    virtual Buffer* getBuffer(uint32_t id) = 0;


protected:
    BufferManager(std::string serviceName = "BufferManager") : Manager(serviceName) {};

private:

};