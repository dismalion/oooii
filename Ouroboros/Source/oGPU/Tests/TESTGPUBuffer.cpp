// Copyright (c) 2014 Antony Arciuolo. See License.txt regarding use.
#include <oPlatform/oTest.h>
#include <oGPU/device.h>
#include <oGPU/shaders.h>
#include <oGPU/readback_buffer.h>
#include <oGPU/vertex_buffer.h>
#include <oGPU/rwstructured_buffer.h>

using namespace ouro::gpu;

namespace ouro {
	namespace tests {

void TESTbuffer()
{
	static int GPU_BufferAppendIndices[20] = { 5, 6, 7, 18764, 2452, 2423, 52354, 344, -1542, 3434, 53, -4535, 3535, 88884747, 534535, 88474, -445, 4428855, -1235, 4661};
	
	device_init init("GPU Buffer test");
	init.enable_driver_reporting = true;
	device Device(init);

	rwstructured_buffer AppendBuffer;
	AppendBuffer.initialize_append("BufferAppend", Device, sizeof(int), oCOUNTOF(GPU_BufferAppendIndices) * 2);

	readback_buffer AppendReadbackBuffer;
	AppendReadbackBuffer.initialize("BufferAppend", Device, sizeof(int), oCOUNTOF(GPU_BufferAppendIndices) * 2);

	readback_buffer AppendBufferCount;
	AppendBufferCount.initialize("BufferAppendCount", Device, sizeof(int));

	vertex_layout VL;
	VL.initialize("vertex layout", Device, gpu::intrinsic::elements(gpu::intrinsic::vertex_layout::pos), gpu::intrinsic::vs_byte_code(gpu::intrinsic::vertex_layout::pos));

	vertex_shader VS;
	VS.initialize("VS", Device, gpu::intrinsic::byte_code(gpu::intrinsic::vertex_shader::test_buffer));

	pixel_shader PS;
	PS.initialize("PS", Device, gpu::intrinsic::byte_code(gpu::intrinsic::pixel_shader::test_buffer));

	command_list& CommandList = Device.immediate();

	AppendBuffer.set_draw_target(CommandList, 0);

	VL.set(CommandList, mesh::primitive_type::points);
	VS.set(CommandList);
	PS.set(CommandList);
	vertex_buffer::draw_unindexed(CommandList, oCOUNTOF(GPU_BufferAppendIndices), 0);

	AppendBuffer.copy_counter_to(CommandList, AppendBufferCount, 0);

	int count = 0;
	AppendBufferCount.copy_to(&count, sizeof(int));
	oCHECK(oCOUNTOF(GPU_BufferAppendIndices) == count, "Append counter didn't reach %d", oCOUNTOF(GPU_BufferAppendIndices));

	AppendReadbackBuffer.copy_from(CommandList, AppendBuffer);

	std::vector<int> Values(20);
	oCHECK(AppendReadbackBuffer.copy_to(Values.data(), Values.size() * sizeof(int)), "Copy out of readback buffer failed");

	for(int i = 0; i < oCOUNTOF(GPU_BufferAppendIndices); ++i)
		oCHECK(find_and_erase(Values, GPU_BufferAppendIndices[i]), "GPU Appended bad value");
}

	}
}
