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
#include <oPlatform/oTest.h>
#include "oGPUTestCommon.h"
#include <oGPU/texture3d.h>

using namespace ouro::gpu;

namespace ouro {
	namespace tests {

static const bool kIsDevMode = false;

struct gpu_test_texture3dmip : public gpu_texture_test
{
	gpu_test_texture3dmip() : gpu_texture_test("GPU test: texture3D mip", kIsDevMode) {}

	pipeline get_pipeline() override { pipeline p; p.input = gpu::intrinsic::vertex_layout::pos_uvw; p.vs = gpu::intrinsic::vertex_shader::texture3d; p.ps = gpu::intrinsic::pixel_shader::texture3d; return p; } 
	resource* make_test_texture() override
	{
		surface::info si;
		si.layout = surface::tight;
		si.format = surface::b8g8r8a8_unorm;
		si.dimensions = int3(64,64,64);
		surface::buffer image(si);
		{
			surface::lock_guard lock(image);
			surface::fill_color_cube((color*)lock.mapped.data, lock.mapped.row_pitch, lock.mapped.depth_pitch, si.dimensions);
		}
		image.generate_mips();

		t.initialize("Test 3D", Device, image, true);
		return &t;
	}

	texture3d t;
};

oGPU_COMMON_TEST(texture3dmip);

	} // namespace tests
} // namespace ouro
