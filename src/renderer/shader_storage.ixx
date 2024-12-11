module;

#include <wil/com.h>
#include <dxcapi.h>
#include <d3d12.h>
#include <d3d12shader.h>

export module renderer.shader_storage;

import std;
import system.string_helpers;
import system.filesystem;
import system.logger;
import system.asserts;

export namespace ysn
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
    std::wstring shader_path = L"";
    std::wstring entry_point = L"main";
    bool disable_optimizations = false;
    ShaderType shader_type = ShaderType::None;
    std::vector<std::wstring> defines;
};

struct ShaderStorage
{
    bool Initialize();
    std::optional<wil::com_ptr<IDxcBlob>> CompileShader(const ShaderCompileParameters& parameters);

    // todo(modules)
#ifndef YSN_RELEASE
    // void VerifyAnyShaderChanged();
#endif

private:
    // #ifndef YSN_RELEASE
    //	std::optional<std::time_t> GetShaderModificationTime(const std::filesystem::path& shader_path);
    //	std::map<std::wstring, std::time_t> m_shaders_modified_time;
    // #endif

    std::wstring m_debug_data_path = L"ShadersDebugData";
    std::wstring m_binary_data_path = L"ShadersBinaryData";

    bool m_treat_warnings_as_errors = false;

    wil::com_ptr<IDxcUtils> m_dxc_utils;
    wil::com_ptr<IDxcCompiler3> m_dxc_compiler;
    wil::com_ptr<IDxcIncludeHandler> m_dxc_include_handler;
    std::vector<wil::com_ptr<IDxcBlob>> m_dxc_included_files;
};
} // namespace ysn

module :private;

//
// #ifndef YSN_RELEASE
// #include <sys/types.h>
// #include <sys/stat.h>
// #ifndef WIN32
// #include <unistd.h>
// #endif
//
// #ifdef WIN32
// #define stat _stat
// #endif
// #endif

// https://simoncoenen.com/blog/programming/graphics/DxcCompiling

namespace ysn
{

// class CustomIncludeHandler : public IDxcIncludeHandler
//{
// public:
//	HRESULT STDMETHODCALLTYPE LoadSource(_In_ LPCWSTR pFilename, _COM_Outptr_result_maybenull_ IDxcBlob** ppIncludeSource) override
//	{
//		ComPtr<IDxcBlobEncoding> pEncoding;
//		std::string path = Paths::Normalize(UNICODE_TO_MULTIBYTE(pFilename));
//		if (IncludedFiles.find(path) != IncludedFiles.end())
//		{
//			// Return empty string blob if this file has been included before
//			static const char nullStr[] = " ";
//			pUtils->CreateBlobFromPinned(nullStr, ARRAYSIZE(nullStr), DXC_CP_ACP, pEncoding.GetAddressOf());
//			*ppIncludeSource = pEncoding.Detach();
//			return S_OK;
//		}

//		HRESULT hr = pUtils->LoadFile(pFilename, nullptr, pEncoding.GetAddressOf());
//		if (SUCCEEDED(hr))
//		{
//			IncludedFiles.insert(path);
//			*ppIncludeSource = pEncoding.Detach();
//		}
//		return hr;
//	}

//	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject) override { return E_NOINTERFACE; }
//	ULONG STDMETHODCALLTYPE AddRef(void) override {	return 0; }
//	ULONG STDMETHODCALLTYPE Release(void) override { return 0; }

//	std::unordered_set<std::string> IncludedFiles;
//};

bool ShaderStorage::Initialize()
{
    if (auto result = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(m_dxc_compiler.addressof())); result != S_OK)
    {
        LogError << "Can't create dxc compiler\n";
        return false;
    }

    if (auto result = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(m_dxc_utils.addressof())); result != S_OK)
    {
        LogError << "Can't create dxc utils\n";
        return false;
    }

    // Some common include files
    {
        HRESULT hr = m_dxc_utils->CreateDefaultIncludeHandler(m_dxc_include_handler.addressof());

        if (hr != S_OK)
        {
            LogError << "Can't create include handler\n";
            return false;
        }

        wil::com_ptr<IDxcBlob> new_included_file;

        const std::wstring shared_path = ysn::GetVirtualFilesystemPath(L"shaders/include/shared.hlsl");
        hr = m_dxc_include_handler->LoadSource(shared_path.c_str(), new_included_file.addressof());

        if (hr != S_OK)
        {
            LogError << "Can't load shaders/include/shared.hlsl\n";
            return false;
        }

        m_dxc_included_files.push_back(new_included_file);
    }

    return true;
}

