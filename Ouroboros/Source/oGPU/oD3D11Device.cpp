/**************************************************************************
 * The MIT License                                                        *
 * Copyright (c) 2013 Antony Arciuolo.                                    *
 * arciuolo@gmail.com                                                     *
 *                                                                        *
 * Permission is hereby granted, free of charge, to any person obtaining  *
 * a copy of this software and associated documentation files (the        *
 * "Software"), to deal in the Software without restriction, including    *
 * without limitation the rights to use, copy, modify, merge, publish,    *
 * distribute, sublicense, and/or sell copies of the Software, and to     *
 * permit persons to whom the Software is furnished to do so, subject to  *
 * the following conditions:                                              *
 *                                                                        *
 * The above copyright notice and this permission notice shall be         *
 * included in all copies or substantial portions of the Software.        *
 *                                                                        *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        *
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND                  *
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE *
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION *
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION  *
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.        *
 **************************************************************************/
#include "oD3D11Device.h"
#include "oD3D11CommandList.h"
#include "oD3D11Query.h"
#include "oD3D11RenderTarget.h"

#include "d3d11_util.h"
#include "dxgi_util.h"
#include "CSNoop.h"

#include <oGUI/window.h>

namespace ouro {
	namespace gpu {

std::shared_ptr<device> device::make(const device_init& _Init)
{
	auto p = std::make_shared<d3d11::d3d11_device>(_Init);
	p->intialize_immediate_context();
	return p;
}

scoped_allocation compile_shader(const char* _IncludePaths
																 , const char* _Defines
																 , const char* _ShaderSourcePath
																 , const pipeline_stage::value& _Stage
																 , const char* _EntryPoint
																 , const char* _ShaderSource
																 , const allocator& _Allocator)
{
	std::string cmdline;
	cmdline.reserve(2048);
	cmdline = "/O3 /T ";
	cmdline += d3d11::shader_profile(D3D_FEATURE_LEVEL_11_0, _Stage);
	cmdline += " /E ";
	cmdline += _EntryPoint;

	// Add defines
	cmdline += " /D oHLSL ";
	if (_Defines)
	{
		char* ctx = nullptr;
		std::string defs(_Defines);
		char* def = strtok_r((char*)defs.c_str(), ";", &ctx);
		while (def)
		{
			cmdline += " /D ";
			cmdline += def;
			def = strtok_r(nullptr, ";", &ctx);
		}
	}
	
	// Add includes
	if (_IncludePaths)
	{
		char* ctx = nullptr;
		std::string incs(_IncludePaths);
		char* inc = strtok_r((char*)incs.c_str(), ";", &ctx);
		while (inc)
		{
			cmdline += " /I ";
			cmdline += inc;
			inc = strtok_r(nullptr, ";", &ctx);
		}
	}

	return d3d11::compile_shader(cmdline.c_str(), _ShaderSourcePath, _ShaderSource, _Allocator);
}

	} // namespace gpu
} // namespace ouro

oGPU_NAMESPACE_BEGIN

template<typename StateT, size_t size> bool state_exists(size_t _Index, StateT (&_States)[size])
{
	for (size_t j = 0; j < _Index; j++)
		if (_States[_Index] == _States[j])
			return true;
	return false;
}

