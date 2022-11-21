#pragma once
#include <string>
#include <vector>
#include <sstream>
#include "SDL_version.h"
#pragma warning(push)
#include SDL2_INCLUDE_DIR
#include SDL2_IMAGE_INCLUDE_DIR
#include SDL2_MIXER_INCLUDE_DIR
#include SDL2_NET_INCLUDE_DIR
#include SDL2_TTF_INCLUDE_DIR
#include SDL2_GPU_INCLUDE_DIR
#pragma warning(pop)

class Func
{
public:
	static bool FileExists(std::string FileName);

	// good for big data, for small use split
	static const std::vector<std::string> Explode(std::string& String, const char Delim);
	// good for small data, for big use explode
	static const std::vector<std::string> Split(std::string& String, const char Delim);

	static char* GetFileBuf(std::string file, Sint64* len);
	static unsigned char* GetFileBytes(std::string file, Sint64* len);

	static SDL_GLattr get_sdl_attr_from_string(std::string arg, bool* error);

	static Uint32 load_shader(GPU_ShaderEnum shader_type, const char* filename)
	{	
		Uint32 shader;
		Sint64 siz = 0;
		char* source = Func::GetFileBuf(std::string(filename), &siz);

		if (siz == 0) return -1;

		// Compile the shader
		shader = GPU_CompileShader(shader_type, source);
		free(source);
		return shader;

	}

	static GPU_ShaderBlock load_shader_program(Uint32* p, const char* vertex_shader_file, const char* fragment_shader_file)
	{
		Uint32 v, f;
		v = load_shader(GPU_VERTEX_SHADER, vertex_shader_file);

		if (!v)
			GPU_LogError("Failed to load vertex shader (%s): %s\n", vertex_shader_file, GPU_GetShaderMessage());

		f = load_shader(GPU_FRAGMENT_SHADER, fragment_shader_file);

		if (!f)
			GPU_LogError("Failed to load fragment shader (%s): %s\n", fragment_shader_file, GPU_GetShaderMessage());

		*p = GPU_LinkShaders(v, f);

		if (!*p)
		{
			GPU_ShaderBlock b = { -1, -1, -1, -1 };
			GPU_LogError("Failed to link shader program (%s + %s): %s\n", vertex_shader_file, fragment_shader_file, GPU_GetShaderMessage());
			return b;
		}

		{
			GPU_ShaderBlock block = GPU_LoadShaderBlock(*p, "gpu_Vertex", "gpu_TexCoord", "gpu_Color", "gpu_ModelViewProjectionMatrix");
			GPU_ActivateShaderProgram(*p, &block);

			return block;
		}
	}

	static void free_shader(Uint32 p)
	{
		GPU_FreeShaderProgram(p);
	}

};
