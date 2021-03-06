// Copyright (c) 2014 Antony Arciuolo. See License.txt regarding use.
#include <oCompiler.h>
#include <oMemory/tlsf_allocator.h>
#include <vector>
#include "../../test_services.h"

namespace ouro {
	namespace tests {

void TESTtlsf_allocator(test_services& services)
{
	bool EnoughPhysRamForFullTest = true;

	#if o64BIT == 1
		// Allocating more memory than physically available is possible,
		// but really slowing, so leave some RAM for the operating system.

		// On machines with less memory, it's not a good idea to use all of it
		// because the system would need to page out everything it has to allocate
		// that much memory, which makes the test take many minutes to run.
		size_t ArenaSize = __min(size_t(services.total_physical_memory() * 0.15f), oMB(4500));
		EnoughPhysRamForFullTest = (ArenaSize > oGB(4));
	#else
		const size_t ArenaSize = oMB(500);
	#endif

	services.report("Allocating %u MB arena using CRT... (SLOW! OS has to defrag virtual memory to get a linear run of this size)", ArenaSize/(1024*1024));

	std::vector<char> arena;
	{
		test_services::scoped_timer t(services, "std::vector allocation");
		arena.resize(ArenaSize); // should be bigger than 32-bit's 4 GB limitation
	}

	tlsf_allocator a(arena.data(), arena.size());

	const size_t NUM_POINTER_TESTS = 1000;
	std::vector<char*> pointers(NUM_POINTER_TESTS);
	memset(pointers.data(), 0, sizeof(char*) * pointers.size());

	size_t totalUsed = 0;
	size_t smallestAlloc = size_t(-1);
	size_t largestAlloc = 0;

	for (size_t numRuns = 0; numRuns < 1; numRuns++)
	{
		totalUsed = 0;

		// allocate some pointers
		for (size_t i = 0; i < NUM_POINTER_TESTS; i++)
		{
			size_t s = 0;
			size_t r = rand();
			size_t limitation = r % 3;
			switch (limitation)
			{
				default:
				case 0:
					s = r; // small
					break;
				case 1:
					s = r * 512; // med
					break;
				case 2:
					s = r * 8 * 1024; // large
					break;
			}

			float percentUsed = round(100.0f * totalUsed/(float)ArenaSize);
			float PercentAboutToBeUsed = round(100.0f * (totalUsed + s)/(float)ArenaSize);

			if (PercentAboutToBeUsed < 97.0f && percentUsed < 97.0f) // TLSF is expected to have ~3% fragmentation
			{
				pointers[i] = (char*)a.allocate(s);
				oTEST(pointers[i], "Failed on allocate %u of %u bytes (total used %0.1f%%)", i, s, percentUsed);
				totalUsed += s;
				smallestAlloc = __min(smallestAlloc, s);
				largestAlloc = __max(largestAlloc, s);
				oTEST(a.valid(), "Heap corrupt on allocate %u of %u bytes.", i, s);
			}
		}

		// shuffle pointer order
		for (size_t i = 0; i < NUM_POINTER_TESTS; i++)
		{
			size_t r = rand() % NUM_POINTER_TESTS;
			char* p = pointers[i];
			pointers[i] = pointers[r];
			pointers[r] = p;
		}

		// free out of order
		for (size_t i = 0; i < NUM_POINTER_TESTS; i++)
		{
			a.deallocate(pointers[i]);
			pointers[i] = 0;
			oTEST(a.valid(), "Heap corrupt on deallocate %u.", i);
		}
	}

	// test really small
	void* p1 = a.allocate(1);
	oTEST(a.valid(), "Heap corrupt on allocate of %u.", 1);
	void* p2 = a.allocate(0);
	oTEST(a.valid(), "Heap corrupt on allocate of %u.", 0);
	a.deallocate(p2);
	oTEST(a.valid(), "Heap corrupt on deallocate");
	a.deallocate(p1);
	oTEST(a.valid(), "Heap corrupt on deallocate");

	// Fill out statistics and report
	services.report("%sRAMused: %u bytes, minsize:%u bytes, maxsize:%u bytes"
		, EnoughPhysRamForFullTest ? "" : "WARNING: system memory not enough to run full test quickly. "
		, ArenaSize, smallestAlloc, largestAlloc);
}
	}
}