d3d11_device::d3d11_device(const device_init& _Init)
	: FrameID(invalid)
	, hHeap(HeapCreate(0, oMB(10), 0))
	, IsSoftwareEmulation(_Init.use_software_emulation)
	, SupportsDeferredCommandLists(false)
{
	D3DDevice = make_device(_Init);
	SupportsDeferredCommandLists = supports_deferred_contexts(D3DDevice);

	Info = d3d11::get_info(D3DDevice, IsSoftwareEmulation);

	HeapAllocations.reserve(500);

	D3DDevice->GetImmediateContext(&ImmediateContext);
	
	lstring StateName;

	// Blend States
	{
		static const D3D11_RENDER_TARGET_BLEND_DESC sBlends[] =
		{
			{ FALSE, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_COLOR_WRITE_ENABLE_ALL },
			{ FALSE, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_BLEND_ONE, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_COLOR_WRITE_ENABLE_ALL },
			{ TRUE, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, D3D11_COLOR_WRITE_ENABLE_ALL },
			{ TRUE, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, D3D11_BLEND_ZERO, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_COLOR_WRITE_ENABLE_ALL },
			{ TRUE, D3D11_BLEND_DEST_COLOR, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_BLEND_ZERO, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_COLOR_WRITE_ENABLE_ALL },
			{ TRUE, D3D11_BLEND_INV_DEST_COLOR, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD, D3D11_BLEND_ZERO, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_COLOR_WRITE_ENABLE_ALL },
			{ TRUE, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD, D3D11_BLEND_ZERO, D3D11_BLEND_ZERO, D3D11_BLEND_OP_ADD, D3D11_COLOR_WRITE_ENABLE_ALL },
			{ TRUE, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_MIN, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_MIN, D3D11_COLOR_WRITE_ENABLE_ALL },
			{ TRUE, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_MAX, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_MAX, D3D11_COLOR_WRITE_ENABLE_ALL },
		};
		static_assert(oCOUNTOF(sBlends) == blend_state::count, "# blend states mismatch");

		D3D11_BLEND_DESC desc = {0};
		oFORI(i, BlendStates)
		{
			desc.AlphaToCoverageEnable = FALSE;
			desc.IndependentBlendEnable = FALSE;

			for (uint j = 0; j < oCOUNTOF(desc.RenderTarget); j++)
				desc.RenderTarget[j] = sBlends[i];

			oV(D3DDevice->CreateBlendState(&desc, &BlendStates[i]));

			if (!state_exists(i, BlendStates))
			{
				snprintf(StateName, "%s.%s", _Init.debug_name.c_str(), as_string((blend_state::value)i));
				debug_name(BlendStates[i], StateName);
			}
		}
	}

	// Depth States
	{
		static const D3D11_DEPTH_STENCIL_DESC sDepthStencils[] = 
		{
			{ FALSE, D3D11_DEPTH_WRITE_MASK_ALL, D3D11_COMPARISON_ALWAYS, FALSE, 0xFF, 0XFF, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS },
			{ TRUE, D3D11_DEPTH_WRITE_MASK_ALL, D3D11_COMPARISON_LESS_EQUAL, FALSE, 0xFF, 0XFF, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS },
			{ TRUE, D3D11_DEPTH_WRITE_MASK_ZERO, D3D11_COMPARISON_LESS_EQUAL, FALSE, 0xFF, 0XFF, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS },
		};
		oFORI(i, DepthStencilStates)
		{
			oV(D3DDevice->CreateDepthStencilState(&sDepthStencils[i], &DepthStencilStates[i]));
			if (!state_exists(i, DepthStencilStates))
			{
				snprintf(StateName, "%s.%s", _Init.debug_name.c_str(), as_string((depth_stencil_state::value)i));
				debug_name(DepthStencilStates[i], StateName);
			}
		}
	}
	// Surface States
	{
		static const D3D11_FILL_MODE sFills[] = 
		{
			D3D11_FILL_SOLID,
			D3D11_FILL_SOLID,
			D3D11_FILL_SOLID,
			D3D11_FILL_WIREFRAME,
			D3D11_FILL_WIREFRAME,
			D3D11_FILL_WIREFRAME,
		};
		static_assert(oCOUNTOF(sFills) == surface_state::count, "# surface states mismatch");

		static const D3D11_CULL_MODE sCulls[] = 
		{
			D3D11_CULL_BACK,
			D3D11_CULL_FRONT,
			D3D11_CULL_NONE,
			D3D11_CULL_BACK,
			D3D11_CULL_FRONT,
			D3D11_CULL_NONE,
		};
		static_assert(oCOUNTOF(sCulls) == surface_state::count, "# surface states mismatch");

		D3D11_RASTERIZER_DESC desc;
		memset(&desc, 0, sizeof(desc));
		desc.FrontCounterClockwise = FALSE;
		desc.DepthClipEnable = TRUE;

		for (size_t i = 0; i < oCOUNTOF(SurfaceStates); i++)
		{
			desc.FillMode = sFills[i];
			desc.CullMode = sCulls[i];
			oV(D3DDevice->CreateRasterizerState(&desc, &SurfaceStates[i]));
	
			if (!state_exists(i, SurfaceStates))
			{
				snprintf(StateName, "%s.%s", _Init.debug_name.c_str(), as_string((surface_state::value)i));
				debug_name(SurfaceStates[i], StateName);
			}
		}
	}

	// Sampler States
	{
		static const int NUM_ADDRESS_STATES = 6;
		static const int NUM_MIP_BIAS_LEVELS = sampler_type::count / NUM_ADDRESS_STATES;

		static const D3D11_FILTER sFilters[] = 
		{
			D3D11_FILTER_MIN_MAG_MIP_POINT,
			D3D11_FILTER_MIN_MAG_MIP_POINT,
			D3D11_FILTER_MIN_MAG_MIP_LINEAR,
			D3D11_FILTER_MIN_MAG_MIP_LINEAR,
			D3D11_FILTER_COMPARISON_ANISOTROPIC,
			D3D11_FILTER_COMPARISON_ANISOTROPIC,
		};
		static_assert(oCOUNTOF(sFilters) == NUM_ADDRESS_STATES, "# sampler states mismatch");

		static const D3D11_TEXTURE_ADDRESS_MODE sAddresses[] = 
		{
			D3D11_TEXTURE_ADDRESS_CLAMP,
			D3D11_TEXTURE_ADDRESS_WRAP,
			D3D11_TEXTURE_ADDRESS_CLAMP,
			D3D11_TEXTURE_ADDRESS_WRAP,
			D3D11_TEXTURE_ADDRESS_CLAMP,
			D3D11_TEXTURE_ADDRESS_WRAP,
		};
		static_assert(oCOUNTOF(sAddresses) == NUM_ADDRESS_STATES, "# sampler states mismatch");

		static const FLOAT sBiases[] =
		{
			0.0f,
			-1.0f,
			-2.0f,
			1.0f,
			2.0f,
		};
		static_assert(oCOUNTOF(sBiases) == NUM_MIP_BIAS_LEVELS, "# mip bias levels mismatch");

		D3D11_SAMPLER_DESC desc;
		memset(&desc, 0, sizeof(desc));
		mstring StateName;

		for (size_t bias = 0, i = 0; bias < NUM_MIP_BIAS_LEVELS; bias++)
		{
			desc.MipLODBias = sBiases[bias];
			for (size_t state = 0; state < NUM_ADDRESS_STATES; state++)
			{
				desc.Filter = sFilters[state];
				desc.AddressU = desc.AddressV = desc.AddressW = sAddresses[state];
				desc.MaxLOD = FLT_MAX; // documented default
				desc.MaxAnisotropy = 16; // documented default
				desc.ComparisonFunc = D3D11_COMPARISON_NEVER;  // documented default

				snprintf(StateName, "%s.%s", _Init.debug_name.c_str(), as_string(sampler_type::value(NUM_ADDRESS_STATES * bias + state)));
				oV(D3DDevice->CreateSamplerState(&desc, &SamplerStates[i]));
				debug_name(SamplerStates[i], StateName);
				i++;
			}
		}
	}
	
	// Set up some null buffers that will be used to reset parts of the API that
	// do no easily transition between compute and rasterization pipelines.
	{
		// ends up being all set to invalid/-1/D3D11_KEEP_UNORDERED_ACCESS_VIEWS 
		// which means leave value alone
		memset(NoopUAVInitialCounts, 0xff, sizeof(NoopUAVInitialCounts));

		// set to an array of nulls
		memset(NullUAVs, 0, sizeof(NullUAVs));
		memset(NullRTVs, 0, sizeof(NullRTVs));
	}

	// Set up a noop compute shader to flush for SetCounter()
	{
		oV(D3DDevice->CreateComputeShader(CSNoop, byte_code_size(CSNoop), nullptr, &NoopCS));

		sstring CSName;
		debug_name(CSName, D3DDevice);
		sncatf(CSName, "NoopCS");
		debug_name(NoopCS, CSName);
	}
}

