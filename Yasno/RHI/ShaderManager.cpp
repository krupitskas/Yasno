#include "ShaderManager.hpp"

#include <fstream>
#include <filesystem>
#include <string>

#ifndef YSN_RELEASE
#include <sys/types.h>
#include <sys/stat.h>
#ifndef WIN32
#include <unistd.h>
#endif

#ifdef WIN32
#define stat _stat
#endif
#endif

#include <System/String.hpp>

// https://simoncoenen.com/blog/programming/graphics/DxcCompiling

namespace ysn
{
	// IDxcBlob* CompileShaderLibrary(LPCWSTR fileName)
	//{
	//	static IDxcCompiler* pCompiler = nullptr;
	//	static IDxcLibrary* pLibrary = nullptr;
	//	static IDxcIncludeHandler* dxcIncludeHandler;

	//	HRESULT hr;

	//	// Initialize the DXC compiler and compiler helper
	//	if (!pCompiler)
	//	{
	//		ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, __uuidof(IDxcCompiler), (void**)&pCompiler));
	//		ThrowIfFailed(DxcCreateInstance(CLSID_DxcLibrary, __uuidof(IDxcLibrary), (void**)&pLibrary));
	//		ThrowIfFailed(pLibrary->CreateIncludeHandler(&dxcIncludeHandler));
	//	}
	//	// Open and read the file
	//	std::ifstream shaderFile(fileName);
	//	if (shaderFile.good() == false)
	//	{
	//		throw std::logic_error("Cannot find shader file");
	//	}
	//	std::stringstream strStream;
	//	strStream << shaderFile.rdbuf();
	//	std::string sShader = strStream.str();

	//	// Create blob from the string
	//	IDxcBlobEncoding* pTextBlob;
	//	ThrowIfFailed(pLibrary->CreateBlobWithEncodingFromPinned((LPBYTE)sShader.c_str(), (uint32_t)sShader.size(), 0, &pTextBlob));

	//	// Compile
	//	IDxcOperationResult* pResult;
	//	ThrowIfFailed(pCompiler->Compile(pTextBlob, fileName, L"", L"lib_6_3", nullptr, 0, nullptr, 0,
	//									 dxcIncludeHandler, &pResult));

	//	// Verify the result
	//	HRESULT resultCode;
	//	ThrowIfFailed(pResult->GetStatus(&resultCode));
	//	if (FAILED(resultCode))
	//	{
	//		IDxcBlobEncoding* pError;
	//		hr = pResult->GetErrorBuffer(&pError);
	//		if (FAILED(hr))
	//		{
	//			throw std::logic_error("Failed to get shader compiler error");
	//		}

	//		// Convert error blob to a string
	//		std::vector<char> infoLog(pError->GetBufferSize() + 1);
	//		memcpy(infoLog.data(), pError->GetBufferPointer(), pError->GetBufferSize());
	//		infoLog[pError->GetBufferSize()] = 0;

	//		std::string errorMsg = "Shader Compiler Error:\n";
	//		errorMsg.append(infoLog.data());

	//		MessageBoxA(nullptr, errorMsg.c_str(), "Error!", MB_OK);
	//		throw std::logic_error("Failed compile shader");
	//	}

	//	IDxcBlob* pBlob;
	//	ThrowIfFailed(pResult->GetResult(&pBlob));
	//	return pBlob;
	//}

	static IDxcCompiler* dxc_compiler = nullptr;
	static IDxcLibrary* dxc_library = nullptr;
	static IDxcIncludeHandler* dxc_include_handler = nullptr;

	bool ShaderManager::Initialize()
	{
		if (auto result = DxcCreateInstance(CLSID_DxcCompiler, __uuidof(IDxcCompiler), (void**)&dxc_compiler); result != S_OK)
		{
			LogError << "Can't create dxc compiler\n";
			return false;
		}

		if (auto result = DxcCreateInstance(CLSID_DxcLibrary, __uuidof(IDxcLibrary), (void**)&dxc_library); result != S_OK)
		{
			LogError << "Can't create dxc library\n";
			return false;
		}

		if (auto result = dxc_library->CreateIncludeHandler(&dxc_include_handler); result != S_OK)
		{
			LogError << "Can't create dxc include handler\n";
			return false;
		}

		return true;
	}

