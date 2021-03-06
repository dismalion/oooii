// Copyright (c) 2014 Antony Arciuolo. See License.txt regarding use.
#include <oConcurrency/concurrent_hash_map.h>
#include <oMemory/bit.h>

namespace ouro {

concurrent_hash_map::size_type concurrent_hash_map::calc_size(size_type capacity)
{
	const size_type n = __max(8, nextpow2(capacity * 2));
	const size_type key_bytes = n * sizeof(std::atomic<key_type>);
	const size_type value_bytes = n * sizeof(std::atomic<value_type>);
	return key_bytes + value_bytes;
}

concurrent_hash_map::concurrent_hash_map()
	: keys(nullptr)
	, values(nullptr)
	, modulo_mask(0)
{}

concurrent_hash_map::concurrent_hash_map(concurrent_hash_map&& that)
	: keys(that.keys)
	, values(that.values)
	, modulo_mask(that.modulo_mask)
{
	that.deinitialize();
}

concurrent_hash_map::concurrent_hash_map(size_type capacity, const char* alloc_label, const allocator& alloc_)
{
	initialize(capacity, alloc_label, alloc_);
}

concurrent_hash_map::concurrent_hash_map(void* memory, size_type capacity)
{
	initialize(memory, capacity);
}

concurrent_hash_map::~concurrent_hash_map()
{
	deinitialize();
}

concurrent_hash_map& concurrent_hash_map::operator=(concurrent_hash_map&& that)
{
	if (this != &that)
	{
		deinitialize();
		modulo_mask = that.modulo_mask; that.modulo_mask = 0;
		keys = that.keys; that.keys = 0;
		values = that.values; that.values = 0;
	}
	return *this;
}

void concurrent_hash_map::initialize(size_type capacity, const char* alloc_label, const allocator& a)
{
	initialize(a.allocate(calc_size(capacity), 0, alloc_label), capacity);
	alloc = a;
}

void concurrent_hash_map::initialize(void* memory, size_type capacity)
{
	const size_type n = __max(8, nextpow2(capacity * 2));
	const size_type key_bytes = n * sizeof(std::atomic<key_type>);
	const size_type value_bytes = n * sizeof(std::atomic<value_type>);
	const size_type req = key_bytes + value_bytes;
	modulo_mask = n - 1;
	keys = (std::atomic<key_type>*)memory;
	values = (std::atomic<value_type>*)((uint8_t*)memory + key_bytes);
	memset(keys, 0xff/*nullkey*/, key_bytes);
	memset(values, nullidx, value_bytes);
	alloc = noop_allocator;
}

void* concurrent_hash_map::deinitialize()
{
	void* p = keys;
	alloc.deallocate(p);
	p = alloc == noop_allocator ? p : nullptr;
	modulo_mask = 0;
	alloc = noop_allocator;
	keys = nullptr;
	values = nullptr;
	return p;
}

void concurrent_hash_map::clear()
{
	const size_type n = modulo_mask + 1;
	const size_type key_bytes = n * sizeof(std::atomic<key_type>);
	const size_type value_bytes = n * sizeof(std::atomic<value_type>);
	memset(keys, 0xff/*nullkey*/, key_bytes);
	memset(values, nullidx, value_bytes);
}

concurrent_hash_map::size_type concurrent_hash_map::size() const
{
	size_type n = 0;
	for (uint32_t i = 0; i <= modulo_mask; i++)
		if (keys[i].load(std::memory_order_relaxed) != nullkey && 
			values[i].load(std::memory_order_relaxed) != nullidx)
			n++;
	return n;
}

concurrent_hash_map::size_type concurrent_hash_map::reclaim()
{
	size_type n = 0;
	uint32_t i = 0;
	while (i <= modulo_mask)
	{
		if (values[i] == nullidx && keys[i] != nullkey)
		{
			keys[i] = nullkey;
			n++;

			uint32_t ii = (i + 1) & modulo_mask;
			while (keys[ii] != nullkey)
			{
				if (values[ii] == nullidx)
				{
					keys[ii] = nullkey;
					n++;
				}

				else if ((keys[ii] & modulo_mask) != ii) // move if key is misplaced due to collision
				{
					key_type k = keys[ii];
					value_type v = values[ii];
					keys[ii] = nullkey;
					values[ii] = nullidx;
					set(k, v);
				}

				i = __max(i, ii);
				ii = (ii + 1) & modulo_mask;
			}
		}
		else
			i++;
	}

	return n;
}

concurrent_hash_map::size_type concurrent_hash_map::migrate(concurrent_hash_map& that, size_type max_moves)
{
	size_type n = 0;
	uint32_t i = 0;
	while (i <= modulo_mask)
	{
		if (values[i] != nullidx && keys[i] != nullkey)
		{
			that.set(keys[i], values[i]);
			remove(keys[i]);
			if (++n >= max_moves)
				break;
		}
		
		i++;
	}

	return n;
}

concurrent_hash_map::value_type concurrent_hash_map::set(const key_type& key, const value_type& value)
{
	if (key == nullkey)
		throw std::invalid_argument("key must be non-zero");
	for (key_type k = key, j = 0;; k++, j++)
	{
		if (j > modulo_mask)
			throw std::length_error("concurrent_hash_map full");

		k &= modulo_mask;
		std::atomic<key_type>& stored = keys[k];
		key_type probed = stored.load(std::memory_order_relaxed);
		if (probed != key)
		{
			if (probed != nullkey)
				continue;
			key_type prev = nullkey;
			stored.compare_exchange_strong(prev, key, std::memory_order_relaxed);
			if (prev != nullkey && prev != key)
				continue;
		}
		return values[k].exchange(value, std::memory_order_relaxed);
	}
}
	
concurrent_hash_map::value_type concurrent_hash_map::get(const key_type& key) const
{
	if (key == nullkey)
		throw std::invalid_argument("key must be non-zero");
	for (key_type k = key;; k++)
	{
		k &= modulo_mask;
		std::atomic<key_type>& stored = keys[k];
		key_type probed = stored.load(std::memory_order_relaxed);
		if (probed == key)
			return values[k].load(std::memory_order_relaxed);
		if (probed == nullkey)
			return nullidx;
	}
}

}
