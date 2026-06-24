#ifndef CROSSHAIR_DX12_H
#define CROSSHAIR_DX12_H

#include "i_crosshair.h"

#include "buffer_dx12.h"
#include "graphics_pipeline_dx12.h"

#include <memory>

class DX12Main;
class ShaderDX12;
struct FrameContext;
struct FrameContextDX12;

class CrosshairDX12 : public ICrosshair
{
public:
	CrosshairDX12(DX12Main& dx);
	~CrosshairDX12();

	void init() override;
	void render(
		const FrameContext* frameVk,
		const FrameContextDX12* frameDX12
	) override;

private:
	void createResources();
	void createPipeline();
private:
	DX12Main* dx_{ nullptr };

	std::unique_ptr<ShaderDX12> shader_;

	BufferDX12 vBuffer_;
	GraphicsPipelineDX12 pipeline_;
};

#endif
