#include "application.h"

#include "frame_context_vk.h"
#include "frame_context_dx12.h"

#include "opengl_main.h"
#include "vulkan_main.h"
#include "dx12_main.h"

#include "ui.h"
#include "scene.h"
#include "scene_vk.h"
#include "scene_dx12.h"
#include "renderer_gl.h"
#include "renderer_vk.h"

#include <GLFW/glfw3.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <filesystem>
#include <iostream>

//--- HELPER ---//
static void RunShaderCompilerScript()
{
	namespace fs = std::filesystem;

	fs::path buildDir = fs::path(RESOURCES_PATH);

	fs::path scriptPath = buildDir / "convert_spv.py";
	fs::path shaderRoot = buildDir / "shader";

	if (!fs::exists(scriptPath))
	{
		throw std::runtime_error("Missing script: " + scriptPath.string());
	}
	if (!fs::exists(shaderRoot))
	{
		throw std::runtime_error("Missing shader folder: " + shaderRoot.string());
	}

	const std::string pythonExe = "python";

#ifdef _DEBUG
	const std::string buildMode = "debug";
#else
	const std::string buildMode = "release";
#endif

	std::string cmd =
		pythonExe + " \"" + scriptPath.make_preferred().string() + "\" \"" + shaderRoot.make_preferred().string() + "\" " + buildMode;

	//std::cout << "Running: " << cmd << "\n";

	int code = std::system(cmd.c_str());
	if (code != 0)
	{
		throw std::runtime_error("Shader compilation script failed. Exit code: " + std::to_string(code));
	}
} // end of RunShaderCompilerScript()

static void WarmupOpenGLContext()
{
	glfwDefaultWindowHints();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

	// request opengl debug context ONLY in debug mode
#ifdef _DEBUG
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

	GLFWwindow* warmup = glfwCreateWindow(64, 64, "", nullptr, nullptr);
	if (!warmup)
	{
		throw std::runtime_error("Failed to create GL warmup window");
	}

	glfwMakeContextCurrent(warmup);

	glfwMakeContextCurrent(nullptr);
	glfwDestroyWindow(warmup);

#ifdef _DEBUG
	std::cout << "[TEST] OpenGL warmup context created/destroyed\n";
#endif
} // end of WarmupOpenGLContext()


//--- PUBLIC ---//
Application::Application(
	int width, 
	int height,
	Backend backend
)
	: width_(width), 
	height_(height), 
	backend_(backend), 
	pendingBackend_(backend)
{
	// intialize GLFW
	if (!glfwInit())
	{
		throw std::runtime_error("GLFW initialization error!");
	}

	if (backend_ == Backend::OpenGL)
	{
		initOpenGL();
	}
	else if (backend_ == Backend::Vulkan)
	{
		initVk();
	}
	else if (backend == Backend::DX12)
	{
		initDX12();
	}
	else
	{
		throw std::runtime_error("BACKEND not supported!");
	}
} // end of constructor

Application::~Application()
{
	shutdownBackend();
	glfwTerminate();
} // end of destructor

