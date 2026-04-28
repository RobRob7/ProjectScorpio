#ifndef SCENE_VK_H
#define SCENE_VK_H

#include "i_scene.h"

#include <memory>

class VulkanMain;
class Camera;
class CubemapVk;
class CrosshairVk;
class ChunkManager;
class LightVk;

class UI;
class IRenderer;
struct RenderInputs;

class SceneVk final : public IScene
{
public:
	SceneVk(VulkanMain& vk, int w, int h);
	~SceneVk() override;

	void init() override;

	// render scene
	void render(
		IRenderer& renderer,
		RenderInputs& in,
		const FrameContext* frame,
		UI* ui
	) override;

	// handle user inputs
	void update(float dt, const InputState& in) override;

	// window events
	void onResize(int w, int h) override;
	void onMouseMove(float x, float y) override;
	void onScroll(float yoffset) override;

	// getters
	Camera& getCamera() override;
	ICubemap& getSkybox() override;
	ChunkManager& getWorld() override;
	ILight& getLight() override;

private:
	VulkanMain& vk_;
	// width of window
	int width_{};
	// height of window
	int height_{};

	// save timer
	float saveTimer_{ 0.0f };
	// auto save time threshold (in min)
	const float autoSaveTime_{ 5 };

	// objects
	std::unique_ptr<Camera> camera_;
	std::unique_ptr<CubemapVk> skybox_;
	std::unique_ptr<CrosshairVk> crosshair_;
	std::unique_ptr<ChunkManager> world_;
	std::unique_ptr<LightVk> light_;
};

#endif