std::optional<wil::com_ptr<IDxcBlob>> ShaderStorage::CompileShader(const ShaderCompileParameters& parameters)
{
    // TODO: search for this cached binary and return it if possible

    std::filesystem::path fullpath(parameters.shader_path);
    const std::wstring filename = fullpath.filename();

    LogInfo << "Compiling shader: " << fullpath.string().c_str() << "\n";

    std::vector<LPCWSTR> arguments;

    std::wstring shader_profile;

    switch (parameters.shader_type)
    {
    case ShaderType::None:
        LogError << "... has None as shader type\n";
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

    if (!parameters.disable_optimizations)
    {
        arguments.push_back(DXC_ARG_DEBUG);
    }

    if (m_treat_warnings_as_errors)
    {
        arguments.push_back(DXC_ARG_WARNINGS_ARE_ERRORS);
    }

    // /enable-16bit-types

    // Entry point
    arguments.push_back(L"-E");
    arguments.push_back(parameters.entry_point.c_str());

    // Shaders include dir
    arguments.push_back(L"-I shaders/include");

    // Profile
    arguments.push_back(L"-T");
    arguments.push_back(shader_profile.c_str());

    // Defines
    for (const std::wstring& define : parameters.defines)
    {
        arguments.push_back(L"-D");
        arguments.push_back(define.c_str());
    }

    // Strip reflection data and pdbs, store them separately
    arguments.push_back(L"-Qstrip_debug");
    arguments.push_back(L"-Qstrip_reflect");

    std::ifstream shader_file(parameters.shader_path);
    if (shader_file.good() == false)
    {
        LogError << "Cannot find shader file :" << WStringToString(parameters.shader_path) << "\n";
        return std::nullopt;
    }

    std::stringstream str_stream;
    str_stream << shader_file.rdbuf();
    std::string shader_code = str_stream.str();

    wil::com_ptr<IDxcBlobEncoding> dxc_text_blob;

    if (auto result = m_dxc_utils->CreateBlob(
            shader_code.c_str(), static_cast<uint32_t>(shader_code.size()), CP_UTF8, dxc_text_blob.addressof());
        result != S_OK)
    {
        LogError << "Can't create shader blob\n";
        return std::nullopt;
    }

    DxcBuffer source_buffer;
    source_buffer.Ptr = dxc_text_blob->GetBufferPointer();
    source_buffer.Size = dxc_text_blob->GetBufferSize();
    source_buffer.Encoding = DXC_CP_UTF8;

    IDxcResult* dxc_op_result; // TODO: Make this wil_comptr and result would be corrupted -> investigate

    if (auto result = m_dxc_compiler->Compile(
            &source_buffer,
            arguments.data(),
            static_cast<uint32_t>(arguments.size()),
            m_dxc_include_handler.get(),
            __uuidof(IDxcResult),
            (void**)&dxc_op_result);
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

        LogError << "Failed compile shader\n" << error_msg.c_str() << "\n";
        return std::nullopt;
    }

    wil::com_ptr<IDxcBlob> shader_data;
    wil::com_ptr<IDxcBlobUtf16> shader_name;
    if (auto result = dxc_op_result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(shader_data.addressof()), shader_name.addressof());
        FAILED(result))
    {
        LogError << "Can't get shader blob DXC_OUT_OBJECT\n";
        return std::nullopt;
    }

    wil::com_ptr<IDxcBlob> shader_debug_data;
    wil::com_ptr<IDxcBlobUtf16> shader_debug_data_path;
    if (auto result =
            dxc_op_result->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(shader_debug_data.addressof()), shader_debug_data_path.addressof());
        FAILED(result))
    {
        LogError << "Can't get shader blob DXC_OUT_PDB\n";
        return std::nullopt;
    }

    if (shader_debug_data && shader_debug_data_path)
    {
        char buffer[MAX_PATH];
        GetModuleFileNameA(nullptr, buffer, MAX_PATH);

        std::string executable_path(buffer);
        std::string directory_path = executable_path.substr(0, executable_path.find_last_of("\\/"));

        std::filesystem::path executable_dir(directory_path);

        std::wstring pdb_path;
        pdb_path.append(executable_dir.wstring());
        pdb_path.append(L"\\");
        pdb_path.append(m_debug_data_path);

        if (CreateDirectoryIfNotExists(pdb_path))
        {
            pdb_path.append(L"\\");
            pdb_path.append(
                shader_debug_data_path->GetStringPointer(),
                shader_debug_data_path->GetStringPointer() + shader_debug_data_path->GetStringLength());

            FILE* file_poiter = nullptr;
            _wfopen_s(&file_poiter, pdb_path.c_str(), L"wb");

            if (file_poiter)
            {
                fwrite(shader_debug_data->GetBufferPointer(), shader_debug_data->GetBufferSize(), 1, file_poiter);
                fclose(file_poiter);
            }
            else
            {
                LogError << "Can't store pdb data\n";
            }
        }
        else
        {
            LogError << "Can't create pdb storage directory \n";
        }
    }

    // Read reflection data
    wil::com_ptr<IDxcBlob> reflection_data;
    if (auto result = dxc_op_result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(reflection_data.addressof()), nullptr); FAILED(result))
    {
        LogError << "Can't get shader blob DXC_OUT_REFLECTION\n";
        return std::nullopt;
    }

    // Not using it right now but can be handy in the future
    DxcBuffer reflection_buffer;
    reflection_buffer.Ptr = reflection_data->GetBufferPointer();
    reflection_buffer.Size = reflection_data->GetBufferSize();
    reflection_buffer.Encoding = 0;
    wil::com_ptr<ID3D12ShaderReflection> shader_reflection;
    m_dxc_utils->CreateReflection(&reflection_buffer, IID_PPV_ARGS(shader_reflection.addressof()));

    // Read shader hash
    wil::com_ptr<IDxcBlob> hash;
    if (auto result = dxc_op_result->GetOutput(DXC_OUT_SHADER_HASH, IID_PPV_ARGS(hash.addressof()), nullptr); FAILED(result))
    {
        LogError << "Can't get shader blob DXC_OUT_SHADER_HASH\n";
        return std::nullopt;
    }

    DxcShaderHash* hash_buffer = (DxcShaderHash*)hash->GetBufferPointer();
    std::vector<uint8_t> hash_vec;
    for (int i = 0; i < _countof(hash_buffer->HashDigest); i++)
    {
        hash_vec.push_back(hash_buffer->HashDigest[i]);
    }

    // #ifndef YSN_RELEASE
    //	const auto shader_modification_time = GetShaderModificationTime(parameters.shader_path);

    //	if (shader_modification_time.has_value())
    //	{
    //		m_shaders_modified_time.emplace(parameters.shader_path, shader_modification_time.value());
    //	}
    //	else
    //	{
    //		LogError << "Can't get shader modification time: " << WStringToString(parameters.shader_path) << "\n";
    //	}
    // #endif

    return shader_data;
}
//
// #ifndef YSN_RELEASE
//	void ShaderStorage::VerifyAnyShaderChanged()
//	{
//		for(const auto& [shader_path, time] : m_shaders_modified_time)
//		{
//			const auto latest_time = GetShaderModificationTime(shader_path);
//
//			if(latest_time.has_value())
//			{
//				if(latest_time.value() != time)
//				{
//					LogInfo << "Shader " << WStringToString(shader_path) << " was modified, recompiling PSO\n";
//
//					// TODO: Notify PSO manager to compile the shader here
//					m_shaders_modified_time[shader_path] = latest_time.value();
//				}
//			}
//			else
//			{
//				LogError << "Can't get shader modification time: " << WStringToString(shader_path) << "\n";
//			}
//		}
//	}
//
//	std::optional<std::time_t> ShaderStorage::GetShaderModificationTime(const std::filesystem::path& shader_path)
//	{
//		//struct stat result;
//
//		//if(stat(shader_path.string().c_str(), &result)==0)
//		//{
//		//	return result.st_mtime;
//		//}
//
//		return std::nullopt;
//	}
// #endif
} // namespace ysn
