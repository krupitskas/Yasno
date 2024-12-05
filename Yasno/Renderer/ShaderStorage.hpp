#pragma once

#include <wil/com.h>
#include <dxcapi.h>
#include <d3d12.h>

import <string>;
import <vector>;
import <optional>;
import <unordered_map>;
import <optional>;

namespace ysn
{
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
		std::wstring shader_path			= L"";
		std::wstring entry_point			= L"main";
		bool disable_optimizations			= false;
		ShaderType shader_type				= ShaderType::None;
		std::vector<std::wstring> defines;
	};

	struct ShaderStorage
	{
		bool Initialize();
		std::optional<wil::com_ptr<IDxcBlob>> CompileShader(const ShaderCompileParameters& parameters);

	#ifndef YSN_RELEASE
		void VerifyAnyShaderChanged();
	#endif

	private:
	#ifndef YSN_RELEASE
		std::optional<std::time_t> GetShaderModificationTime(const std::filesystem::path& shader_path);
		std::map<std::wstring, std::time_t> m_shaders_modified_time;
	#endif

		std::wstring m_debug_data_path = L"ShadersDebugData";
		std::wstring m_binary_data_path = L"ShadersBinaryData";

		bool m_treat_warnings_as_errors = false;

		wil::com_ptr<IDxcUtils> m_dxc_utils;
		wil::com_ptr<IDxcCompiler3> m_dxc_compiler;
		wil::com_ptr<IDxcIncludeHandler> m_dxc_include_handler;
		std::vector<wil::com_ptr<IDxcBlob>> m_dxc_included_files;
		
	};
}
