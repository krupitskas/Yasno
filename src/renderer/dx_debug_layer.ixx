module;

#include <DirectXMath.h>
#include <d3dx12.h>
#include <wil/com.h>

export module renderer.dx_debug_layer;

export namespace ysn
{
	class DxDebugLayer
	{
	public:
		bool Initialize();
	};
};

namespace ysn
{
	bool DxDebugLayer::Initialize()
	{
		//HMODULE d3d12DllHandle = GetDllHandle(("d3d12.dll"));
		//typedef HRESULT(WINAPI * FD3D12GetInterface)(REFCLSID, REFIID, void**);

		//FD3D12GetInterface D3D12GetInterfaceFnPtr = (FD3D12GetInterface)(void*)(GetProcAddress(d3d12DllHandle, "D3D12GetInterface"));

		//ID3D12DeviceRemovedExtendedDataSettings* DredSettings;
		//HRESULT hr = D3D12GetInterfaceFnPtr(CLSID_D3D12DeviceRemovedExtendedData, IID_PPV_ARGS(&DredSettings));


		return true;
	}
}