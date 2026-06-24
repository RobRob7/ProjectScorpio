#ifndef I_CROSSHAIR_H
#define I_CROSSHAIR_H

struct FrameContext;
struct FrameContextDX12;

class ICrosshair
{
public:
	virtual ~ICrosshair() = default;

	virtual void init() = 0;
	virtual void render(
		const FrameContext* frameVk,
		const FrameContextDX12* frameDX12
	) = 0;
};

#endif
