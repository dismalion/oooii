// Copyright (c) 2014 Antony Arciuolo. See License.txt regarding use.
// Set/Get a name that shows up in driver tty.
#pragma once
#ifndef oGPU_d3d_debug_h
#define oGPU_d3d_debug_h

#include "d3d_types.h"
#include "d3d11sdklayers.h"
#include <oCore/windows/win_error.h>
#include <oCore/windows/win_util.h>

namespace ouro {
	namespace gpu {
		namespace d3d {

// Set/Get a name that appears in D3D11's debug layer
void debug_name(Device* dev, const char* name);
void debug_name(DeviceChild* child, const char* name);

char* debug_name(char* _StrDestination, size_t _SizeofStrDestination, const Device* dev);
template<size_t size> char* debug_name(char (&_StrDestination)[size], const Device* dev) { return debug_name(_StrDestination, size, dev); }
template<size_t capacity> char* debug_name(fixed_string<char, capacity>& _StrDestination, const Device* dev) { return debug_name(_StrDestination, _StrDestination.capacity(), dev); }

// Fills the specified buffer with the string set with oD3D11SetDebugName().
char* debug_name(char* _StrDestination, size_t _SizeofStrDestination, const	DeviceChild* child);
template<size_t size> char* debug_name(char (&_StrDestination)[size], const DeviceChild* child) { return debug_name(_StrDestination, size, child); }
template<size_t capacity> char* debug_name(fixed_string<char, capacity>& _StrDestination, const DeviceChild* child) { return debug_name(_StrDestination, _StrDestination.capacity(), child); }

// Use InfoQueue::AddApplicationMessage to trace user errors.
int vtrace(InfoQueue* iq, D3D11_MESSAGE_SEVERITY severity, const char* format, va_list args);
inline int vtrace(Device* dev, D3D11_MESSAGE_SEVERITY severity, const char* format, va_list args) { intrusive_ptr<InfoQueue> InfoQueue; oV(dev->QueryInterface(__uuidof(InfoQueue), (void**)&InfoQueue)); return vtrace(InfoQueue, severity, format, args); }
inline int vtrace(DeviceContext* dc, D3D11_MESSAGE_SEVERITY severity, const char* format, va_list args) { intrusive_ptr<Device> Device; dc->GetDevice(&Device); return vtrace(Device, severity, format, args); }
inline int trace(InfoQueue* iq, D3D11_MESSAGE_SEVERITY severity, const char* format, ...) { va_list args; va_start(args, format); int len = vtrace(iq, severity, format, args); va_end(args); return len; }
inline int trace(Device* dev, D3D11_MESSAGE_SEVERITY severity, const char* format, ...) { va_list args; va_start(args, format); int len = vtrace(dev, severity, format, args); va_end(args); return len; }
inline int trace(DeviceContext* dc, D3D11_MESSAGE_SEVERITY severity, const char* format, ...) { va_list args; va_start(args, format); int len = vtrace(dc, severity, format, args); va_end(args); return len; }

		} // namespace d3d
	} // namespace gpu
}

#endif
