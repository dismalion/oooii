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
#include <oBasis/oOSC.h>
#include <oBase/algorithm.h>
#include <oBase/memory.h>
#include <oBasis/oError.h>
#include "oBasisTestStruct.h"
#include <vector>

using namespace ouro::osc;

namespace ouro {
	namespace tests {

static const char* type_tags() { return ",Tc3Tih[fff]iTTs2f1sicbsi[iiii]dIfbt"; }

void TESTosc()
{
	// Set up test object and a reference object we'll test against after going
	// through the OSC serialization and deserialization.
	char b1[10];
	memset(b1, 21, sizeof(b1));
	char b2[20];
	memset(b2, 22, sizeof(b2));

	test_struct sent, received;
	memset4(&sent, 0xdeadc0de, sizeof(sent));
	memset4(&received, 0xdeadc0de, sizeof(received));
	init_test_struct(&sent, b1, sizeof(b1), b2, sizeof(b2));

	const char* _MessageName = "/TESTOSC/Run/TEST/sent";

	oCHECK(((void*)(&sent.c2 + 1) != (void*)(&sent.b1size)), "Expected padding");

	size_t TestSize = sizeof(test_struct);
	oCHECK(TestSize == calc_deserialized_struct_size(type_tags()), "calc_deserialized_struct_size failed to compute correct size");

	static const size_t kExpectedArgsSize = 216;

	size_t argsSize = calc_args_data_size(type_tags(), sent);
	oCHECK(argsSize == kExpectedArgsSize, "calc_args_data_size failed to compute correct size");

	size_t msgSize = calc_msg_size(_MessageName, type_tags(), argsSize);

	{
		std::vector<char> ScopedBuffer(msgSize);
		size_t SerializedSize = serialize_struct_to_msg(_MessageName, type_tags(), sent, ScopedBuffer.data(), ScopedBuffer.size());
		oCHECK(SerializedSize > 0, "Failed to serialize buffer");
		oCHECK(deserialize_msg_to_struct(data(ScopedBuffer), &received), "Deserialization failed");

		// NOTE: Remember that received has string and blob pointers that point
		// DIRECTLY into the ScopedBuffer, so either the client code here needs to
		// copy those values to their own buffers, or received can only remain in
		// the scope of the ScopedBuffer.
		oCHECK(sent == received, "Sent and received buffers do not match");
	}
}

	} // namespace tests
} // namespace ouro

using namespace ouro;
using namespace ouro::osc;

bool oBasisTest_oOSC()
{
	try { ouro::tests::TESTosc(); }
	catch (std::system_error& e)
	{
		return oErrorSetLast(e.code().value(), e.what());
	}

	oErrorSetLast(0, "");
	return true;
}
