module;

#include <d3dx12.h>

export module renderer.command_context;

import std;

export namespace ysn
{
	class CommandContext;

	enum class ContextType
	{
		Direct,
		Compute,
		Copy
	};

	class ContextManager
	{
	public:
		ContextManager(void)
		{
		}

		CommandContext* AllocateContext(ContextType type);
		void FreeContext(CommandContext* context);
		void DestroyAllContexts();

	private:
		static constexpr uint8_t g_max_context_count = 4;

		//std::vector<std::unique_ptr<CommandContext>> m_context_pool[g_max_context_count];
		//std::queue<CommandContext*> m_available_contexts[g_max_context_count];
	};

	class CommandContext
	{
	public:
		CommandContext() = default;
		CommandContext(const CommandContext&) = delete;
		CommandContext& operator=(const CommandContext&) = delete;

	private:
	};

	class GraphicsContext : public CommandContext
	{
	};

	class ComputeContext : public CommandContext
	{
	};

	// Not sure about this one so far
	class RaytracingContext : public CommandContext
	{
	};
}

module :private;