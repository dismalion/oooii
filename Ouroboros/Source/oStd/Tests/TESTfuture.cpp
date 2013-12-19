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
#include <oConcurrency/oConcurrency.h>
#include <oStd/for.h>
#include <oStd/future.h>
#include <oCore/process_stats_monitor.h>

#include "../../test_services.h"

#define oSTD_THROW(_SystemError, _Message) do { std::error_code ec = std::make_error_code(std::errc::_SystemError); throw std::system_error(ec, _Message); } while(false)

namespace oStd {
	namespace tests {

static void exercise_thread(size_t _Index, int* _pResults, unsigned int _RuntimeMS)
{
	int n = 0;
	double t = oStd::chrono::high_resolution_clock::now().time_since_epoch().count();
	double timeout = t + _RuntimeMS / 1000.0;
	while (t < timeout)
	{
		n += rand();
		t = oStd::chrono::high_resolution_clock::now().time_since_epoch().count();
	}

	_pResults[_Index] = n;
}

static bool exercise_all_threads()
{
	const int nTasks = 5 * oStd::thread::hardware_concurrency(); // ensure more work than the number of threads.
	int* results = (int*)_alloca(nTasks * sizeof(int));
	memset(results, -1, nTasks * sizeof(int));

	oConcurrency::parallel_for(0, size_t(nTasks), std::bind(exercise_thread, std::placeholders::_1, results, 1500));
	for (int i = 0; i < nTasks; i++)
		oCHECK(results[i] != -1, "Invalid results from parallel_for");
	return true;
}

static bool fail_and_report()
{
	if (1)
		oSTD_THROW(not_supported, "not supported");
	return false;
}

static void test_workstealing(ouro::test_services& _Services)
{
	float CPUavg = 0.0f, CPUpeak = 0.0f;
	oStd::future<bool> Result = oStd::async(exercise_all_threads);

	oTRACE("Waiting for result...");
	bool r = Result.get();
	oCHECK(r, "future returned, but the algo returned the wrong result");

	_Services.get_cpu_utilization(&CPUavg, &CPUpeak);
	if (CPUpeak <= 99.0f)
	{
		float CPUpeak2 = 0.0f;
		_Services.reset_cpu_utilization();
		oStd::this_thread::sleep_for(oStd::chrono::seconds(10));
		_Services.get_cpu_utilization(&CPUavg, &CPUpeak2);
		if (CPUpeak2 > 5.0f)
			oSTD_THROW(permission_denied, "There is too much CPU activity currently on the system to properly judge oStdFuture's workstealing capabilities.");
		else
		{
			char buf[128];
			snprintf(buf, "Failed to achieve 100%s CPU utilization. Peaked at %.01f%s", "%%", CPUpeak, "%%");
			oSTD_THROW(protocol_error, buf);
		}
	}
}

void TESTfuture(ouro::test_services& _Services)
{
	// Test packaged_task with void return type
	{
		oStd::packaged_task<void(int, int, char*)> test_no_return([&](int _Param1, int _Param2, char*_Param3){});
		test_no_return(1,2,"t");
		test_no_return.get_future().get();
		
		// Test if reset works
		test_no_return.reset();
		test_no_return(3,4,"t");
		test_no_return.get_future().get();
	}

	// Test packaged_task with a return type
	{
		oStd::packaged_task<int(int, const char*, int)> hmmm([&](int _Param1, const char* _Param2, int _Param3)->int{ return _Param1 + _Param3; });
		// Get future before execution
		oStd::future<int> hmmfuture = hmmm.get_future();
		hmmm(10, "a", 20);
		oCHECK(hmmfuture.get() == 30, "Unexpected result1");

		// Test if reset works
		hmmm.reset();
		hmmm(20, "b", 30);
		// Get future after execution
		hmmfuture = hmmm.get_future();
		oCHECK(hmmfuture.get() == 50, "Unexpected result2");
	}

	// Test swapping packaged_tasks
	{
		oStd::packaged_task<int(int,int)> tasktest1([&](int _Param1, int _Param2)->int{ return _Param1 + _Param2; });
		oStd::packaged_task<int(int,int)> tasktest2([&](int _Param1, int _Param2)->int{ return _Param1 - _Param2; });

		oCHECK(tasktest1.valid() && tasktest2.valid(), "oStd::packaged_task should have been valid");

		tasktest1.swap(tasktest2);

		oStd::future<int> tasktest1_future = tasktest1.get_future();
		tasktest1(20, 10);

		tasktest2(20, 10);
		oStd::future<int> tasktest2_future = tasktest2.get_future();

		// tasktest1 should subtract
		oCHECK(tasktest1_future.get() == 10, "Unexpected result3");

		// tasktest2 should add
		oCHECK(tasktest2_future.get() == 30, "Unexpected result4");
	}

	// Test a packaged_task through async with maximum number of arguments
	// (std::bind is apparently limited to 10)
	{
		oStd::future<bool> Result2 = oStd::async((std::function<bool(int,int,int,int,int,int,int,int,int,int)>)[&](int _Param1,int _Param2,int _Param3,int _Param4,int _Param5,int _Param6,int _Param7,int _Param8,int _Param9,int _Param10)->bool
		{ 
			if (_Param10 == 10)
				return true; 
			else 
				return false; 
		}, 1,2,3,4,5,6,7,8,9,10);

		oCHECK(Result2.get(), "Unexpected result5");
	}

	// test failure
	{
		oStd::future<bool> FutureToFail = oStd::async(fail_and_report);

		bool ThisShouldFail = true;
		try { ThisShouldFail = FutureToFail.get(); }
		catch (std::system_error& e)
		{
			oCHECK(e.code() == std::errc::not_supported, "error code not properly set");
			ThisShouldFail = false;
		}

		oCHECK(ThisShouldFail == false, "Error reporting failed");
	}

	test_workstealing(_Services);
};

	} // namespace tests
} // namespace oConcurrency
