#ifndef APPLICATION_H
#define APPLICATION_H

#include "constants.h"

#include "render_inputs.h"
#include "i_renderer.h"
#include "i_scene.h"

#include <memory>

class OpenGLMain;
class VulkanMain;
class DX12Main;
class UI;
struct InputState;
struct GLFWwindow;

class Application
{
public:
	Application(
		int width, 
		int height, 
		Backend backend
	);
	~Application();

	void run();

private:
	void switchBackend(Backend newBackend);
	void shutdownBackend();
	void initDX12();
	void initVk();
	void initOpenGL();
	void setCallbacks();
	void initWindowGL();
	void initWindowVk();
	void initWindowDX12();
	InputState buildInputState();
private:
	RenderInputs in_;

	bool leftMouseDown_ = false;
	bool rightMouseDown_ = false;

	int width_{};
	int height_{};

	GLFWwindow* window_ = nullptr;

	float deltaTime_{ 0.0f };
	float lastFrame_{ 0.0f };

	Backend backend_;
	Backend pendingBackend_;
	bool backendChangeRequested_{ false };

	std::unique_ptr<OpenGLMain> openglMain_;
	std::unique_ptr<VulkanMain> vulkanMain_;
	std::unique_ptr<DX12Main> dx12Main_;

	std::unique_ptr<IScene> scene_;
	std::unique_ptr<IRenderer> renderer_;

	std::unique_ptr<UI> ui_;
};
#endif