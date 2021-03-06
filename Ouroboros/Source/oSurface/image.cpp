// Copyright (c) 2014 Antony Arciuolo. See License.txt regarding use.
#include <oSurface/image.h>
#include <oSurface/convert.h>
#include <oMemory/memory.h>
#include <oBase/throw.h>
#include <mutex>

namespace ouro { namespace surface {

image::image(image&& that) 
	: bits(that.bits)
	, inf(that.inf)
	, alloc(that.alloc)
{
	that.bits = nullptr;
	that.inf = info();
	that.alloc = allocator();
}

image& image::operator=(image&& that)
{
	if (this != &that)
	{
		mtx.lock();
		that.mtx.lock();
		deinitialize();
		bits = that.bits; that.bits = nullptr; 
		inf = that.inf; that.inf = info();
		alloc = that.alloc; that.alloc = allocator();
		that.mtx.unlock();
		mtx.unlock();
	}
	return *this;
}

void image::initialize(const info& i, const allocator& a)
{
	deinitialize();
	inf = i;
	alloc = a;
	bits = alloc.allocate(size(), memory_alignment::align_default, "image");
}

void image::initialize(const info& i, const void* data, const allocator& a)
{
	deinitialize();
	inf = i;
	alloc = a;
	bits = (void*)data;
}

void image::initialize_array(const image* const* sources, uint num_sources, bool mips)
{
	deinitialize();
	info si = sources[0]->get_info();
	oCHECK_ARG(si.mip_layout == mip_layout::none, "all images in the specified array must be simple types and the same 2D dimensions");
	oCHECK_ARG(si.dimensions.z == 1, "all images in the specified array must be simple types and the same 2D dimensions");
	si.mip_layout = mips ? mip_layout::tight : mip_layout::none;
	si.array_size = static_cast<int>(num_sources);
	initialize(si);

	const uint nMips = num_mips(mips, si.dimensions);
	const uint nSlices = max(1u, si.array_size);
	for (uint i = 0; i < nSlices; i++)
	{
		int dst = calc_subresource(0, i, 0, nMips, nSlices);
		int src = calc_subresource(0, 0, 0, nMips, 0);
		copy_from(dst, *sources[i], src);
	}

	if (mips)
		generate_mips();
}

void image::initialize_3d(const image* const* sources, uint num_sources, bool mips)
{
	deinitialize();
	info si = sources[0]->get_info();
	oCHECK_ARG(si.mip_layout == mip_layout::none, "all images in the specified array must be simple types and the same 2D dimensions");
	oCHECK_ARG(si.dimensions.z == 1, "all images in the specified array must be simple types and the same 2D dimensions");
	oCHECK_ARG(si.array_size == 0, "arrays of 3d surfaces not yet supported");
	si.mip_layout = mips ? mip_layout::tight : mip_layout::none;
	si.dimensions.z = static_cast<int>(num_sources);
	si.array_size = 0;
	initialize(si);

	box region;
	region.right = si.dimensions.x;
	region.bottom = si.dimensions.y;
	for (uint i = 0; i < si.dimensions.z; i++)
	{
		region.front = i;
		region.back = i + 1;
		shared_lock lock(sources[i]);
		update_subresource(0, region, lock.mapped);
	}

	if (mips)
		generate_mips();
}

void image::deinitialize()
{
	if (bits && alloc.deallocate)
		alloc.deallocate(bits);
	bits = nullptr;
	alloc = allocator();
}

void image::clear()
{
	lock_t lock(mtx);
	memset(bits, 0, size());
}

void image::flatten()
{
	if (is_block_compressed(inf.format))
		oTHROW(not_supported, "block compressed formats not handled yet");

	int rp = row_pitch(inf);
	size_t sz = size();
	inf.mip_layout = mip_layout::none;
	inf.dimensions = int3(rp / element_size(inf.format), int(sz / rp), 1);
	inf.array_size = 0;
}

void image::update_subresource(uint subresource, const const_mapped_subresource& src, const copy_option& option)
{
	uint2 bd;
	mapped_subresource dst = get_mapped_subresource(inf, subresource, 0, bits, &bd);
	lock_t lock(mtx);
	memcpy2d(dst.data, dst.row_pitch, src.data, src.row_pitch, bd.x, bd.y, option == copy_option::flip_vertically);
}

void image::update_subresource(uint subresource, const box& _box, const const_mapped_subresource& src, const copy_option& option)
{
	if (is_block_compressed(inf.format) || inf.format == format::r1_unorm)
		throw std::invalid_argument("block compressed and bit formats not supported");

	uint2 bd;
	mapped_subresource Dest = get_mapped_subresource(inf, subresource, 0, bits, &bd);

	const auto NumRows = _box.height();
	auto PixelSize = element_size(inf.format);
	auto RowSize = PixelSize * _box.width();

	// Dest points at start of subresource, so offset to subrect of first slice
	Dest.data = byte_add(Dest.data, _box.front * Dest.depth_pitch + _box.top * Dest.row_pitch + _box.left * PixelSize);

	const void* pSource = src.data;

	lock_t lock(mtx);
	for (uint slice = _box.front; slice < _box.back; slice++)
	{
		memcpy2d(Dest.data, Dest.row_pitch, pSource, src.row_pitch, RowSize, NumRows, option == copy_option::flip_vertically);
		Dest.data = byte_add(Dest.data, Dest.depth_pitch);
		pSource = byte_add(pSource, src.depth_pitch);
	}
}

void image::map(uint subresource, mapped_subresource* _pMapped, uint2* out_byte_dimensions)
{
	mtx.lock();
	try
	{
		*_pMapped = get_mapped_subresource(inf, subresource, 0, bits, out_byte_dimensions);
	}

	catch (std::exception&)
	{
		auto e = std::current_exception();
		mtx.unlock();
		std::rethrow_exception(e);
	}
}

void image::unmap(uint subresource)
{
	mtx.unlock();
}

void image::map_const(uint subresource, const_mapped_subresource* _pMapped, uint2* out_byte_dimensions) const
{
	lock_shared();
	*_pMapped = get_const_mapped_subresource(inf, subresource, 0, bits, out_byte_dimensions);
}

void image::unmap_const(uint subresource) const
{
	unlock_shared();
}

void image::copy_to(uint subresource, const mapped_subresource& dst, const copy_option& option) const
{
	uint2 bd;
	const_mapped_subresource src = get_const_mapped_subresource(inf, subresource, 0, bits, &bd);
	lock_shared_t lock(mtx);
	memcpy2d(dst.data, dst.row_pitch, src.data, src.row_pitch, bd.x, bd.y, option == copy_option::flip_vertically);
}

image image::convert(const info& dst_info) const
{
	return convert(dst_info, alloc);
}

image image::convert(const info& dst_info, const allocator& a) const
{
	info src_info = get_info();
	image converted(dst_info, a);
	shared_lock slock(this);
	lock_guard dlock(converted);
	surface::convert(src_info, slock.mapped, dst_info, dlock.mapped);
	return converted;
}

void image::convert_to(uint subresource, const mapped_subresource& dst, const format& dst_format, const copy_option& option) const
{
	if (inf.format == dst_format)
		copy_to(subresource, dst, option);
	else
	{
		shared_lock slock(this, subresource);
		info src_info = get_info();
		info dst_info = src_info;
		dst_info.format = dst_format;
		surface::convert(src_info, slock.mapped, dst_info, (mapped_subresource&)dst, option);
	}
}

void image::convert_from(uint subresource, const const_mapped_subresource& src, const format& src_format, const copy_option& option)
{
	if (inf.format == src_format)
		copy_from(subresource, src, option);
	else
	{
		info src_info = inf;
		src_info.format = src_format;
		subresource_info sri = surface::subresource(src_info, subresource);
		lock_guard lock(this);
		convert_subresource(sri, src, inf.format, lock.mapped, option);
	}
}

void image::convert_in_place(const format& fmt)
{
	lock_guard lock(this);
	convert_swizzle(inf, fmt, lock.mapped);
	inf.format = fmt;
}

void image::generate_mips(const filter& f)
{
	lock_t lock(mtx);

	uint nMips = num_mips(inf);
	uint nSlices = max(1u, inf.array_size);

	for (uint slice = 0; slice < nSlices; slice++)
	{
		int mip0subresource = calc_subresource(0, slice, 0, nMips, inf.array_size);
		const_mapped_subresource mip0 = get_const_mapped_subresource(inf, mip0subresource, 0, bits);

		for (uint mip = 1; mip < nMips; mip++)
		{
			uint subresource = calc_subresource(mip, slice, 0, nMips, inf.array_size);
			subresource_info subinfo = surface::subresource(inf, subresource);

			for (uint depth = 0; depth < subinfo.dimensions.z; depth++)
			{
				mapped_subresource dst = get_mapped_subresource(inf, subresource, depth, bits);
				info di = inf;
				di.dimensions = subinfo.dimensions;
				resize(inf, mip0, di, dst, f);
			}
		}
	}
}

float calc_rms(const image& b1, const image& b2)
{
	return calc_rms(b1, b2, nullptr);
}

float calc_rms(const image& b1, const image& b2, image* out_diffs, int diff_scale, const allocator& a)
{
	info si1 = b1.get_info();
	info si2 = b2.get_info();

	if (any(si1.dimensions != si2.dimensions)) throw std::invalid_argument("mismatched dimensions");
	if (si1.format != si2.format) throw std::invalid_argument("mismatched format");
	if (si1.array_size != si2.array_size) throw std::invalid_argument("mismatched array_size");
	int n1 = num_subresources(si1);
	int n2 = num_subresources(si2);
	if (n1 != n2) throw std::invalid_argument("incompatible layouts");

	info dsi;
	if (out_diffs)
	{
		dsi = si1;
		dsi.format = format::r8_unorm;
		out_diffs->initialize(dsi);
	}

	float rms = 0.0f;
	for (int i = 0; i < n1; i++)
	{
		mapped_subresource msr;
		if (out_diffs)
			out_diffs->map(i, &msr);

		shared_lock lock1(b1, i);
		shared_lock lock2(b2, i);
	
		if (out_diffs)
			rms += calc_rms(si1, lock1.mapped, lock2.mapped, dsi, msr);
		else
			rms += calc_rms(si1, lock1.mapped, lock2.mapped);

		if (out_diffs)
			out_diffs->unmap(i);
	}

	return rms / static_cast<float>(n1);
}

	} // namespace surface
}
