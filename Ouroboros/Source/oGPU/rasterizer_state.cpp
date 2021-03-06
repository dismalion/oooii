// Copyright (c) 2014 Antony Arciuolo. See License.txt regarding use.
#include <oGPU/rasterizer_state.h>
#include <oCore/windows/win_util.h>
#include "d3d_debug.h"
#include "d3d_util.h"

using namespace ouro::gpu::d3d;

namespace ouro {

const char* as_string(const gpu::rasterizer_state::value& _Value)
{
	switch (_Value)
	{
		case gpu::rasterizer_state::front_face: return "front_face";
		case gpu::rasterizer_state::back_face: return "back_face"; 
		case gpu::rasterizer_state::two_sided: return "two_sided";
		case gpu::rasterizer_state::front_wireframe: return "front_wireframe";
		case gpu::rasterizer_state::back_wireframe: return "back_wireframe"; 
		case gpu::rasterizer_state::two_sided_wireframe: return "two_sided_wireframe";
		default: break;
	}
	return "?";
}
oDEFINE_TO_FROM_STRING(gpu::rasterizer_state::value);

namespace gpu {

Device* get_device(device& dev);
DeviceContext* get_dc(command_list& cl);

void rasterizer_state::initialize(const char* name, device& dev)
{
	deinitialize();

	static const D3D11_FILL_MODE sFills[] = 
	{
		D3D11_FILL_SOLID,
		D3D11_FILL_SOLID,
		D3D11_FILL_SOLID,
		D3D11_FILL_WIREFRAME,
		D3D11_FILL_WIREFRAME,
		D3D11_FILL_WIREFRAME,
	};
	static_assert(oCOUNTOF(sFills) == count, "array mismatch");

	static const D3D11_CULL_MODE sCulls[] = 
	{
		D3D11_CULL_BACK,
		D3D11_CULL_FRONT,
		D3D11_CULL_NONE,
		D3D11_CULL_BACK,
		D3D11_CULL_FRONT,
		D3D11_CULL_NONE,
	};
	static_assert(oCOUNTOF(sCulls) == count, "array mismatch");

	mstring n;
	D3D11_RASTERIZER_DESC desc;
  desc.FrontCounterClockwise = false;
  desc.DepthBias = D3D11_DEFAULT_DEPTH_BIAS;
  desc.DepthBiasClamp = D3D11_DEFAULT_DEPTH_BIAS_CLAMP;
  desc.SlopeScaledDepthBias = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
  desc.DepthClipEnable = true;
  desc.ScissorEnable = false;
  desc.MultisampleEnable = false;
  desc.AntialiasedLineEnable = false;

	Device* D3DDevice = get_device(dev);
	for (int i = 0; i < count; i++)
	{
		desc.FillMode = sFills[i];
		desc.CullMode = sCulls[i];
		oV(D3DDevice->CreateRasterizerState(&desc, (RasterizerState**)&states[i]));
		snprintf(n, "%s::%s", name, as_string(value(i)));
		debug_name((RasterizerState*)states[i], n);
	}
}

void rasterizer_state::deinitialize()
{
	for (auto& p : states)
	{
		oSAFE_RELEASEV(p);
	}
}

void rasterizer_state::set(command_list& cl, const value& state)
{
	get_dc(cl)->RSSetState((RasterizerState*)states[state]);
}

}}
