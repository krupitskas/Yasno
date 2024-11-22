#pragma once

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <optional>
#include <string>

#include <wil/com.h>
#include <dxcapi.h>
#include <d3d12.h>

#include <System/Result.hpp>

namespace ysn
{
	static constexpr char SHADER_PROFILE_VERSION[] = "6_6";

	enum class ShaderType
	{
		None,
		Library,
		Pixel,
		Vertex,
		Compute
	};

	struct ShaderCompileParameters
	{
		std::wstring shader_path		= L"";
		std::wstring entry_point		= L"main";
		bool disable_optimizations		= false;
		ShaderType shader_type			= ShaderType::None;
		std::vector<DxcDefine>	defines;
	};

	struct ShaderStorage
	{
		bool Initialize();
		std::optional<wil::com_ptr<IDxcBlob>> CompileShader(const ShaderCompileParameters* parameters);

	#ifndef YSN_RELEASE
		void VerifyAnyShaderChanged();
	#endif

	private:
	#ifndef YSN_RELEASE
		std::optional<std::time_t> GetShaderModificationTime(const std::filesystem::path& shader_path);
		std::map<std::wstring, std::time_t> m_shaders_modified_time;
	#endif

		//wil::com_ptr<IDxcBlob> m_include_blob;
	};
}
