// Copyright (c) 2014 Antony Arciuolo. See License.txt regarding use.
#include <oConcurrency/concurrency.h>
#include <oConcurrency/threadpool.h>
#include <oConcurrency/tests/oConcurrencyTests.h>
#include <oMemory/byte.h>
#include <oString/fixed_string.h>
#include <oCore/thread_traits.h>
#include <atomic>
#include "../../test_services.h"

namespace ouro { namespace tests {

template<typename ThreadpoolT> void test_basics(test_services& services, ThreadpoolT& t)
{
	oTEST(t.joinable(), "Threadpool is not joinable");

	{
		std::atomic<int> value(-1);
		t.dispatch([&] { value++; });
		t.flush();
		oTEST(value == 0, "Threadpool did not execute a single task correctly.");
	}

	static const int kNumDispatches = 100;
	{
		std::atomic<int> value(0);
		for (int i = 0; i < kNumDispatches; i++)
			t.dispatch([&] { value++; });

		t.flush();
		oTEST(value == kNumDispatches, "Threadpool did not dispatch correctly, or failed to properly block on flush");
	}
}

template<typename TaskGroupT> static void test_task_group(test_services& services, TaskGroupT& g)
{
	static const int kNumRuns = 100;
	{
		std::atomic<int> value = 0;
		for (size_t i = 0; i < kNumRuns; i++)
			g.run([&] { value++; });

		g.wait();
		std::atomic_thread_fence(std::memory_order_seq_cst);
		oTEST(value == kNumRuns, "std::function<void()>group either failed to run or failed to block on wait (1st run)");
	}

	{
		std::atomic<int> value = 0;
		for (int i = 0; i < kNumRuns; i++)
			g.run([&] { value++; });

		g.wait();
		std::atomic_thread_fence(std::memory_order_seq_cst);
		oTEST(value == kNumRuns, "std::function<void()>group was not able to be reused or either failed to run or failed to block on wait (2nd run)");
	}
}

static void set_value(size_t _Index, size_t* _pArray)
{
	_pArray[_Index] = 1;
}

template<typename ThreadpoolT> static void test_parallel_for(test_services& services, ThreadpoolT& thdpool)
{
	static const size_t kFullRange = 100;

	size_t Results[kFullRange];
	memset(Results, 0, kFullRange * sizeof(size_t));

	ouro::detail::parallel_for<16>(thdpool, 10, kFullRange - 10, std::bind(set_value, std::placeholders::_1, Results));
	for (size_t i = 0; i < 10; i++)
		oTEST(Results[i] == 0, "wrote out of range at beginning");

	for (size_t i = 10; i < kFullRange-10; i++)
		oTEST(Results[i] == 1, "bad write in main set_value loop");

	for (size_t i = kFullRange-10; i < kFullRange; i++)
		oTEST(Results[i] == 0, "wrote out of range at end");
}

template<typename ThreadpoolT> static void TestT(test_services& services)
{
	ThreadpoolT t;
	test_services::finally OSE([&] { if (t.joinable()) t.join(); });

	test_basics(services, t);
	t.join();
	oTEST(!t.joinable(), "Threadpool was joined, but remains joinable()");
}

void TESTthreadpool(test_services& services)
{
	TestT<threadpool<core_thread_traits>>(services);
}

void TESTtask_group(test_services& services)
{
	threadpool<core_thread_traits> t;
	test_services::finally OSE([&] { if (t.joinable()) t.join(); });

	detail::task_group<core_thread_traits> g(t);
	test_task_group(services, g);
	
	test_parallel_for(services, t);
}

namespace RatcliffJobSwarm {

#if 1
	// Settings used in John Ratcliffe's code
	#define FRACTAL_SIZE 2048
	#define SWARM_SIZE 8
	#define MAX_ITERATIONS 65536
#else
	// Super-soak test... thread_pool tests take about ~30 sec each
	#define FRACTAL_SIZE 16384
	#define SWARM_SIZE 64
	#define MAX_ITERATIONS 65536
#endif

#define TILE_SIZE ((FRACTAL_SIZE)/(SWARM_SIZE))

//********************************************************************************
// solves a single point in the mandelbrot set.
//********************************************************************************
static inline unsigned int mandelbrotPoint(unsigned int iterations,double real,double imaginary)
{
	double fx,fy,xs,ys;
	unsigned int count;

  double two(2.0);

	fx = real;
	fy = imaginary;
  count = 0;

  do
  {
    xs = fx*fx;
    ys = fy*fy;
		fy = (two*fx*fy)+imaginary;
		fx = xs-ys+real;
    count++;
	} while ( xs+ys < 4.0 && count < iterations);

	return count;
}

static inline unsigned int solvePoint(unsigned int x,unsigned int y,double x1,double y1,double xscale,double yscale)
{
  return mandelbrotPoint(MAX_ITERATIONS,(double)x*xscale+x1,(double)y*yscale+y1);
}

static void MandelbrotTask(size_t _Index, void* _pData, double _FX, double _FY, double _XScale, double _YScale)
{
	unsigned int Xs = static_cast<unsigned int>(_Index % TILE_SIZE);
	unsigned int Ys = static_cast<unsigned int>(_Index / TILE_SIZE);
	const size_t stride = FRACTAL_SIZE * sizeof(unsigned char);
	const size_t offset = Ys*stride + Xs*sizeof(unsigned char);
	unsigned char* fractal_image = byte_add((unsigned char*)_pData, offset);
	for (unsigned int y=0; y<SWARM_SIZE; y++)
	{
		unsigned char* pDest = byte_add(fractal_image, stride*y);
		for (unsigned int x=0; x<SWARM_SIZE; x++)
		{
			unsigned int v = 0xff & solvePoint(x+Xs,y+Ys,_FX,_FY,_XScale,_YScale);
			*pDest++ = (unsigned char)v;
		}
	}
}

#pragma fenv_access (on)

#ifdef _DEBUG
	#define DEBUG_DISCLAIMER "(DEBUG: Non-authoritive) "
#else
	#define DEBUG_DISCLAIMER
#endif

} // namespace RatcliffJobSwarm

void TESTthreadpool_performance(ouro::test_services& services, test_threadpool& thdpool)
{
	const unsigned int taskRow = TILE_SIZE;
	const unsigned int taskCount = taskRow*taskRow;
	const double x1 = -0.56017680903960034334758968;
	const double x2 = -0.5540396934395273995800156;
	const double y1 = -0.63815211573948702427222672;
	const double y2 = y1+(x2-x1);
	const double xscale = (x2-x1)/(double)FRACTAL_SIZE;
	const double yscale = (y2-y1)/(double)FRACTAL_SIZE;
	std::vector<unsigned char> fractal;
	fractal.resize(FRACTAL_SIZE*FRACTAL_SIZE);

	const char* n = thdpool.name() ? thdpool.name() : "(null)";

	services.report("%s::dispatch()...", n);
	test_services::timer t(services);
	for (unsigned int y=0; y<TILE_SIZE; y++)
		for (unsigned int x=0; x<TILE_SIZE; x++)
			thdpool.dispatch(std::bind(RatcliffJobSwarm::MandelbrotTask
			, y*TILE_SIZE + x, fractal.data(), x1, y1, xscale, yscale));

	double scheduling_time = t.seconds();

	thdpool.flush();
	double execution_time = t.seconds();

	services.report("%s::parallel_for()...", n);
	t.reset();
	bool DidParallelFor = thdpool.parallel_for(0, taskCount
		, std::bind(RatcliffJobSwarm::MandelbrotTask, std::placeholders::_1, fractal.data(), x1, y1, xscale, yscale));

	double parallel_for_time = t.seconds();
	const char* DebuggerDisclaimer = services.is_debugger_attached() ? "(DEBUGGER ATTACHED: Non-authoritive) " : "";

	sstring st, et, pt;
	format_duration(st, scheduling_time, true, true);
	format_duration(et, execution_time, true, true);
	format_duration(pt, parallel_for_time, true, true);

	services.report(DEBUG_DISCLAIMER 
		"%s%s: dispatch %s (%s sched), parallel_for %s"
		, DebuggerDisclaimer, n, et.c_str(), st.c_str(), DidParallelFor ? pt.c_str() : "not supported");
}

struct threadpool_impl : test_threadpool
{
	threadpool<threadpool_default_traits> t;
	~threadpool_impl() { if (t.joinable()) t.join(); }
	const char* name() const override { return "threadpool"; }
	void dispatch(const std::function<void()>& _Task) override { return t.dispatch(_Task); }
	bool parallel_for(size_t _Begin, size_t _End, const std::function<void(size_t _Index)>& _Task) override
	{
		ouro::detail::parallel_for<16>(t, _Begin, _End, _Task);
		return true;
	}

	void flush() override { t.flush(); }
	void release() override { if (t.joinable()) t.join(); }
};

namespace {
	// Implement this inside a TESTMyThreadpool() function.
	template<typename test_threadpool_impl_t> void TESTthreadpool_performance_impl1(test_services& services)
	{
		test_threadpool_impl_t tp;
		test_services::finally Release([&] { tp.release(); });
		TESTthreadpool_performance(services, tp);
	}
}

void TESTthreadpool_perf(test_services& services)
{
	#ifdef _DEBUG
		services.skip("This is slow in debug, and pointless as a benchmark.");
	#else
		TESTthreadpool_performance_impl1<threadpool_impl>(services);
	#endif
}

}}
