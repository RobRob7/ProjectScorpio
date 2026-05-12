#include "scene_vk.h"

#include "vulkan_main.h"

#include "constants.h"

#include "i_renderer.h"
#include "render_inputs.h"

#include "camera.h"
#include "cubemap_vk.h"
#include "crosshair_vk.h"
#include "chunk_manager.h"
#include "light_vk.h"

#include <glm/glm.hpp>

using namespace World;

//--- PUBLIC ---//
SceneVk::SceneVk(VulkanMain& vk, int w, int h)
	: vk_(vk), width_(w), height_(h)
{
} // end of constuctor

SceneVk::~SceneVk() = default;

void SceneVk::init()
{
	world_ = std::make_unique<ChunkManager>();
	world_->init(&vk_);

	camera_ = std::make_unique<Camera>(width_, height_, glm::vec3(0.0f, CHUNK_SIZE_Y, 3.0f));

	light_ = std::make_unique<LightVk>(vk_);
	light_->init();

	skybox_ = std::make_unique<CubemapVk>(vk_);
	skybox_->init();

	crosshair_ = std::make_unique<CrosshairVk>(vk_);
	crosshair_->init();
} // end of init

void SceneVk::render(
	IRenderer& renderer,
	RenderInputs& in,
	const FrameContext* frame,
	UI* ui
)
{
	if (!camera_ || !world_ || !light_ || !skybox_ || !crosshair_) return;

	in.world = world_.get();
	in.camera = camera_.get();
	in.light = light_.get();
	in.skybox = skybox_.get();
	in.crosshair = crosshair_.get();

	renderer.renderFrame(in, frame, ui);
} // end of render()

void SceneVk::update(float dt, const InputState& in)
{
	if (!camera_ || !world_) return;

	saveTimer_ += dt;
	if (saveTimer_ >= (autoSaveTime_ * 60.0f))
	{
		world_->saveWorld();
		saveTimer_ = 0.0f;
	}

	if (in.quitRequested)
	{
		world_->saveWorld();
		return;
	}

	if (in.disableCameraPressed)
	{
		camera_->setEnabled(false);
	}

	if (in.enableCameraPressed)
	{
		camera_->setEnabled(true);
	}

	// functionality ONLY when camera active
	if (camera_->isEnabled())
	{
		camera_->setAccelerationMultiplier(in.sprint ? 15.0f : 1.0f);

		if (in.w) camera_->processKeyboard(CameraMovement::FORWARD, dt);
		if (in.a) camera_->processKeyboard(CameraMovement::LEFT, dt);
		if (in.s) camera_->processKeyboard(CameraMovement::BACKWARD, dt);
		if (in.d) camera_->processKeyboard(CameraMovement::RIGHT, dt);

		if (in.removeBlockPressed)
		{
			world_->placeOrRemoveBlock(false,
				camera_->getCameraPosition(),
				camera_->getCameraDirection());
		}

		if (in.placeBlockPressed)
		{
			world_->placeOrRemoveBlock(true,
				camera_->getCameraPosition(),
				camera_->getCameraDirection());
		}
	}
} // end of update()

void SceneVk::onResize(int w, int h)
{
	width_ = w;
	height_ = h;

	if (camera_)
	{
		camera_->onResize(w, h);
	}
} // end of onResize()

void SceneVk::onMouseMove(float x, float y)
{
	if (camera_ && camera_->isEnabled())
	{
		camera_->handleMousePosition(x, y);
	}
} // end of onMouseMove()

void SceneVk::onScroll(float yoffset)
{
	if (camera_ && camera_->isEnabled())
	{
		camera_->handleMouseScroll(yoffset);
	}
} // end of onScroll()

Camera& SceneVk::getCamera()
{
	return *camera_;
} // end of getCamera()

ICubemap& SceneVk::getSkybox()
{
	return *skybox_;
} // end of getSkybox()

ChunkManager& SceneVk::getWorld()
{
	return *world_;
} // end of getWorld()

ILight& SceneVk::getLight()
{
	return *light_;
} // end of getLight()
