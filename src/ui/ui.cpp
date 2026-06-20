#include "ui.h"

#include "dx12_main.h"

#include "vulkan_main.h"
#include "render_settings.h"
#include "frame_context_vk.h"

#include "i_scene.h"
#include "i_light.h"

#include "texture_2d_vk.h"
#include "texture_gl.h"

#include "chunk_manager.h"
#include "camera.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_dx12.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vulkan/vulkan.hpp>

#include <memory>
#include <filesystem>

#if defined(_WIN32)
	#define NOMINMAX
	#include <windows.h>
	#include <psapi.h>
#elif defined(__linux__)
	#include <cstdio>
	#include <unistd.h>
#endif

//--- HELPER ---//
static size_t GetProcessMemoryMB()
{
#if defined(_WIN32)
	// WINDOWS
	PROCESS_MEMORY_COUNTERS_EX pmc{};
	GetProcessMemoryInfo(
		GetCurrentProcess(),
		(PROCESS_MEMORY_COUNTERS*)&pmc,
		sizeof(pmc)
	);
	return pmc.WorkingSetSize / (1024 * 1024);

#elif defined(__linux__)
	// LINUX
	long rss = 0;
	FILE* fp = std::fopen("/proc/self/statm", "r");
	if (!fp) return 0;

	long pages = 0;
	if (std::fscanf(fp, "%*s%ld", &pages) == 1)
		rss = pages * sysconf(_SC_PAGESIZE);

	std::fclose(fp);
	return static_cast<size_t>(rss) / (1024 * 1024);

#else
	return 0;
#endif
} // end of GetProcessMemoryMB()

//--- PUBLIC ---//
UI::UI(
	VulkanMain* vk, 
	GLFWwindow* window, 
	RenderSettings& rs, 
	Backend activeBackend,
	DX12Main* dx
)
	: vk_(vk),
	dx_(dx),
	window_(window), 
	renderSettings_(rs),
	activeBackend_(activeBackend),
	selectedBackend_(activeBackend)
{

	// ------ imgui init ------ //
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();

	std::filesystem::path path = std::filesystem::path(RESOURCES_PATH) / 
		"fonts/Open_Sans/OpenSans-VariableFont_wdth,wght.ttf";
	io.Fonts->AddFontFromFileTTF(
		path.string().c_str(),
		22.0f
	);
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	setDarkTheme();

	// DX12
	if (dx_)
	{
		renderSettings_.enableVsync = dx_->getVSync();

		ImGui_ImplGlfw_InitForOther(window_, true);

		ImGui_ImplDX12_InitInfo initInfo{};
		initInfo.Device = dx_->getDevice();
		initInfo.CommandQueue = dx_->getGraphicsQueue();
		initInfo.NumFramesInFlight = dx_->getMaxFramesInFlight();
		initInfo.RTVFormat = dx_->getSwapChainImageFormat();
		initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
		initInfo.SrvDescriptorHeap = dx_->getImguiSrvHeap();

		initInfo.SrvDescriptorAllocFn =
			[](ImGui_ImplDX12_InitInfo* info,
				D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle,
				D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle)
			{
				DX12Main* dx = static_cast<DX12Main*>(info->UserData);
				dx->allocateImGuiDescriptor(*outCpuHandle, *outGpuHandle);
			};

		initInfo.SrvDescriptorFreeFn =
			[](ImGui_ImplDX12_InitInfo* info,
				D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
				D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle)
			{
				DX12Main* dx = static_cast<DX12Main*>(info->UserData);
				dx->freeImGuiDescriptor(cpuHandle, gpuHandle);
			};

		initInfo.UserData = dx_;

		ImGui_ImplDX12_Init(&initInfo);

		// logo
		
	}
	// vulkan
	else if (vk_)
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
		logoTexGL_ = std::make_unique<TextureGL>("blocks.png");

		ImGui_ImplGlfw_InitForOpenGL(window_, true);
		ImGui_ImplOpenGL3_Init("#version 460 core");
	}
} // end of constructor

UI::~UI()
{
	if (dx_)
	{
		ImGui_ImplDX12_Shutdown();
	}
	else if (vk_)
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
	if (dx_)
	{
		ImGui_ImplDX12_NewFrame();
	}
	else if (vk_)
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
	if (!vk_ && !dx_)
	{
		glDisable(GL_FRAMEBUFFER_SRGB);
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	drawTitleBar();
	drawMenuBar(scene);
	ImGui::PopStyleVar();

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
	if (!vk_ && !dx_)
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

		cmd.beginDebugUtilsLabelEXT({ "UI::cmd" });

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

		cmd.endDebugUtilsLabelEXT();
	}
} // end of renderVk()

