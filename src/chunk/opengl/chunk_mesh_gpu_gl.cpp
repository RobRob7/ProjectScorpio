#include "chunk_mesh_gpu_gl.h"

#include "chunk_mesh_data.h"

#include <glad/glad.h>

using namespace World;

//--- PUBLIC ---//
ChunkMeshGPUGL::ChunkMeshGPUGL()
{
	// OPAQUE
	// create VAO + buffers
	glCreateVertexArrays(1, &opaqueVao_);
	glCreateBuffers(1, &opaqueVbo_);
	glCreateBuffers(1, &opaqueEbo_);

	// attach buffers to vao
	glVertexArrayVertexBuffer(opaqueVao_, 0, opaqueVbo_, 0, sizeof(Vertex));
	glVertexArrayElementBuffer(opaqueVao_, opaqueEbo_);

	// combined data packed in int
	glEnableVertexArrayAttrib(opaqueVao_, 0);
	glVertexArrayAttribIFormat(opaqueVao_, 0, 1, GL_UNSIGNED_INT, offsetof(Vertex, sample));
	glVertexArrayAttribBinding(opaqueVao_, 0, 0);


	// WATER
	// create VAO + buffers
	glCreateVertexArrays(1, &waterVao_);
	glCreateBuffers(1, &waterVbo_);
	glCreateBuffers(1, &waterEbo_);

	// attach buffers to vao
	glVertexArrayVertexBuffer(waterVao_, 0, waterVbo_, 0, sizeof(VertexWater));
	glVertexArrayElementBuffer(waterVao_, waterEbo_);

	// position
	glEnableVertexArrayAttrib(waterVao_, 0);
	glVertexArrayAttribFormat(waterVao_, 0, 3, GL_FLOAT, GL_FALSE, offsetof(VertexWater, pos));
	glVertexArrayAttribBinding(waterVao_, 0, 0);
} // end of constructor

ChunkMeshGPUGL::~ChunkMeshGPUGL()
{
	if (opaqueVao_)
	{
		glDeleteVertexArrays(1, &opaqueVao_);
		opaqueVao_ = 0;
	}
	if (opaqueVbo_)
	{
		glDeleteBuffers(1, &opaqueVbo_);
		opaqueVbo_ = 0;
	}
	if (opaqueEbo_)
	{
		glDeleteBuffers(1, &opaqueEbo_);
		opaqueEbo_ = 0;
	}

	if (waterVao_)
	{
		glDeleteVertexArrays(1, &waterVao_);
		waterVao_ = 0;
	}
	if (waterVbo_)
	{
		glDeleteBuffers(1, &waterVbo_);
		waterVbo_ = 0;
	}
	if (waterEbo_)
	{
		glDeleteBuffers(1, &waterEbo_);
		waterEbo_ = 0;
	}
} // end of destructor

void ChunkMeshGPUGL::upload(
	vk::CommandBuffer,
	const ChunkMeshData& data
)
{
	// OPAQUE reupload into vbo
	glNamedBufferData(
		opaqueVbo_,
		data.opaqueVertices.size() * sizeof(Vertex),
		data.opaqueVertices.empty() ? nullptr : data.opaqueVertices.data(),
		GL_STATIC_DRAW
	);

	// reupload into ebo
	glNamedBufferData(
		opaqueEbo_,
		data.opaqueIndices.size() * sizeof(uint32_t),
		data.opaqueIndices.empty() ? nullptr : data.opaqueIndices.data(),
		GL_STATIC_DRAW
	);

	opaqueIndexCount_ = static_cast<int32_t>(data.opaqueIndices.size());


	// WATER reupload into vbo
	glNamedBufferData(
		waterVbo_,
		data.waterVertices.size() * sizeof(VertexWater),
		data.waterVertices.empty() ? nullptr : data.waterVertices.data(),
		GL_STATIC_DRAW
	);

	// reupload into ebo
	glNamedBufferData(
		waterEbo_,
		data.waterIndices.size() * sizeof(uint32_t),
		data.waterIndices.empty() ? nullptr : data.waterIndices.data(),
		GL_STATIC_DRAW
	);

	waterIndexCount_ = static_cast<int32_t>(data.waterIndices.size());
} // end of upload()

void ChunkMeshGPUGL::drawOpaque(vk::CommandBuffer cmd)
{
	if (opaqueIndexCount_ <= 0) return;

	const GLboolean wasDepthEnabled = glIsEnabled(GL_DEPTH_TEST);

	glEnable(GL_DEPTH_TEST);

	glBindVertexArray(opaqueVao_);
	glDrawElements(GL_TRIANGLES, opaqueIndexCount_, GL_UNSIGNED_INT, nullptr);
	glBindVertexArray(0);

	if (!wasDepthEnabled)
		glDisable(GL_DEPTH_TEST);
} // end of drawOpaque()

void ChunkMeshGPUGL::drawWater(vk::CommandBuffer cmd)
{
	if (waterIndexCount_ <= 0) return;

	const GLboolean wasDepthEnabled = glIsEnabled(GL_DEPTH_TEST);

	glEnable(GL_DEPTH_TEST);
	
	glBindVertexArray(waterVao_);
	glDrawElements(GL_TRIANGLES, waterIndexCount_, GL_UNSIGNED_INT, nullptr);
	glBindVertexArray(0);

	if (!wasDepthEnabled)
		glDisable(GL_DEPTH_TEST);
} // end of drawWater()