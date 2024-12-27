module;

#include <DirectXMath.h>
#include <d3dx12.h>
#include <wil/com.h>

export module graphics.techniques.debug_renderer;

import renderer.dxrenderer;
import renderer.dx_types;

export namespace ysn
{
class DebugRenderer
{
public:
    bool Initialize();
    void RenderDebugGeometry();

private:
    wil::com_ptr<ID3D12RootSignature> m_root_signature;
    wil::com_ptr<ID3D12PipelineState> m_pipeline_state;
};

} // namespace ysn

module :private;

namespace ysn
{
bool DebugRenderer::Initialize()
{

    return true;
}

void DebugRenderer::RenderDebugGeometry()
{
}
} // namespace ysn