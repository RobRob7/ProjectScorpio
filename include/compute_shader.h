#ifndef COMPUTE_SHADER_H
#define COMPUTE_SHADER_H

#include <cstdint>

class ComputeShader
{
public:
	ComputeShader(const char* computePath);
	~ComputeShader();

	ComputeShader(const ComputeShader&) = delete;
	ComputeShader& operator=(const ComputeShader&) = delete;

	ComputeShader(ComputeShader&& other) = delete;
	ComputeShader& operator=(ComputeShader&& other) = delete;

	void use() const;

private:
	uint32_t id_{};
};

#endif
