#pragma once

#include <Renderer/PipelineStateObject.hpp>

namespace ysn
{
	struct PSOStorage
	{
		PipelineStateObjectId CreatePso();
		void DeletePso();

		std::unordered_map<PipelineStateObjectId, PipelineStateObject> pso_pool;
	};
}