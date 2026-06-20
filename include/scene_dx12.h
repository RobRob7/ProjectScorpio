#ifndef SCENE_DX12_H
#define SCENE_DX12_H

#include "i_scene.h"

#include <memory>

class DX12Main;
class Camera;
class CubemapDX12;
class ChunkManager;

class UI;
class IRenderer;
struct RenderInputs;

class SceneDX12 final : public IScene
{
public:
	SceneDX12(
		DX12Main& dx, 
		int w, 
		int h
	);
	~SceneDX12() override;

	void init() override;

	void render(
		IRenderer& renderer,
		RenderInputs& in,
		const FrameContext* frameVk,
		const FrameContextDX12* frameDX12,
		UI* ui
	) override;

	void update(float dt, const InputState& in) override;

	void onResize(int w, int h) override;
	void onMouseMove(float x, float y) override;
	void onScroll(float yoffset) override;

	Camera& getCamera() override;
	ICubemap& getSkybox() override;
	ChunkManager& getWorld() override;
	ILight& getLight() override;

private:
	DX12Main* dx_{ nullptr };
	int width_{};
	int height_{};

	float saveTimer_{ 0.0f };
	// auto save time threshold (in min)
	const float autoSaveTime_{ 5 };

	// objects
	std::unique_ptr<Camera> camera_;
	std::unique_ptr<ChunkManager> world_;
	//std::unique_ptr<CubemapVk> skybox_;
};

#endif
