#include "ui.h"

#include "vulkan_main.h"
#include "render_settings.h"
#include "frame_context_vk.h"

#include "i_scene.h"
#include "i_light.h"

#include "texture_2d_vk.h"
#include "texture.h"

#include "chunk_manager.h"
#include "camera.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vulkan/vulkan.hpp>

#include <memory>
#define NOMINMAX
#include <windows.h>
#include <psapi.h>

//--- HELPER ---//
static size_t GetProcessMemoryMB()
{
	PROCESS_MEMORY_COUNTERS_EX pmc{};
	GetProcessMemoryInfo(
		GetCurrentProcess(),
		(PROCESS_MEMORY_COUNTERS*)&pmc,
		sizeof(pmc)
	);
	 
	// Working Set = physical RAM currently used
	return pmc.WorkingSetSize / (1024 * 1024);
} // end of GetProcessMemoryMB()

//--- PUBLIC ---//
UI::UI(
	VulkanMain* vk, 
	GLFWwindow* window, 
	RenderSettings& rs, 
	Backend activeBackend
)
	: vk_(vk), 
	window_(window), 
	renderSettings_(rs),
	activeBackend_(activeBackend),
	selectedBackend_(activeBackend)
{

	// ------ imgui init ------ //
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	setDarkTheme();

	// vulkan
	if (vk_)
	{
		renderSettings_.enableVsync = vk_->getVSync();

		// window top nav bar logo
		logoTexVk_ = std::make_unique<Texture2DVk>(*vk_);
		logoTexVk_->loadFromFile("blocks.png", false);

		ImGui_ImplGlfw_InitForVulkan(window_, true);

		ImGui_ImplVulkan_InitInfo initInfo{};
		initInfo.Instance = vk_->getInstance();
		initInfo.PhysicalDevice = vk_->getPhysicalDevice();
		initInfo.Device = vk_->getDevice();
		initInfo.QueueFamily = vk_->getGraphicsQueueFamilyIndex();
		initInfo.Queue = vk_->getGraphicsQueue();
		initInfo.PipelineCache = VK_NULL_HANDLE;
		initInfo.DescriptorPool = vk_->getImGuiDescriptorPool();
		initInfo.DescriptorPoolSize = 0;
		initInfo.MinImageCount = vk_->getMinImageCount();
		initInfo.ImageCount = vk_->getSwapchainImageCount();
		initInfo.Allocator = nullptr;
		initInfo.CheckVkResultFn = nullptr;

		initInfo.UseDynamicRendering = true;
		initInfo.PipelineInfoMain.Subpass = 0;
		initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

		vk::Format colorFormat = vk_->getSwapChainImageFormat();

		vk::PipelineRenderingCreateInfoKHR pipelineRenderingInfo{};
		pipelineRenderingInfo.colorAttachmentCount = 1;
		pipelineRenderingInfo.pColorAttachmentFormats = &colorFormat;
		pipelineRenderingInfo.depthAttachmentFormat = vk_->getDepthFormat();
		pipelineRenderingInfo.stencilAttachmentFormat = vk::Format::eUndefined;

		initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = pipelineRenderingInfo;

		ImGui_ImplVulkan_Init(&initInfo);

		// logo
		logoIdVk_ = reinterpret_cast<ImTextureID>(
			ImGui_ImplVulkan_AddTexture(
				logoTexVk_->sampler(),
				logoTexVk_->view(),
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			)
		);
	}
	// opengl
	else
	{
		// window top nav bar logo
		logoTexGL_ = std::make_unique<Texture>("blocks.png");

		ImGui_ImplGlfw_InitForOpenGL(window_, true);
		ImGui_ImplOpenGL3_Init("#version 460 core");
	}
} // end of constructor

UI::~UI()
{
	// imgui shutdown
	if (vk_)
	{
		ImGui_ImplVulkan_Shutdown();
	}
	else
	{
		ImGui_ImplOpenGL3_Shutdown();
	}
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
} // end of destructor

void UI::beginFrame()
{
	if (vk_)
	{
		ImGui_ImplVulkan_NewFrame();
	}
	else
	{
		ImGui_ImplOpenGL3_NewFrame();
	}
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
} // end of beginFrame()

void UI::buildUI(float dt, IScene& scene)
{
	if (!vk_)
	{
		glDisable(GL_FRAMEBUFFER_SRGB);
	}

	drawTitleBar();
	drawMenuBar(scene);

	if (statsEnabled_)
	{
		drawStatsFPS(scene, dt);
	}
	if (inspectorEnabled_)
	{
		drawInspector(scene);
	}

	ImGui::Render();
} // end of drawFullUI()

