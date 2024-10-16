#pragma once

#include <Renderer/Pso.hpp>

namespace ysn
{
	struct PSOStorage
	{
		PsoId CreatePso();
		void DeletePso();

	};
}