#ifndef CROSSHAIR_H
#define CROSSHAIR_H

#include "i_crosshair.h"

#include <cstdint>
#include <memory>

class Shader;
struct FrameContext;

class Crosshair : public ICrosshair
{
public:
	Crosshair();
	~Crosshair();

	void init() override;
	void render(
		const FrameContext* frameVk,
		const FrameContextDX12* frameDX12
	) override;

private:
	void destroyGL();
private:
	std::unique_ptr<Shader> crosshairShader_;
	uint32_t vao_{};
	uint32_t vbo_{};
};

#endif