void UI::renderDX12(ID3D12GraphicsCommandList* cmd)
{
	if (!dx_) return;

	ID3D12DescriptorHeap* heaps[] =
	{
		dx_->getImguiSrvHeap()
	};

	cmd->SetDescriptorHeaps(1, heaps);

	ImGui_ImplDX12_RenderDrawData(
		ImGui::GetDrawData(),
		cmd
	);
} // end of renderDX12()

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
	ImGui::SetNextWindowBgAlpha(1.0f);
	ImGuiViewport* vp = ImGui::GetMainViewport();

	const float padding = 8.0f;
	const float btnSize = ImGui::GetFrameHeight();
	const float barHeight = btnSize + 8.0f;

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
	if (vk_)
	{
		float h = barHeight - 4.0f;
		float logoWidth = static_cast<float>(logoTexVk_->width());
		float logoHeight = static_cast<float>(logoTexVk_->height());
		float aspect = logoWidth / logoHeight;

		ImGui::Image(logoIdVk_, ImVec2(h * aspect, h));
		ImGui::SameLine();
	}
	else if (!dx_)
	{
		float h = barHeight - 4.0f;
		float logoWidth = static_cast<float>(logoTexGL_->getWidth());
		float logoHeight = static_cast<float>(logoTexGL_->getHeight());
		float aspect = logoWidth / logoHeight;

		ImGui::Image((void*)(intptr_t)logoTexGL_->ID(), ImVec2(h * aspect, h));
		ImGui::SameLine();
	}

	// ----- title -----
	std::string title = "Project Scorpio - " + std::string(backendToString(activeBackend_));
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(title.c_str());

	ImGui::SameLine();

	// ----- right-aligned buttons -----
	float right = ImGui::GetWindowContentRegionMax().x;
	ImGui::SetCursorPosX(right - (btnSize * 3 + padding * 2));

	if (ImGui::Button("-", ImVec2(btnSize, btnSize)))
		glfwIconifyWindow(window_);

	ImGui::SameLine();

	if (ImGui::Button("##max", ImVec2(btnSize, btnSize)))
	{
		if (glfwGetWindowAttrib(window_, GLFW_MAXIMIZED))
			glfwRestoreWindow(window_);
		else
			glfwMaximizeWindow(window_);
	}

	ImVec2 min = ImGui::GetItemRectMin();
	ImVec2 max = ImGui::GetItemRectMax();
	ImDrawList* draw = ImGui::GetWindowDrawList();

	float pad = btnSize * 0.30f;
	draw->AddRect(
		ImVec2(min.x + pad, min.y + pad),
		ImVec2(max.x - pad, max.y - pad),
		ImGui::GetColorU32(ImGuiCol_Text),
		0.0f,
		0,
		1.5f
	);

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
	ImGui::SetNextWindowBgAlpha(1.0f);

	ImGuiViewport* vp = ImGui::GetMainViewport();

	const float titleBarHeight = ImGui::GetFrameHeight() + 8.0f;
	const float menuBarHeight = ImGui::GetFrameHeight() + 4.0f;
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
		// GENERAL OPTIONS
		if (ImGui::BeginMenu("Options"))
		{
			// font size
			ImGuiIO& io = ImGui::GetIO();

			static float fontScale = 1.0f;
			if (ImGui::SliderFloat(
				"Font Scale",
				&fontScale,
				0.75f,
				1.25f,
				"%.2f"
			))
			{
				io.FontGlobalScale = fontScale;
			}

			ImGui::EndMenu();
		}

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
			if (dx_)
			{
				if (ImGui::Checkbox("VSync", &renderSettings_.enableVsync))
				{
					dx_->setVSync(renderSettings_.enableVsync);
				}
			}
			else if (vk_)
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
		bool enabledRT = renderSettings_.useRT;
		const bool supportsRT = vk_ && vk_->supportsRayTracing();
		if (ImGui::BeginMenu("Graphics"))
		{
			if (vk_)
			{
				ImGui::BeginDisabled(!supportsRT);
				if (ImGui::Checkbox("RT Mode##graphics", &renderSettings_.useRT))
				{
				}
				ImGui::EndDisabled();

				if (!supportsRT)
				{
					ImGui::SameLine();
					ImGui::TextDisabled("(?)");

					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::PushTextWrapPos(ImGui::GetFontSize() * 22.0f);
						ImGui::TextUnformatted(
							"Ray tracing mode is unavailable because this GPU does not support "
							"the required Vulkan ray tracing extensions."
						);
						ImGui::PopTextWrapPos();
						ImGui::EndTooltip();
					}
				}
			}

			if (!enabledRT)
			{
				if (ImGui::Checkbox("Shadows##graphics", &renderSettings_.useShadowMap))
				{
				}
				if (ImGui::Checkbox("SSAO##graphics", &renderSettings_.useSSAO))
				{
				}
			}
			if (enabledRT)
			{
				if (ImGui::Checkbox("RT Shadows##graphics", &renderSettings_.useRTShadow))
				{
				}
				if (ImGui::Checkbox("RTAO##graphics", &renderSettings_.useRTAO))
				{
				}
			}
			if (ImGui::Checkbox("FXAA##graphics", &renderSettings_.useFXAA))
			{
			}
			if (ImGui::Checkbox("Volumetric Fog##graphics", &renderSettings_.useFog))
			{
			}
			if (ImGui::Checkbox("God Rays##graphics", &renderSettings_.useGodRays))
			{
			}
			ImGui::EndMenu();
		}

		// RENDERER OPTIONS
		if (ImGui::BeginMenu("Renderer"))
		{
			// Pass Resolution Scale
			if (ImGui::BeginMenu("Res Scale"))
			{
				constexpr float LABEL_WIDTH = 200.0f;

				// HYBRID OPTIONS
				{
					// fog scale
					{
						ImGui::Text("Fog Pass");
						ImGui::SameLine(LABEL_WIDTH);

						if (ImGui::SmallButton("-##Fog"))
						{
							renderSettings_.resScale.FOG =
								std::max(1u, renderSettings_.resScale.FOG - 1);
						}

						ImGui::SameLine();
						ImGui::Text("%u", renderSettings_.resScale.FOG);

						ImGui::SameLine();

						if (ImGui::SmallButton("+##Fog"))
						{
							renderSettings_.resScale.FOG =
								std::min(6u, renderSettings_.resScale.FOG + 1);
						}
					}
					// god ray scale
					{
						ImGui::Text("God Ray Pass");
						ImGui::SameLine(LABEL_WIDTH);

						if (ImGui::SmallButton("-##godray"))
						{
							renderSettings_.resScale.GOD_RAYS =
								std::max(1u, renderSettings_.resScale.GOD_RAYS - 1);
						}

						ImGui::SameLine();
						ImGui::Text("%u", renderSettings_.resScale.GOD_RAYS);

						ImGui::SameLine();

						if (ImGui::SmallButton("+##godray"))
						{
							renderSettings_.resScale.GOD_RAYS =
								std::min(6u, renderSettings_.resScale.GOD_RAYS + 1);
						}
					}
				}

				// RT OPTIONS
				{
					ImGui::BeginDisabled(!renderSettings_.useRT);
					// RT world scale
					{
						ImGui::Text("RTWorld Pass");
						ImGui::SameLine(LABEL_WIDTH);

						if (ImGui::SmallButton("-##RTWorld"))
						{
							renderSettings_.resScale.RT_WORLD =
								std::max(1u, renderSettings_.resScale.RT_WORLD - 1);
						}

						ImGui::SameLine();
						ImGui::Text("%u", renderSettings_.resScale.RT_WORLD);

						ImGui::SameLine();

						if (ImGui::SmallButton("+##RTWorld"))
						{
							renderSettings_.resScale.RT_WORLD =
								std::min(4u, renderSettings_.resScale.RT_WORLD + 1);
						}
					}
					// RTAO scale
					{
						ImGui::Text("RTAO Pass");
						ImGui::SameLine(LABEL_WIDTH);

						if (ImGui::SmallButton("-##RTAO"))
						{
							renderSettings_.resScale.RTAO =
								std::max(1u, renderSettings_.resScale.RTAO - 1);
						}

						ImGui::SameLine();
						ImGui::Text("%u", renderSettings_.resScale.RTAO);

						ImGui::SameLine();

						if (ImGui::SmallButton("+##RTAO"))
						{
							renderSettings_.resScale.RTAO =
								std::min(4u, renderSettings_.resScale.RTAO + 1);
						}
					}
					// RT Shadow scale
					{
						ImGui::Text("RTShadow Pass");
						ImGui::SameLine(LABEL_WIDTH);

						if (ImGui::SmallButton("-##RTShadow"))
						{
							renderSettings_.resScale.RT_SHADOW =
								std::max(1u, renderSettings_.resScale.RT_SHADOW - 1);
						}

						ImGui::SameLine();
						ImGui::Text("%u", renderSettings_.resScale.RT_SHADOW);

						ImGui::SameLine();

						if (ImGui::SmallButton("+##RTShadow"))
						{
							renderSettings_.resScale.RT_SHADOW =
								std::min(4u, renderSettings_.resScale.RT_SHADOW + 1);
						}
					}
				}
				ImGui::EndDisabled();

				// NON RT OPTIONS
				{
					ImGui::BeginDisabled(renderSettings_.useRT);
					// water scale
					{
						ImGui::Text("Water Pass");
						ImGui::SameLine(LABEL_WIDTH);

						if (ImGui::SmallButton("-##Water"))
						{
							renderSettings_.resScale.WATER =
								std::max(1u, renderSettings_.resScale.WATER - 1);
						}

						ImGui::SameLine();
						ImGui::Text("%u", renderSettings_.resScale.WATER);

						ImGui::SameLine();

						if (ImGui::SmallButton("+##Water"))
						{
							renderSettings_.resScale.WATER =
								std::min(6u, renderSettings_.resScale.WATER + 1);
						}
					}
					// SSAO scale
					{
						ImGui::Text("SSAO Pass");
						ImGui::SameLine(LABEL_WIDTH);

						if (ImGui::SmallButton("-##SSAO"))
						{
							renderSettings_.resScale.SSAO =
								std::max(1u, renderSettings_.resScale.SSAO - 1);
						}

						ImGui::SameLine();
						ImGui::Text("%u", renderSettings_.resScale.SSAO);

						ImGui::SameLine();

						if (ImGui::SmallButton("+##SSAO"))
						{
							renderSettings_.resScale.SSAO =
								std::min(6u, renderSettings_.resScale.SSAO + 1);
						}
					}
				}
				ImGui::EndDisabled();

				ImGui::EndMenu();
			}

			// API selection
			if (ImGui::BeginMenu("API"))
			{

				if (ImGui::Selectable("DX12", selectedBackend_ == Backend::DX12, ImGuiSelectableFlags_DontClosePopups))
					selectedBackend_ = Backend::DX12;

				if (ImGui::Selectable("Vulkan", selectedBackend_ == Backend::Vulkan, ImGuiSelectableFlags_DontClosePopups))
					selectedBackend_ = Backend::Vulkan;

				if (ImGui::Selectable("OpenGL", selectedBackend_ == Backend::OpenGL, ImGuiSelectableFlags_DontClosePopups))
					selectedBackend_ = Backend::OpenGL;

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

	float topBarsHeight =
		(ImGui::GetFrameHeight() + 8.0f) +
		(ImGui::GetFrameHeight() + 4.0f);

	float minWidth = 360.0f;
	float fontBasedWidth = ImGui::GetFontSize() * 22.0f;

	const float renderLeft = vp->Pos.x + std::max(minWidth, fontBasedWidth);
	const float renderTop = vp->Pos.y + topBarsHeight;
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
		static float timer = 0.0f;
		static float displayFps = 0.0f;
		static float displayMs = 0.0f;
		static float smoothedDt = 0.0f;

		if (smoothedDt <= 0.0f)
			smoothedDt = dt;

		smoothedDt += 0.08f * (dt - smoothedDt);

		timer += dt;

		// update fps, frametime every 100ms
		if (timer >= 0.1f)
		{
			displayMs = smoothedDt * 1000.0f;
			displayFps = 1.0f / smoothedDt;
			timer = 0.0f;
		}

		ImGui::Text("FPS: %.1f", displayFps);
		ImGui::Text("Frametime: %.3f ms", displayMs);

		ImGui::Separator();
		ImGui::Text("RAM (Working Set): %zu MB", GetProcessMemoryMB());

		ImGui::Separator();
		
		// DX12
		if (dx_)
		{
			ImGui::Text("Device: %s", dx_->getAdapterName().c_str());
		}
		// vulkan
		else if (vk_)
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

	float topBarsHeight =
		(ImGui::GetFrameHeight() + 8.0f) +
		(ImGui::GetFrameHeight() + 4.0f);

	float inspectorWidth = std::max(
		460.0f,
		ImGui::GetFontSize() * 26.0f
	);

	ImVec2 pos = ImVec2(vp->Pos.x, vp->Pos.y + topBarsHeight);
	ImVec2 size = ImVec2(inspectorWidth, vp->Size.y - topBarsHeight);

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
		case DebugMode::rtDepth:
			mode = "RT Depth";
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

	// ------- sun -------
	if (!dx_ && ImGui::CollapsingHeader("Sun", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ILight& light = scene.getLight();
		float speed = light.getSpeed();
		glm::vec3 color = light.getLightColor();

		if (ImGui::Checkbox("Pause", &renderSettings_.sunPaused))
		{
		}

		bool changed = false;

		changed |= ImGui::DragFloat("Direction Speed##sun", &speed, 0.005f);
		if (ImGui::Button("Reset##dirSpeed"))
		{
			light.setSpeed(0.05f);
		}
		changed |= ImGui::ColorEdit3("Color##sun", glm::value_ptr(color));
		if (ImGui::Button("Reset##Color"))
		{
			light.setLightColor(glm::vec3(Light_Constants::MAX_COLOR));
		}

		if (changed)
		{
			light.setSpeed(speed);
			light.setLightColor(color);
		}

		ImGui::Separator();
	}

	// ------- fog -------
	ImGui::BeginDisabled(!renderSettings_.useFog);
	if (ImGui::CollapsingHeader("Fog", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bool changed = false;
		changed |= ImGui::DragFloat("Max Ray Distance##fog", &renderSettings_.fogSettings.maxDistance, 0.1f, 0.0f, 200.0f);
		if (ImGui::Button("Reset##fog_maxdistance"))
		{
			renderSettings_.fogSettings.maxDistance = 100.0f;
		}
		changed |= ImGui::DragFloat("Step Size##fog", &renderSettings_.fogSettings.stepSize, 0.01f, 0.03f, 1.5f);
		if (ImGui::Button("Reset##fog_stepsize"))
		{
			renderSettings_.fogSettings.stepSize = 0.150f;
		}
		changed |= ImGui::DragFloat("Scattering Density##fog", &renderSettings_.fogSettings.scatteringDensity, 0.005f, 0.005f, 0.03f);
		if (ImGui::Button("Reset##fog_scatteringdensity"))
		{
			renderSettings_.fogSettings.scatteringDensity = 0.015f;
		}
		changed |= ImGui::DragFloat("Absorption Density##fog", &renderSettings_.fogSettings.absorptionDensity, 0.001f, 0.001f, 0.01f);
		if (ImGui::Button("Reset##fog_absorptiondensity"))
		{
			renderSettings_.fogSettings.absorptionDensity = 0.003f;
		}
		ImGui::Separator();
	}
	ImGui::EndDisabled();

	// ------- god rays -------
	ImGui::BeginDisabled(!renderSettings_.useGodRays);
	if (ImGui::CollapsingHeader("God Rays", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bool changed = false;
		changed |= ImGui::DragFloat("Max Ray Distance##godray", &renderSettings_.godRaySettings.maxDistance, 0.1f, 0.0f, 200.0f);
		if (ImGui::Button("Reset##godray_maxdistance"))
		{
			renderSettings_.godRaySettings.maxDistance = 100.0f;
		}
		changed |= ImGui::DragFloat("Step Size##godray", &renderSettings_.godRaySettings.stepSize, 0.01f, 0.03f, 1.5f);
		if (ImGui::Button("Reset##godray_stepsize"))
		{
			renderSettings_.godRaySettings.stepSize = 0.150f;
		}
		ImGui::Separator();
	}
	ImGui::EndDisabled();

	// ------- AO -------
	if (ImGui::CollapsingHeader("Ambient Occlusion", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bool changed = false;
		changed |= ImGui::DragInt("Samples##AO", &renderSettings_.aoSettings.samples, 1.0f, 1, 64);
		if (ImGui::Button("Reset##AOSamples"))
		{
			renderSettings_.aoSettings.samples = 16;
		}
		changed |= ImGui::DragFloat("Radius##AO", &renderSettings_.aoSettings.radius, 1.0f, 1.0f, 10.0f);
		if (ImGui::Button("Reset##AORadius"))
		{
			renderSettings_.aoSettings.radius = 2.0f;
		}
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