	std::optional<wil::com_ptr<IDxcBlob>> ShaderManager::CompileShader(const ShaderCompileParameters* parameters)
	{
		std::filesystem::path fullpath(parameters->shader_path);
		const std::wstring filename = fullpath.filename();

		LogInfo << "Compiling shader: " << fullpath.string().c_str() << "\n";

		std::vector<LPCWSTR> arguments;

		std::wstring shader_profile;

		switch (parameters->shader_type)
		{
			case ShaderType::None:
				LogFatal << "... has None as shader type\n";
				return std::nullopt;
			case ShaderType::Pixel:
				shader_profile = L"ps_6_6";
				break;
			case ShaderType::Vertex:
				shader_profile = L"vs_6_6";
				break;
			case ShaderType::Compute:
				shader_profile = L"cs_6_6";
				break;
			case ShaderType::Library:
				shader_profile = L"lib_6_6";
				break;
		}

		// Strip reflection data and pdbs (see later)
		//arguments.push_back(L"-Qstrip_debug");
		//arguments.push_back(L"-Qstrip_reflect");

		//arguments.push_back(DXC_ARG_WARNINGS_ARE_ERRORS); //-WX
		if (!parameters->disable_optimizations)
		{
			arguments.push_back(DXC_ARG_DEBUG);
		}

		std::ifstream shader_file(parameters->shader_path);
		if (shader_file.good() == false)
		{
			LogError << "Cannot find shader file :" << WStringToString(parameters->shader_path) << "\n";
			return std::nullopt;
		}

		std::stringstream str_stream;
		str_stream << shader_file.rdbuf();
		std::string shader_cod = str_stream.str();

		IDxcBlobEncoding* dxc_text_blob;
		if (auto result = dxc_library->CreateBlobWithEncodingFromPinned((LPBYTE)shader_cod.c_str(), (uint32_t)shader_cod.size(), 0, &dxc_text_blob); result != S_OK)
		{
			LogError << "Can't create shader blob\n";
			return std::nullopt;
		}

		IDxcOperationResult* dxc_op_result;
		if (auto result = dxc_compiler->Compile(dxc_text_blob,
			filename.c_str(),
			parameters->entry_point.c_str(),
			shader_profile.c_str(),
			arguments.data(),
			static_cast<uint32_t>(arguments.size()),
			parameters->defines.data(),
			static_cast<uint32_t>(parameters->defines.size()),
			dxc_include_handler,
			&dxc_op_result);
			result != S_OK)
		{
			LogError << "Can't compile shader\n";
			return std::nullopt;
		}

		HRESULT result_code;
		if (auto result = dxc_op_result->GetStatus(&result_code); result != S_OK)
		{
			LogError << "Can't get status after shader compilation\n";
			return std::nullopt;
		}

		if (FAILED(result_code))
		{
			IDxcBlobEncoding* error_buffer;
			const auto result = dxc_op_result->GetErrorBuffer(&error_buffer);

			if (FAILED(result))
			{
				LogError << "Failed to get shader compiler error\n";
				return std::nullopt;
			}

			// Convert error blob to a string
			std::vector<char> info_log(error_buffer->GetBufferSize() + 1);
			memcpy(info_log.data(), error_buffer->GetBufferPointer(), error_buffer->GetBufferSize());
			info_log[error_buffer->GetBufferSize()] = 0;

			std::string error_msg = "Shader Compiler Error: ";
			error_msg.append(info_log.data());

			LogFatal << "Failed compile shader\n" << error_msg.c_str() << "\n";
			return std::nullopt;
		}

		// wil::com_ptr<ID3DBlob> result_blob;
		wil::com_ptr<IDxcBlob> result_blob;

		if (auto result = dxc_op_result->GetResult((IDxcBlob**)&result_blob); FAILED(result))
		{
			LogError << "Can't get shader blob result\n";
			return std::nullopt;
		}

	#ifndef YSN_RELEASE
		const auto shader_modification_time = GetShaderModificationTime(parameters->shader_path);
		
		if (shader_modification_time.has_value())
		{
			m_shaders_modified_time.emplace(parameters->shader_path, shader_modification_time.value());
		}
		else
		{
			LogError << "Can't get shader modification time: " << WStringToString(parameters->shader_path) << "\n";
		}
	#endif

		//m_shaders_modified_time

		return result_blob;

		// ComPtr<IDxcBlob> pDebugData;
		// ComPtr<IDxcBlobUtf16> pDebugDataPath;
		// pCompileResult->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(pDebugData.GetAddressOf()), pDebugDataPath.GetAddressOf());

		// Reflection
		/*ComPtr<IDxcBlob> pReflectionData;
		pCompileResult->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(pReflectionData.GetAddressOf()), nullptr);
		DxcBuffer reflectionBuffer;
		reflectionBuffer.Ptr = pReflectionData->GetBufferPointer();
		reflectionBuffer.Size = pReflectionData->GetBufferSize();
		reflectionBuffer.Encoding = 0;
		ComPtr<ID3D12ShaderReflection> pShaderReflection;
		pUtils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(pShaderReflection.GetAddressOf()));*/

		// RefCountPtr<IDxcBlob> pHash;
		// if (SUCCEEDED(pCompileResult->GetOutput(DXC_OUT_SHADER_HASH, IID_PPV_ARGS(pHash.GetAddressOf()), nullptr)))
		//{
		//	DxcShaderHash* pHashBuf = (DxcShaderHash*)pHash->GetBufferPointer();
		// }
	}

	void ShaderManager::VerifyAnyShaderChanged()
	{

		for(const auto& [shader_path, time] : m_shaders_modified_time)
		{
			const auto latest_time = GetShaderModificationTime(shader_path);

			if(latest_time.has_value())
			{
				if(latest_time.value() != time)
				{
					LogInfo << "Shader " << WStringToString(shader_path) << " was modified, recompiling PSO\n";

					// TODO: Notify PSO manager to compile the shader here
					
					m_shaders_modified_time[shader_path] = latest_time.value();
				}
			}
			else
			{
				LogError << "Can't get shader modification time: " << WStringToString(shader_path) << "\n";
			}
		}
	}

#ifndef YSN_RELEASE
	std::optional<std::time_t> ShaderManager::GetShaderModificationTime(const std::filesystem::path& shader_path)
	{
		struct stat result;

		if(stat(shader_path.string().c_str(), &result)==0)
		{
			return result.st_mtime;
		}

		return std::nullopt;
	}
#endif
}