void d3d11_device::intialize_immediate_context()
{
	ImmediateCommandList = std::make_shared<d3d11_command_list>(get_shared(), ImmediateContext, invalid);
}

d3d11_device::~d3d11_device()
{
	if (hHeap)
		HeapDestroy(hHeap);
}

device_info d3d11_device::get_info() const
{
	return Info;
}

const char* d3d11_device::name() const
{
	return Info.debug_name;
}

int d3d11_device::frame_id() const
{
	return FrameID;
}

std::shared_ptr<render_target> d3d11_device::make_primary_render_target(window* _pWindow, surface::format _DepthStencilFormat, bool _EnableOSRendering)
{
	if (SwapChain)
		oTHROW(protocol_error, "There already exists a primary render target, only one can exist for a given device at a time.");

	window_shape s = _pWindow->shape();
	if (has_statusbar(s.style))
		oTHROW_INVARG("A window used for rendering must not have a status bar");

	SwapChain = dxgi::make_swap_chain(D3DDevice
		, false
		, max(int2(1,1), s.client_size)
		, false
		, surface::b8g8r8a8_unorm
		, 0
		, 0
		, (HWND)_pWindow->native_handle()
		, _EnableOSRendering);

	sstring RTName;
	snprintf(RTName, "%s.PrimaryRT", name());

	return std::make_shared<d3d11_render_target>(get_shared(), name(), SwapChain, _DepthStencilFormat);
}

