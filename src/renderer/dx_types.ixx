module;

#include <d3d12.h>
#include <dxgi1_5.h>
#include <dxcapi.h>
#include <dxgi1_6.h>

export module renderer.dx_types;

export using DxDevice = ID3D12Device5;
export using DxSwapChain = IDXGISwapChain4;
export using DxGraphicsCommandList = ID3D12GraphicsCommandList4;
export using DxResource = ID3D12Resource;
export using DxCompiler = IDxcCompiler3;
export using DxAdapter = IDXGIAdapter4;
