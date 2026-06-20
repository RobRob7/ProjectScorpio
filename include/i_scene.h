#ifndef I_SCENE_H
#define I_SCENE_H

class IRenderer;
class RenderInputs;

class Camera;
class ICubemap;
class ChunkManager;
class ILight;
struct FrameContext;
struct FrameContextDX12;
class UI;

struct InputState
{
	// keys
	bool w = false;
	bool a = false;
	bool s = false;
	bool d = false;
	bool sprint = false;

	// actions
	bool enableCameraPressed = false;
	bool disableCameraPressed = false;
	bool placeBlockPressed = false;
	bool removeBlockPressed = false;
	bool quitRequested = false;
	bool enableImguiPressed = false;
	bool disableImguiPressed = false;
};

class IScene
{
public:
	virtual ~IScene() = default;

	virtual void init() = 0;

	// render scene
	virtual void render(
		IRenderer& renderer, 
		RenderInputs& in, 
		const FrameContext* frameVk,
		const FrameContextDX12* frameDX12,
		UI* ui
	) = 0;

	// handle user inputs
	virtual void update(float dt, const InputState& in) = 0;

	// window events
	virtual void onResize(int w, int h) = 0;
	virtual void onMouseMove(float x, float y) = 0;
	virtual void onScroll(float yoffset) = 0;

	// getters
	virtual Camera& getCamera() = 0;
	virtual ICubemap& getSkybox() = 0;
	virtual ChunkManager& getWorld() = 0;
	virtual ILight& getLight() = 0;
};

#endif