bool d3d11_device::map_read(resource* _pReadbackResource, int _Subresource, surface::mapped_subresource* _pMappedSubresource, bool _Blocking)
{
	uint D3DSubresourceIndex = 0;
	ID3D11Resource* r = get_subresource(_pReadbackResource, _Subresource, &D3DSubresourceIndex);

	D3D11_MAPPED_SUBRESOURCE msr;
	if (FAILED(ImmediateContext->Map(r, D3DSubresourceIndex, D3D11_MAP_READ, _Blocking ? 0 : D3D11_MAP_FLAG_DO_NOT_WAIT, &msr)))
		return false;
	if (!msr.pData)
		oTHROW0(no_buffer_space);
	_pMappedSubresource->data = msr.pData;
	_pMappedSubresource->row_pitch = msr.RowPitch;
	_pMappedSubresource->depth_pitch = msr.DepthPitch;
	return true;
}

void d3d11_device::unmap_read(resource* _pReadbackResource, int _Subresource)
{
	uint D3DSubresourceIndex = 0;
	ID3D11Resource* r = get_subresource(_pReadbackResource, _Subresource, &D3DSubresourceIndex);
	ImmediateContext->Unmap(r, D3DSubresourceIndex);
}

bool d3d11_device::read_query(query* _pQuery, void* _pData, uint _SizeofData)
{
	return static_cast<d3d11_query*>(_pQuery)->read_query(ImmediateContext, _pData, _SizeofData);
}

bool d3d11_device::begin_frame()
{
	FrameMutex.lock_shared();
	FrameID++;
	return true;
}

void d3d11_device::end_frame()
{
	draw_command_lists();
	FrameMutex.unlock_shared();
}

draw_context_handle d3d11_device::begin_os_frame()
{
	SwapChainMutex.lock_shared();
	if (!SwapChain)
	{
		SwapChainMutex.unlock_shared();
		return nullptr;
	}

	return (draw_context_handle)dxgi::get_dc(SwapChain);
}

void d3d11_device::end_os_frame()
{
	dxgi::release_dc(SwapChain);
	SwapChainMutex.unlock_shared();
}

bool d3d11_device::is_fullscreen_exclusive() const
{
	shared_lock<shared_mutex> lock(SwapChainMutex);
	if (!SwapChain)
		return false;

	BOOL FS = FALSE;
	const_cast<d3d11_device*>(this)->SwapChain->GetFullscreenState(&FS, nullptr);
	return !!FS;
}

void d3d11_device::set_fullscreen_exclusive(bool _Fullscreen)
{
	lock_guard<shared_mutex> lock(SwapChainMutex);
	if (!SwapChain)
		oTHROW(protocol_error, "no primary render target has been created");
	dxgi::set_fullscreen_exclusive(SwapChain, _Fullscreen);
}

void d3d11_device::present(uint _PresentInterval)
{
	lock_guard<shared_mutex> lock(SwapChainMutex);
	if (!SwapChain)
		oTHROW(operation_not_permitted, "present() must only be called on the primary render target");
	dxgi::present(SwapChain, _PresentInterval);
}

