#ifndef UI_H
#define UI_H

#include "constants.h"

#include <imgui.h>

#include <memory>
#include <string_view>

class IScene;
class Texture;
class Texture2DVk;
class VulkanMain;
struct FrameContext;
struct GLFWwindow;
struct RenderSettings;

class UI
{
public:
	UI(
		VulkanMain* vk,
		GLFWwindow* window, 
		RenderSettings& rs, 
		Backend activeBackend
	);
	~UI();

	void beginFrame();
	void buildUI(float dt, IScene& scene);

	void renderGL();
	void renderVk(FrameContext& frame);

	std::string_view backendToString(Backend backend) const;
	void setActiveBackend(Backend backend);
	bool applyBackendRequest(Backend& outBackend);

	void onSwapchainRecreated();

private:
	void drawTitleBar();
	void drawMenuBar(IScene& scene);
	void drawStatsFPS(IScene& scene, float dt);
	void drawInspector(IScene& scene);
	void setDarkTheme();
private:
	Backend activeBackend_;
	Backend selectedBackend_;
	bool backendApplyRequested_{ false };

	VulkanMain* vk_{ nullptr };

	GLFWwindow* window_;
	RenderSettings& renderSettings_;

	std::unique_ptr<Texture> logoTexGL_;

	std::unique_ptr<Texture2DVk> logoTexVk_;
	ImTextureID logoIdVk_;

	bool inspectorEnabled_{ true };
	bool statsEnabled_{ true };
};

#endif