void UI::renderGL()
{
	if (!vk_)
	{
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			GLFWwindow* backup = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup);
		}
	}
} // end of renderGL()

void UI::renderVk(FrameContext& frame)
{
	// vulkan
	if (vk_)
	{
		vk::CommandBuffer cmd = frame.cmd;

		vk::RenderingAttachmentInfo uiColorAttach{};
		uiColorAttach.imageView = frame.colorImageView;
		uiColorAttach.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
		uiColorAttach.loadOp = vk::AttachmentLoadOp::eLoad;
		uiColorAttach.storeOp = vk::AttachmentStoreOp::eStore;

		vk::RenderingInfo uiRenderingInfo{};
		uiRenderingInfo.renderArea.offset = vk::Offset2D{ 0, 0 };
		uiRenderingInfo.renderArea.extent = frame.extent;
		uiRenderingInfo.layerCount = 1;
		uiRenderingInfo.colorAttachmentCount = 1;
		uiRenderingInfo.pColorAttachments = &uiColorAttach;
		uiRenderingInfo.pDepthAttachment = nullptr;
		cmd.beginRendering(uiRenderingInfo);
		{
			ImGui_ImplVulkan_RenderDrawData(
				ImGui::GetDrawData(),
				cmd
			);
		}
		cmd.endRendering();
	}
} // end of renderVk()

std::string_view UI::backendToString(Backend backend) const
{
	switch (backend)
	{
	case Backend::OpenGL: return "OpenGL";
	case Backend::Vulkan: return "Vulkan";
	case Backend::DX12:   return "DX12";
	default:              return "Unknown";
	}
} // end of backendToString()

void UI::setActiveBackend(Backend backend)
{
	activeBackend_ = backend;
	selectedBackend_ = backend;
} // end of setActiveBackend()

bool UI::applyBackendRequest(Backend& outBackend)
{
	if (!backendApplyRequested_)
	{
		return false;
	}

	backendApplyRequested_ = false;
	outBackend = selectedBackend_;
	return true;
} // end of applyBackendRequest()

void UI::onSwapchainRecreated()
{
	if (vk_)
	{
		ImGui_ImplVulkan_SetMinImageCount(vk_->getMinImageCount());
	}
} // end of onSwapchainRecreated()


//--- PRIVATE ---//
void UI::drawTitleBar()
{
	ImGuiViewport* vp = ImGui::GetMainViewport();

	const float barHeight = 30.0f;
	const float padding = 8.0f;
	const float btnSize = 18.0f;

	ImGui::SetNextWindowPos(vp->Pos);
	ImGui::SetNextWindowSize(ImVec2(vp->Size.x, barHeight));

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoSavedSettings;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, 4));
	ImGui::Begin("##TitleBar", nullptr, flags);
	ImGui::PopStyleVar();

	// ----- logo -----
	float h = barHeight - 4.0f;
	float logoWidth = vk_
		? static_cast<float>(logoTexVk_->width())
		: static_cast<float>(logoTexGL_->getWidth());
	float logoHeight = vk_
		? static_cast<float>(logoTexVk_->height())
		: static_cast<float>(logoTexGL_->getHeight());
	float aspect = logoWidth / logoHeight;

	if (vk_)
		ImGui::Image(logoIdVk_, ImVec2(h * aspect, h));
	else
		ImGui::Image((void*)(intptr_t)logoTexGL_->ID(), ImVec2(h * aspect, h));

	ImGui::SameLine();

	// ----- title -----
	std::string title = "Project Atlas - " + std::string(backendToString(activeBackend_));
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(title.c_str());

	ImGui::SameLine();

	// ----- right-aligned buttons -----
	float right = ImGui::GetWindowContentRegionMax().x;
	ImGui::SetCursorPosX(right - (btnSize * 3 + padding * 2));

	if (ImGui::Button("_", ImVec2(btnSize, btnSize)))
		glfwIconifyWindow(window_);

	ImGui::SameLine();

	if (ImGui::Button("[]", ImVec2(btnSize, btnSize)))
	{
		if (glfwGetWindowAttrib(window_, GLFW_MAXIMIZED))
			glfwRestoreWindow(window_);
		else
			glfwMaximizeWindow(window_);
	}

	ImGui::SameLine();

	if (ImGui::Button("X", ImVec2(btnSize, btnSize)))
		glfwSetWindowShouldClose(window_, true);

	// ----- dragging -----
	if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
	{
		double dx = ImGui::GetIO().MouseDelta.x;
		double dy = ImGui::GetIO().MouseDelta.y;

		int wx, wy;
		glfwGetWindowPos(window_, &wx, &wy);
		glfwSetWindowPos(window_, wx + (int)dx, wy + (int)dy);
	}

	ImGui::End();
} // end of drawTitleBar()