surface::mapped_subresource d3d11_device::reserve(ID3D11DeviceContext* _pDeviceContext, resource* _pResource, int _Subresource)
{
	uint2 ByteDimensions = _pResource->byte_dimensions(_Subresource);
	uint size = ByteDimensions.x * ByteDimensions.y;
	HeapLock(hHeap);
	void* p = HeapAlloc(hHeap, 0, size);
	HeapAllocations.push_back(p);
	HeapUnlock(hHeap);

	surface::mapped_subresource mapped;
	mapped.data = p;
	mapped.row_pitch = ByteDimensions.x;
	mapped.depth_pitch = size;

	return mapped;
}

void d3d11_device::commit(ID3D11DeviceContext* _pDeviceContext, resource* _pResource, int _Subresource, const surface::mapped_subresource& _Source, const surface::box& _Subregion)
{
	uint D3DSubresource = invalid;
	ID3D11Resource* pD3DResource = get_subresource(_pResource, _Subresource, &D3DSubresource);

	D3D11_BOX box;
	D3D11_BOX* pBox = nullptr;

	if (!_Subregion.empty())
	{
		uint StructureByteStride = 1;
		resource_type::value type = _pResource->type();
		if (type == resource_type::buffer)
		{
			buffer_info i = d3d11::get_info(static_cast<ID3D11Buffer*>(pD3DResource));
			StructureByteStride = __max(1, i.struct_byte_size);
			oASSERT(_Subregion.top == 0 && _Subregion.bottom == 1, "Buffer subregion must have top == 0 and bottom == 1");
		}

		box.left = _Subregion.left * StructureByteStride;
		box.top = _Subregion.top;
		box.right = _Subregion.right * StructureByteStride;
		box.bottom = _Subregion.bottom; 
		box.front = _Subregion.front;
		box.back = _Subregion.back;
		pBox = &box;
	}

	update_subresource(_pDeviceContext, pD3DResource, D3DSubresource, pBox, _Source, SupportsDeferredCommandLists);

	HeapLock(hHeap);
	if (find_and_erase(HeapAllocations, _Source.data))
		HeapFree(hHeap, 0, _Source.data);
	HeapUnlock(hHeap);
}

struct draw_order_equal
{
	int DrawOrder;
	draw_order_equal(int _DrawOrder) : DrawOrder(_DrawOrder) {}
	bool operator()(const command_list* _pCommandList)
	{
		command_list_info i = _pCommandList->get_info();
		return i.draw_order == DrawOrder;
	}
};

bool by_draw_order(const command_list* _pCommandList1, const command_list* _pCommandList2)
{
	command_list_info i1 = _pCommandList1->get_info();
	command_list_info i2 = _pCommandList2->get_info();
	return i1.draw_order > i2.draw_order;
};

void d3d11_device::insert(command_list* _pCommandList)
{
	std::lock_guard<std::mutex> lock(CommandListsInsertRemoveMutex);
	command_list_info i = _pCommandList->get_info();
	auto it = find_if(CommandLists, draw_order_equal(i.draw_order));
	if (it == CommandLists.end())
		sorted_insert(CommandLists, _pCommandList, by_draw_order);
	else
		oTHROW(operation_in_progress, "Entry with DrawOrder %d already exists", i.draw_order);
}

void d3d11_device::remove(command_list* _pCommandList)
{
	std::lock_guard<std::mutex> lock(CommandListsInsertRemoveMutex);
	find_and_erase(CommandLists, _pCommandList);
}

void d3d11_device::block_submission()
{
	CommandListsBeginEndMutex.lock_shared();
}

void d3d11_device::unblock_submission()
{
	CommandListsBeginEndMutex.unlock_shared();
}

void d3d11_device::draw_command_lists()
{
	std::lock_guard<std::mutex> lock(CommandListsInsertRemoveMutex);
	std::lock_guard<shared_mutex> lock2(CommandListsBeginEndMutex);

	for (command_list* cl : CommandLists)
	{
		d3d11_command_list* c = static_cast<d3d11_command_list*>(cl);
		if (c->CommandList)
		{
			ImmediateContext->ExecuteCommandList(c->CommandList, FALSE);
			c->CommandList = nullptr;
		}
	}
}

void d3d11_device::release_swap_chain()
{
	lock_guard<shared_mutex> lock(SwapChainMutex);
	SwapChain = nullptr;
}

oGPU_NAMESPACE_END
