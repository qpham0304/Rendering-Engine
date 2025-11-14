#pragma once

class Buffer
{
public:
	virtual ~Buffer() = default;

	virtual void create() = 0;
	virtual void destroy() = 0;

protected:
	Buffer() = default;

protected:
	void* bufferHandle { nullptr };

private:
};