void Application::run()
{
	while (!glfwWindowShouldClose(window_))
	{
		///////// BEFORE RENDER ///////////
		// per-frame time logic
		float currentFrame = static_cast<float>(glfwGetTime());
		deltaTime_ = currentFrame - lastFrame_;
		lastFrame_ = currentFrame;

		// poll user input events
		glfwPollEvents();

		// process user input
		InputState input = buildInputState();
		scene_->update(deltaTime_, input);

		// process window close request
		if (input.quitRequested)
		{
			glfwSetWindowShouldClose(window_, true);
		}

		// process camera active/inactive mouse cursor
		if (input.enableCameraPressed)
		{
			glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		}
		if (input.disableCameraPressed)
		{
			glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
		///////////////////////////////////


		///////////// RENDER ///////////////
		in_.time = static_cast<float>(glfwGetTime());

		// OPENGL
		if (openglMain_)
		{
			ui_->beginFrame();

			scene_->render(*renderer_, in_, {}, nullptr);

			ui_->buildUI(deltaTime_, *scene_);
			ui_->renderGL();

			Backend requestedBackend{};
			if (ui_ && ui_->applyBackendRequest(requestedBackend))
			{
				switchBackend(requestedBackend);
				continue;
			}

			glfwSwapBuffers(window_);
		}
		// VULKAN
		if (vulkanMain_)
		{
			FrameContext frame{};
			if (!vulkanMain_->beginFrame(frame))
			{
				if (width_ > 0 && height_ > 0)
				{
					if (scene_)		scene_->onResize(width_, height_);
					if (renderer_)	renderer_->resize(width_, height_);
					if (ui_)		ui_->onSwapchainRecreated();
				}
				continue;
			}

			ui_->beginFrame();

			ui_->buildUI(deltaTime_, *scene_);

			scene_->render(*renderer_, in_, &frame, ui_.get());

			if (!vulkanMain_->endFrame(frame))
			{
				continue;
			}

			Backend requestedBackend{};
			if (ui_ && ui_->applyBackendRequest(requestedBackend))
			{
				switchBackend(requestedBackend);
				continue;
			}
		}
		// DX12
		if (dx12Main_)
		{
			FrameContextDX12 frame{};
			if (!dx12Main_->beginFrame(frame))
			{
				if (width_ > 0 && height_ > 0)
				{
					if (scene_)		scene_->onResize(width_, height_);
					if (renderer_)	renderer_->resize(width_, height_);
					if (ui_)		ui_->onSwapchainRecreated();
				}
				continue;
			}

			ui_->beginFrame();

			ui_->buildUI(deltaTime_, *scene_);

			//scene_->render(*renderer_, in_, &frame, ui_.get());

			if (!dx12Main_->endFrame(frame))
			{
				continue;
			}

			Backend requestedBackend{};
			if (ui_ && ui_->applyBackendRequest(requestedBackend))
			{
				switchBackend(requestedBackend);
				continue;
			}
		}
		///////////////////////////////////
	} // end while
} // end of run()

//--- PRIVATE ---//
void Application::switchBackend(Backend newBackend)
{
	if (newBackend == backend_)
	{
		return;
	}

	shutdownBackend();

	backend_ = newBackend;
	pendingBackend_ = newBackend;

	if (backend_ == Backend::OpenGL)
	{
		initOpenGL();
	}
	else if (backend_ == Backend::Vulkan)
	{
		initVk();
	}
	else if (backend_ == Backend::DX12)
	{
		initDX12();
	}
	else
	{
		throw std::runtime_error("BACKEND not supported!");
	}
} // end of switchBackend()

void Application::shutdownBackend()
{
	if (vulkanMain_)
	{
		vulkanMain_->waitIdle();
	}

	if (dx12Main_)
	{
		dx12Main_->waitIdle();
	}

	ui_.reset();
	renderer_.reset();
	scene_.reset();
	openglMain_.reset();
	vulkanMain_.reset();
	dx12Main_.reset();

	if (window_)
	{
		glfwDestroyWindow(window_);
		window_ = nullptr;
	}
} // end of shutdownBackend()

void Application::initDX12()
{
	WarmupOpenGLContext();

	initWindowDX12();
	dx12Main_ = std::make_unique<DX12Main>(window_);
	dx12Main_->init();

	// setup scene + renderer
	scene_ = std::make_unique<SceneDX12>(*dx12Main_, width_, height_);
	scene_->init();
	//renderer_ = std::make_unique<RendererVk>(*vulkanMain_);
	//renderer_->init();
	//renderer_->resize(width_, height_);

	setCallbacks();

	// setup UI
	//ui_ = std::make_unique<UI>(
	//	nullptr,
	//	window_,
	//	renderer_->settings(),
	//	backend_,
	//	*dx12Main_
	//);
} // end of initDX12()

void Application::initVk()
{
	WarmupOpenGLContext();

	RunShaderCompilerScript();

	initWindowVk();
	vulkanMain_ = std::make_unique<VulkanMain>(window_);
	vulkanMain_->init();

	// setup scene + renderer
	scene_ = std::make_unique<SceneVk>(*vulkanMain_, width_, height_);
	scene_->init();
	renderer_ = std::make_unique<RendererVk>(*vulkanMain_);
	renderer_->init();
	renderer_->resize(width_, height_);

	setCallbacks();

	// setup UI
	ui_ = std::make_unique<UI>(
		vulkanMain_.get(),
		window_, 
		renderer_->settings(), 
		backend_
	);
} // end of initVk()

void Application::initOpenGL()
{
	initWindowGL();
	openglMain_ = std::make_unique<OpenGLMain>();
	openglMain_->init();

	// setup scene + renderer
	scene_ = std::make_unique<Scene>(width_, height_);
	scene_->init();
	renderer_ = std::make_unique<RendererGL>();
	renderer_->init();
	renderer_->resize(width_, height_);

	setCallbacks();

	// setup UI
	ui_ = std::make_unique<UI>(
		nullptr,
		window_,
		renderer_->settings(),
		backend_
	);
} // end of initOpenGL()

void Application::setCallbacks()
{
	// set callbacks
	glfwSetWindowUserPointer(window_, this);
	glfwSetFramebufferSizeCallback(window_, [](GLFWwindow* window, int width, int height)
		{
			// resize check
#ifdef _DEBUG
			printf("[RESIZE] fb = %d x %d\n", width, height);
#endif

			auto* self = static_cast<Application*>(glfwGetWindowUserPointer(window));
			if (!self) return;

			self->width_ = width;
			self->height_ = height;

			// vulkan path
			if (self->vulkanMain_)
			{
				self->vulkanMain_->notifyFramebufferResized();

				return;
			}

			// DX12 path
			if (self->dx12Main_)
			{
				self->dx12Main_->notifyFramebufferResized();

				return;
			}

			// opengl path
			if (self->scene_)
			{
				self->scene_->onResize(self->width_, self->height_);
			}
			if (self->renderer_)
			{
				self->renderer_->resize(self->width_, self->height_);
			}
		});
	glfwSetCursorPosCallback(window_, [](GLFWwindow* window, double xposIn, double yposIn)
		{
			auto* self = static_cast<Application*>(glfwGetWindowUserPointer(window));
			if (!self || !self->scene_) return;

			self->scene_->onMouseMove(static_cast<float>(xposIn),
				static_cast<float>(yposIn));
		});
	glfwSetScrollCallback(window_, [](GLFWwindow* window, double xoffset, double yoffset)
		{
			auto* self = static_cast<Application*>(glfwGetWindowUserPointer(window));
			if (!self || !self->scene_) return;


			self->scene_->onScroll(static_cast<float>(yoffset));
		});
} // end of setCallbacks()

void Application::initWindowGL()
{
	glfwDefaultWindowHints();
	// specify major OpenGL version
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	// specify minor OpenGL version
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	// specify OpenGL core-profile
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// disable top bar
	glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
	// allow resizing
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	// WINDOW CREATION + CHECK
	window_ = glfwCreateWindow(width_, height_, "Scorpio", nullptr, nullptr);
	if (!window_)
	{
		glfwTerminate();
		throw std::runtime_error("GLFW window creation failure!");
	} // end if

	// tell GLFW to make the context of our window the main context on the current thread
	glfwMakeContextCurrent(window_);

	// tell GLFW to capture our mouse
	glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
} // end of initWindowGL()

void Application::initWindowVk()
{
	glfwDefaultWindowHints();
	// disable top bar
	glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
	// allow resizing
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	// vulkan glfw
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	// WINDOW CREATION + CHECK
	window_ = glfwCreateWindow(width_, height_, "Scorpio", nullptr, nullptr);
	if (!window_)
	{
		glfwTerminate();
		throw std::runtime_error("GLFW window creation failure!");
	} // end if

	// tell GLFW to capture our mouse
	glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
} // end of initWindowVk()

void Application::initWindowDX12()
{
	glfwDefaultWindowHints();
	// disable top bar
	glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
	// allow resizing
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	// vulkan glfw
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	// WINDOW CREATION + CHECK
	window_ = glfwCreateWindow(width_, height_, "Scorpio", nullptr, nullptr);
	if (!window_)
	{
		glfwTerminate();
		throw std::runtime_error("GLFW window creation failure!");
	} // end if

	// tell GLFW to capture our mouse
	glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
} // end of initWindowDX12()

InputState Application::buildInputState()
{
	// TEMP: allows keyboard inputs to change state
	// of view mode and SSAO
	// Debug view is not a feature to be used for release
	// SSAO toggle should be mainly controlled through UI
	RenderSettings& settings = renderer_->settings();

#ifdef _DEBUG
	// ------- graphics options -------
	// SSAO
	static bool spaceWasDown = false;
	bool spaceDown = glfwGetKey(window_, GLFW_KEY_SPACE) == GLFW_PRESS;
	if (spaceDown && !spaceWasDown)
	{
		settings.useSSAO = !settings.useSSAO;
	}
	spaceWasDown = spaceDown;

	// debug
	if (glfwGetKey(window_, GLFW_KEY_1) == GLFW_PRESS)
	{
		settings.debugMode = DebugMode::None;
	}
	if (glfwGetKey(window_, GLFW_KEY_2) == GLFW_PRESS)
	{
		settings.debugMode = DebugMode::Normals;
	}
	if (glfwGetKey(window_, GLFW_KEY_3) == GLFW_PRESS)
	{
		settings.debugMode = DebugMode::Depth;
	}
	if (glfwGetKey(window_, GLFW_KEY_4) == GLFW_PRESS)
	{
		settings.debugMode = DebugMode::ShadowMap;
	}
	if (glfwGetKey(window_, GLFW_KEY_5) == GLFW_PRESS)
	{
		settings.debugMode = DebugMode::rtDepth;
	}
#endif

	InputState in{};

	// quit
	in.quitRequested = (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS);

	// camera enable/disable
	in.disableCameraPressed = (glfwGetKey(window_, GLFW_KEY_MINUS) == GLFW_PRESS);
	in.enableCameraPressed = (glfwGetKey(window_, GLFW_KEY_EQUAL) == GLFW_PRESS);

	// UI display
	in.disableImguiPressed = (glfwGetKey(window_, GLFW_KEY_LEFT) == GLFW_PRESS);
	in.enableImguiPressed = (glfwGetKey(window_, GLFW_KEY_RIGHT) == GLFW_PRESS);

	// movement
	in.w = (glfwGetKey(window_, GLFW_KEY_W) == GLFW_PRESS);
	in.s = (glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS);
	in.a = (glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS);
	in.d = (glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS);
	in.sprint = (glfwGetKey(window_, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);

	// LMB
	int leftState = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT);
	in.removeBlockPressed = (leftState == GLFW_PRESS && !leftMouseDown_);
	leftMouseDown_ = (leftState == GLFW_PRESS);
	// RMB
	int rightState = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_RIGHT);
	in.placeBlockPressed = (rightState == GLFW_PRESS && !rightMouseDown_);
	rightMouseDown_ = (rightState == GLFW_PRESS);

	return in;
} // end of buildInputState()