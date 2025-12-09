#pragma once
class RenderPass
{
public:
	virtual ~RenderPass() = default;

	virtual void create() = 0;
	virtual void destroy() = 0;
	virtual void bind() = 0;

protected:
	RenderPass() = default;

private:
	void* renderPassHandle { nullptr };

};