void UI::drawMenuBar(IScene& scene)
{
	ImGuiViewport* vp = ImGui::GetMainViewport();

	const float titleBarHeight = 30.0f;
	const float menuBarHeight = 26.0f;
	const float padding = 8.0f;

	ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + titleBarHeight));
	ImGui::SetNextWindowSize(ImVec2(vp->Size.x, menuBarHeight));

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_MenuBar;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, 2));
	ImGui::Begin("##MenuBarWindow", nullptr, flags);
	ImGui::PopStyleVar();

	if (ImGui::BeginMenuBar())
	{
		// VIEW OPTIONS
		if (ImGui::BeginMenu("View"))
		{
			ImGui::MenuItem("Inspector", nullptr, &inspectorEnabled_);
			ImGui::MenuItem("Stats", nullptr, &statsEnabled_);
			ImGui::EndMenu();
		}

		// DISPLAY OPTIONS
		if (ImGui::BeginMenu("Display"))
		{
			if (vk_)
			{
				std::string vsyncLabel = "VSync [" + vk::to_string(vk_->getVsyncMode()) + "]";
				if (ImGui::Checkbox(vsyncLabel.c_str(), &renderSettings_.enableVsync))
				{
					vk_->setVSync(renderSettings_.enableVsync);
				}
			}
			else
			{
				if (ImGui::Checkbox("VSync", &renderSettings_.enableVsync))
				{
					glfwSwapInterval(renderSettings_.enableVsync);
				}
			}

			ImGui::EndMenu();
		}

		// GRAPHICS OPTIONS
		if (ImGui::BeginMenu("Graphics"))
		{
			if (ImGui::Checkbox("Shadows##graphics", &renderSettings_.useShadowMap))
			{
			}
			if (ImGui::Checkbox("SSAO##graphics", &renderSettings_.useSSAO))
			{
			}
			if (ImGui::Checkbox("FXAA##graphics", &renderSettings_.useFXAA))
			{
			}
			if (ImGui::Checkbox("Fog##graphics", &renderSettings_.useFog))
			{
			}
			ImGui::EndMenu();
		}

		// RENDERER OPTIONS
		if (ImGui::BeginMenu("Renderer"))
		{
			// API selection
			if (ImGui::BeginMenu("API"))
			{
				if (ImGui::Selectable("OpenGL", selectedBackend_ == Backend::OpenGL, ImGuiSelectableFlags_DontClosePopups))
					selectedBackend_ = Backend::OpenGL;

				if (ImGui::Selectable("Vulkan", selectedBackend_ == Backend::Vulkan, ImGuiSelectableFlags_DontClosePopups))
					selectedBackend_ = Backend::Vulkan;

				if (selectedBackend_ != activeBackend_)
				{
					ImGui::Separator();

					if (ImGui::MenuItem("Apply Backend Change"))
					{
						backendApplyRequested_ = true;
					}

					if (ImGui::MenuItem("Cancel Backend Change"))
					{
						selectedBackend_ = activeBackend_;
					}
				}
				ImGui::EndMenu();
			}

			// Culling selection
			if (ImGui::BeginMenu("Culling"))
			{
				ChunkManager& world = scene.getWorld();
				bool frustumCulling = world.statusFrustumCulling();
				if (ImGui::Checkbox("Frustum Culling##render", &frustumCulling))
				{
					world.enableFrustumCulling(frustumCulling);
				}
				bool distanceCulling = world.statusDistanceCulling();
				if (ImGui::Checkbox("Distance Culling##render", &distanceCulling))
				{
					world.enableDistanceCulling(distanceCulling);
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
	ImGui::End();
} // end of drawMenuBar()

void UI::drawStatsFPS(IScene& scene, float dt)
{
	ImGuiViewport* vp = ImGui::GetMainViewport();

	const float padding = 10.0f;

	const float renderLeft = vp->Pos.x + INSPECTOR_WIDTH;
	const float renderTop = vp->Pos.y + TOP_BAR_HEIGHT*2;
	const float renderRight = vp->Pos.x + vp->Size.x;

	ImVec2 anchor = ImVec2(renderRight - padding, renderTop + padding);

	ImGui::SetNextWindowViewport(vp->ID);
	ImGui::SetNextWindowPos(anchor, ImGuiCond_Always, ImVec2(1.0f, 0.0f));

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNav;

	if (ImGui::Begin("##StatsOverlay", nullptr, flags))
	{
		float ms = dt * 1000.0f;
		float fps = (dt > 0.0f) ? (1.0f / dt) : 0.0f;

		ImGui::Text("FPS: %.1f", fps);
		ImGui::Text("Frametime: %.3f ms", ms);

		ImGui::Separator();
		ImGui::Text("RAM (Working Set): %zu MB", GetProcessMemoryMB());

		ImGui::Separator();
		
		// vulkan
		if (vk_)
		{
			vk::PhysicalDeviceProperties props = vk_->getPhysicalDeviceProperties();
			ImGui::Text("Device: %s", props.deviceName);
		}
		// opengl
		else
		{
			ImGui::Text("Device: %s", glGetString(GL_RENDERER));
		}
	}

	ImGui::Separator();

	ChunkManager& world = scene.getWorld();
	ImGui::Text("Chunks Rendered: %d", world.getFrameChunksRendered());
	ImGui::Text("Blocks Rendered: %d", world.getFrameBlocksRendered());
	ImGui::End();
} // end of drawStatsFPS()

void UI::drawInspector(IScene& scene)
{
	ImGuiViewport* vp = ImGui::GetMainViewport();

	ImVec2 pos = ImVec2(vp->Pos.x, vp->Pos.y + TOP_BAR_HEIGHT*2);
	ImVec2 size = ImVec2(INSPECTOR_WIDTH, vp->Size.y - TOP_BAR_HEIGHT*2);

	ImGui::SetNextWindowViewport(vp->ID);
	ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(size, ImGuiCond_Always);

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings;

	ImGui::Begin("Inspector", nullptr, flags);

	// ------- renderer -------
#ifdef _DEBUG
	if (ImGui::CollapsingHeader("Renderer", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// render mode
		std::string_view mode = "ERROR!";

		switch (renderSettings_.debugMode)
		{
		case DebugMode::None:
			mode = "Default";
			break;
		case DebugMode::Normals:
			mode = "Normals";
			break;
		case DebugMode::Depth:
			mode = "Depth";
			break;
		case DebugMode::ShadowMap:
			mode = "Shadow Map";
			break;
		default:
			break;
		}
		ImGui::Text("Render Mode:\n %s\n", mode.data());

		if (vk_)
		{
			ImGui::Text("Swapchain Image Format:\n %s", vk::to_string(vk_->getSwapChainImageFormat()).data());
		}
		ImGui::Separator();
	}
#endif

	// ------- camera -------
	if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
	{
		Camera& camera = scene.getCamera();
		float movementSpeed = camera.getMovementSpeed();
		glm::vec3 pos = camera.getCameraPosition();
		float fp = camera.getFarPlane();
		 
		bool changed = false;

		changed |= ImGui::DragFloat("Movement Speed##cam", &movementSpeed, 0.1f);
		if (ImGui::Button("Reset##cam_mov"))
		{
			camera.setMovementSpeed(camera.MIN_MOVESPEED);
		}
		changed |= ImGui::DragFloat3("Position##cam", glm::value_ptr(pos), 0.1f);
		if (ImGui::Button("Reset##cam_pos"))
		{
			pos = glm::vec3(0.0f, 100.0f, 3.0f);
			camera.setCameraPosition(pos);
		}
		changed |= ImGui::DragFloat("Far Plane##cam", &fp, 5.0f);
		if (ImGui::Button("Reset##cam_fp"))
		{
			camera.setFarPlane(camera.MIN_FARPLANE);
		}

		if (changed)
		{
			camera.setMovementSpeed(movementSpeed);
			camera.setCameraPosition(pos);
			camera.setFarPlane(fp);
		}
		ImGui::Separator();
	}

	// ------- world -------
	if (ImGui::CollapsingHeader("World", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bool changed = false;

		ChunkManager& world = scene.getWorld();
		float ambientStrength = world.getAmbientStrength();
		changed |= ImGui::DragFloat("Ambient Strength##world", &ambientStrength, 0.01f);
		if (ImGui::Button("Reset##amb"))
		{
			ambientStrength = World::MAX_AMBSTR;
			world.setAmbientStrength(ambientStrength);
		}
		int renderRadius = world.getViewRadius();
		changed |= ImGui::DragInt("Render Radius##world", &renderRadius, 1);
		if (ImGui::Button("Reset##radius"))
		{
			renderRadius = World::MIN_RADIUS;
			world.setViewRadius(renderRadius);
		}

		if (changed)
		{
			world.setAmbientStrength(ambientStrength);
			world.setViewRadius(renderRadius);
		}

		ImGui::Separator();
	}

	// ------- light -------
	if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ILight& light = scene.getLight();
		float speed = light.getSpeed();
		glm::vec3 color = light.getColor();

		bool changed = false;

		changed |= ImGui::DragFloat("Direction Speed##light", &speed, 0.01f);
		if (ImGui::Button("Reset##dirSpeed"))
		{
			light.setSpeed(0.1f);
		}
		changed |= ImGui::ColorEdit3("Color##light", glm::value_ptr(color));
		if (ImGui::Button("Reset##Color"))
		{
			light.setColor(glm::vec3(Light_Constants::MAX_COLOR));
		}

		if (changed)
		{
			light.setSpeed(speed);
			light.setColor(color);
		}

		ImGui::Separator();
	}

	// ------- fog -------
	if (ImGui::CollapsingHeader("Fog", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bool changed = false;

		changed |= ImGui::ColorEdit3("Color##fog", glm::value_ptr(renderSettings_.fogSettings.color));
		if (ImGui::Button("Reset##fog_color"))
		{
			renderSettings_.fogSettings.color = glm::vec3{ 1.0f, 1.0f, 1.0f };
		}
		changed |= ImGui::DragFloat("Start Pos##fog", &renderSettings_.fogSettings.start, 0.1f, 0.0f, renderSettings_.fogSettings.end);
		if (ImGui::Button("Reset##fog_start"))
		{
			renderSettings_.fogSettings.start = 50.0f;
		}
		changed |= ImGui::DragFloat("End Pos##fog", &renderSettings_.fogSettings.end, 0.1f, renderSettings_.fogSettings.start, 2000.0f);
		if (ImGui::Button("Reset##fog_end"))
		{
			renderSettings_.fogSettings.end = 200.0f;
		}

		// ensure start + kMinGap <= end ALWAYS
		if (changed)
		{
			const float kMinGap = 100.0f;
			const float minFogStart = 25.0f;
			if (renderSettings_.fogSettings.start < minFogStart)
				renderSettings_.fogSettings.start = minFogStart;

			if (renderSettings_.fogSettings.start > renderSettings_.fogSettings.end - kMinGap)
			{
				renderSettings_.fogSettings.start = std::max(minFogStart, renderSettings_.fogSettings.end - kMinGap);
				renderSettings_.fogSettings.end = renderSettings_.fogSettings.start + kMinGap;
			}
		}
		ImGui::Separator();
	}

	ImGui::End();
} // end of drawInspector()

void UI::setDarkTheme()
{
	ImGuiStyle& style = ImGui::GetStyle();

	style.WindowMinSize = ImVec2(160, 20);
	style.FramePadding = ImVec2(4, 2);
	style.ItemSpacing = ImVec2(6, 2);
	style.ItemInnerSpacing = ImVec2(6, 4);
	style.Alpha = 0.95f;
	style.WindowRounding = 4.0f;
	style.FrameRounding = 2.0f;
	style.IndentSpacing = 6.0f;
	style.ItemInnerSpacing = ImVec2(2, 4);
	style.ColumnsMinSpacing = 50.0f;
	style.GrabMinSize = 14.0f;
	style.GrabRounding = 16.0f;
	style.ScrollbarSize = 12.0f;
	style.ScrollbarRounding = 16.0f;

	style.Colors[ImGuiCol_Text] = ImVec4(0.86f, 0.93f, 0.89f, 0.78f);
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.86f, 0.93f, 0.89f, 0.28f);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
	style.Colors[ImGuiCol_Border] = ImVec4(0.31f, 0.31f, 1.00f, 0.00f);
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.20f, 0.22f, 0.27f, 0.75f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.20f, 0.22f, 0.27f, 0.47f);
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.09f, 0.15f, 0.16f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.71f, 0.22f, 0.27f, 1.00f);
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.47f, 0.77f, 0.83f, 0.14f);
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_Button] = ImVec4(0.47f, 0.77f, 0.83f, 0.14f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.86f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.92f, 0.18f, 0.29f, 0.76f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.86f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_Separator] = ImVec4(0.14f, 0.16f, 0.19f, 1.00f);
	style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
	style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.47f, 0.77f, 0.83f, 0.04f);
	style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
	style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_PlotLines] = ImVec4(0.86f, 0.93f, 0.89f, 0.63f);
	style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.86f, 0.93f, 0.89f, 0.63f);
	style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.92f, 0.18f, 0.29f, 0.43f);
	style.Colors[ImGuiCol_PopupBg] = ImVec4(0.20f, 0.22f, 0.27f, 0.9f);
} // end of setDarkTheme();