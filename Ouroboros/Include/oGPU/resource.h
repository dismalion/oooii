// Copyright (c) 2014 Antony Arciuolo. See License.txt regarding use.
#pragma once
#ifndef oGPU_resource_h
#define oGPU_resource_h

#include <oBase/types.h>

namespace ouro { namespace gpu {

class command_list;

class resource
{
public:
	resource() : ro(nullptr) {}
	resource(resource&& that) : ro(that.ro) { that.ro = nullptr; }
	resource& operator=(resource&& that) { if (this != &that) { ro = that.ro; that.ro = nullptr; } }

	char* name(char* dst, size_t dst_size) const;
	void* get_buffer() const;
	inline void* get_resource() const { return ro; }

	static void set(command_list& cl, uint slot, uint num_resources, resource* const* resources);
	void set(command_list& cl, uint slot);

	void set_highest_mip(command_list& cl, float mip);

	void update(command_list& cl, uint subresource, const surface::const_mapped_subresource& src, const surface::box& region = surface::box());
	inline void update(command_list& cl, uint subresource, const surface::mapped_subresource& src, const surface::box& region = surface::box()) 
		{ update(cl, subresource, (const surface::const_mapped_subresource&)src, region); }

protected:
	void* ro;

private:
	resource(const resource&);
	const resource& operator=(const resource&);
};
	
// @tony: not sure where this goes yet, but this header is included by all render targets
struct viewport
{
	// if this is specified, use the target's dimensions
	static const uint default_value = ~0u;

	viewport()
		: left(0)
		, top(0)
		, width(default_value)
		, height(default_value)
		, min_depth(0.0f)
		, max_depth(1.0f)
	{}

	viewport(uint _width, uint _height)
		: left(0)
		, top(0)
		, width(_width)
		, height(_height)
		, min_depth(0.0f)
		, max_depth(1.0f)
	{}

	viewport(const uint2& dimensions)
		: left(0)
		, top(0)
		, width(dimensions.x)
		, height(dimensions.y)
		, min_depth(0.0f)
		, max_depth(1.0f)
	{}

	uint left;
	uint top;
	uint width;
	uint height;
	float min_depth;
	float max_depth;
};

// @tony: not yet refactored, this is meant only for internal implementation of render target apis
void set_viewports(command_list& cl, const uint2& default_dimensions, const viewport* oRESTRICT viewports, uint num_viewports);

}}

#endif
