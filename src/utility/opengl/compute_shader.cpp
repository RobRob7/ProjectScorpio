#include "compute_shader.h"

#include <glad/glad.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <utility>
#include <filesystem>
#include <stdexcept>
#include <unordered_set>
#include <string>

//--- HELPER ---//
static std::string LoadShaderWithIncludes(
	const std::filesystem::path& filePath,
	std::unordered_set<std::string>& includeStack
)
{
	std::filesystem::path absolutePath = std::filesystem::absolute(filePath).lexically_normal();
	std::string pathKey = absolutePath.string();

	if (includeStack.find(pathKey) != includeStack.end())
	{
		throw std::runtime_error("Circular shader include detected: " + pathKey);
	}

	includeStack.insert(pathKey);

	std::ifstream file(absolutePath);
	if (!file.is_open())
	{
		throw std::runtime_error("Failed to open shader file: " + pathKey);
	}

	std::stringstream stream;
	stream << file.rdbuf();

	std::string source = stream.str();
	std::stringstream output;


	std::stringstream sourceLines(source);
	std::string line;

	while (std::getline(sourceLines, line))
	{
		std::string trimmed = line;

		// remove leading whitespace
		trimmed.erase(0, trimmed.find_first_not_of(" \t"));

		if (trimmed.rfind("#include", 0) == 0)
		{
			size_t firstQuote = trimmed.find('"');
			size_t secondQuote = trimmed.find('"', firstQuote + 1);

			if (firstQuote == std::string::npos ||
				secondQuote == std::string::npos)
			{
				throw std::runtime_error(
					"Invalid #include syntax in shader: " + pathKey
				);
			}

			std::string includeFile =
				trimmed.substr(firstQuote + 1,
					secondQuote - firstQuote - 1);

			std::filesystem::path includePath =
				absolutePath.parent_path() / includeFile;

			output << LoadShaderWithIncludes(includePath, includeStack);
		}
		else
		{
			output << line << "\n";
		}
	}

	includeStack.erase(pathKey);
	return output.str();
} // end of LoadShaderWithIncludes()

static void CheckCompileErrors(uint32_t shader, std::string_view type, std::string_view path)
{
	GLint success;
	GLchar infoLog[1024];

	if (type != "PROGRAM")
	{
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (!success)
		{
			glGetShaderInfoLog(shader, 1024, NULL, infoLog);
			throw std::runtime_error("ERROR::SHADER_COMPILATION_ERROR of type: " + std::string(type) + "-" + std::string(path) + "\n" + infoLog + "\n -- --------------------------------------------------- -- ");
		}
	}
	else
	{
		glGetProgramiv(shader, GL_LINK_STATUS, &success);
		if (!success)
		{
			glGetProgramInfoLog(shader, 1024, NULL, infoLog);
			throw std::runtime_error("ERROR::PROGRAM_LINKING_ERROR of type: " + std::string(type) + "\n" + infoLog + "\n -- --------------------------------------------------- -- ");
		}
	}
} // end of CheckCompileErrors()


//--- PUBLIC ---//
ComputeShader::ComputeShader(const char* computePath)
{
	std::string computeCode;
	std::ifstream computeShaderFile;

	// will contain the problem file trying to open
	std::string problemFile{};

	// ensure ifstream objects can throw exceptions
	computeShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

	try
	{
		std::filesystem::path pathToShaders =
			std::filesystem::path(RESOURCES_PATH) / "shader";

		std::filesystem::path computeFullPath =
			pathToShaders / computePath;

		std::unordered_set<std::string> includeStack;

		problemFile = "COMPUTE";
		computeCode = LoadShaderWithIncludes(
			computeFullPath,
			includeStack
		);
	}
	catch (const std::exception& e)
	{
		throw std::runtime_error(
			"ERROR::" + problemFile +
			"_SHADER::FILE_NOT_SUCCESSFULLY_READ: " +
			std::string(e.what())
		);
	}

	// contains the shader code
	const char* computeShaderCode = computeCode.c_str();

	// will contain id for shader
	uint32_t compute;

	// compute shader compile
	compute = glCreateShader(GL_COMPUTE_SHADER);
	glShaderSource(compute, 1, &computeShaderCode, NULL);
	glCompileShader(compute);
	CheckCompileErrors(compute, "COMPUTE", computePath);

	// create shader program
	id_ = glCreateProgram();

	// attach shader to program
	glAttachShader(id_, compute);

	// link program
	glLinkProgram(id_);
	CheckCompileErrors(id_, "PROGRAM", "");

	// delete the shaders (no longer needed)
	glDeleteShader(compute);
} // end of constructor

ComputeShader::~ComputeShader()
{
	if (id_ != 0)
	{
		glDeleteProgram(id_);
		id_ = 0;
	}
} // end of destructor

void ComputeShader::use() const
{
	if (id_)
	{
		glUseProgram(id_);
	}
} // end of use()
