#ifndef I_CROSSHAIR_H
#define I_CROSSHAIR_H

struct FrameContext;

class ICrosshair
{
public:
	virtual ~ICrosshair() = default;

	virtual void init() = 0;
	virtual void render(const FrameContext* frame) = 0;
};

#endif
