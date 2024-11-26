#include "NvAftermath.hpp"

#include <GFSDK_Aftermath.h>
#include <GFSDK_Aftermath_GpuCrashDump.h>

namespace ysn
{
	void GpuCrashTracker::OnCrashDump(const void* pGpuCrashDump, const uint32_t gpuCrashDumpSize)
	{
		// Make sure only one thread at a time...
		std::lock_guard<std::mutex> lock(m_mutex);

		// Write to file for later in-depth analysis.
		WriteGpuCrashDumpToFile(pGpuCrashDump, gpuCrashDumpSize);
	}

	void GpuCrashDumpCallback(const void* pGpuCrashDump, const uint32_t gpuCrashDumpSize, void* pUserData)
	{
		GpuCrashTracker* pGpuCrashTracker = reinterpret_cast<GpuCrashTracker*>(pUserData);
		pGpuCrashTracker->OnCrashDump(pGpuCrashDump, gpuCrashDumpSize);
	}

	void GpuCrashTracker::OnShaderDebugInfo(const void* pShaderDebugInfo, const uint32_t shaderDebugInfoSize)
	{
		// Make sure only one thread at a time...
		std::lock_guard<std::mutex> lock(m_mutex);

		// Get shader debug information identifier.
		GFSDK_Aftermath_ShaderDebugInfoIdentifier identifier = {};
		AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_GetShaderDebugInfoIdentifier(GFSDK_Aftermath_Version_API, pShaderDebugInfo, shaderDebugInfoSize, &identifier));

		// Write to file for later in-depth analysis of crash dumps with Nsight Graphics.
		WriteShaderDebugInformationToFile(identifier, pShaderDebugInfo, shaderDebugInfoSize);
	}

	void ShaderDebugInfoCallback(const void* pShaderDebugInfo, const uint32_t shaderDebugInfoSize, void* pUserData)
	{
		GpuCrashTracker* pGpuCrashTracker = reinterpret_cast<GpuCrashTracker*>(pUserData);
		pGpuCrashTracker->OnShaderDebugInfo(pShaderDebugInfo, shaderDebugInfoSize);
	}

	void GpuCrashTracker::OnDescription(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addDescription)
	{
		// Add some basic description about the crash.
		addDescription(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName, "Hello Nsight Aftermath");
		addDescription(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationVersion, "v1.0");
		addDescription(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_UserDefined, "This is a GPU crash dump example");
		addDescription(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_UserDefined + 1, "Engine State: Rendering");
	}

	void CrashDumpDescriptionCallback(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addDescription, void* pUserData)
	{
		GpuCrashTracker* pGpuCrashTracker = reinterpret_cast<GpuCrashTracker*>(pUserData);
		pGpuCrashTracker->OnDescription(addDescription);
	}

	void ResolveMarkerCallback(const void* pMarkerData, const uint32_t markerDataSize, void* pUserData, void** ppResolvedMarkerData, uint32_t* pResolvedMarkerDataSize)
	{
		GpuCrashTracker* pGpuCrashTracker = reinterpret_cast<GpuCrashTracker*>(pUserData);
		pGpuCrashTracker->OnResolveMarker(pMarkerData, markerDataSize, ppResolvedMarkerData, pResolvedMarkerDataSize);
	}

	bool InitializeAftermath()
	{

		AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_EnableGpuCrashDumps(
			GFSDK_Aftermath_Version_API,
			GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_DX,
			GFSDK_Aftermath_GpuCrashDumpFeatureFlags_Default,   // Default behavior.
			GpuCrashDumpCallback,                               // Register callback for GPU crash dumps.
			ShaderDebugInfoCallback,                            // Register callback for shader debug information.
			CrashDumpDescriptionCallback,                       // Register callback for GPU crash dump description.
			ResolveMarkerCallback,                              // Register callback for marker resolution (R495 or later NVIDIA graphics driver).
			&m_gpuCrashDumpTracker));                           // Set the GpuCrashTracker object as user data passed back by the above callbacks.

		return false;
	}

	bool InitializeAftermathDevice()
	{
		// Initialize Nsight Aftermath for this device.
		const uint32_t aftermathFlags =
			GFSDK_Aftermath_FeatureFlags_EnableMarkers |             // Enable event marker tracking.
			GFSDK_Aftermath_FeatureFlags_CallStackCapturing |        // Enable automatic call stack event markers.
			GFSDK_Aftermath_FeatureFlags_EnableResourceTracking |    // Enable tracking of resources.
			GFSDK_Aftermath_FeatureFlags_GenerateShaderDebugInfo |   // Generate debug information for shaders.
			GFSDK_Aftermath_FeatureFlags_EnableShaderErrorReporting; // Enable additional runtime shader error reporting.

		AFTERMATH_CHECK_ERROR(GFSDK_Aftermath_DX12_Initialize(
			GFSDK_Aftermath_Version_API,
			aftermathFlags,
			m_device.Get()));
	}
}