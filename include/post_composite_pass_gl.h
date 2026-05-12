#ifndef POST_COMPOSITE_PASS_GL_H
#define POST_COMPOSITE_PASS_GL_H

#include <glad/glad.h>

#include <cstdint>
#include <memory>

class Shader;

class PostCompositePassGL
{
public:
	PostCompositePassGL();
	~PostCompositePassGL();

	void init();
	void resize(int w, int h);

	void render();

	void setInput(
		const uint32_t& inputFogTex,
		const uint32_t& inputFXAATex
	)
	{
		fogColorImage_ = &inputFogTex;
		sceneColorImage_ = &inputFXAATex;
	} // end of setInput()

	const uint32_t& getOutColorImage() const { return postColorImage_; }

private:
	void destroyAttachment();
	void destroyGL();
	void refreshInput();
	void createAttachment();
private:
	int width_{};
	int height_{};

	const uint32_t* fogColorImage_{ nullptr };
	const uint32_t* sceneColorImage_{ nullptr };
	
	uint32_t postColorImage_{};
	GLenum postColorFormat_{ GL_RGBA16F };

	uint32_t fbo_{};
	uint32_t fsVao_{};

	std::unique_ptr<Shader> shader_;
};

#endif
