#pragma once
#include <cstdint>

class Buffer
{
public:
	virtual ~Buffer() = default;

	virtual void create(const void* data, size_t size) = 0;
	virtual void destroy() = 0;
	virtual void bind() = 0;

protected:
	Buffer() = default;

protected:
	void* bufferHandle { nullptr };

private:
};

