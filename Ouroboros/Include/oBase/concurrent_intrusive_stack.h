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
// A concurrent stack that assumes a struct with a next pointer. No 
// memory management is done by the class itself.
#ifndef oBase_concurrent_intrusive_stack_h
#define oBase_concurrent_intrusive_stack_h

#include <oBase/compiler_config.h>
#include <atomic>
#include <stdexcept>

#include <oBase/tagged_pointer.h>

namespace ouro {

template<typename T>
class concurrent_intrusive_stack
{
public:
	#if o64BIT == 1
		static const size_t required_next_alignment = sizeof(void*);
	#else
		static const size_t required_next_alignment = 16; // leaves room for tag
	#endif

	typedef unsigned int size_type;
	typedef T value_type;
	typedef value_type* pointer;
	typedef const value_type* const_pointer;
	typedef value_type& reference;
	typedef const value_type& const_reference;

	
	// non-concurrent api

	// initialized to a valid empty stack
	concurrent_intrusive_stack();
	concurrent_intrusive_stack(concurrent_intrusive_stack&& _That);
	~concurrent_intrusive_stack();
	concurrent_intrusive_stack& operator=(concurrent_intrusive_stack&& _That);

	// returns the top of the stack without removing the item from the stack
	pointer peek() const;

	// walks through the stack and counts the entries. Only use this for 
	// debugging/confirmation during times of no push/pops
	size_type size();


	// concurrent api

	// push an element onto the stack
	void push(pointer element);

	// if there are no elements this return nullptr
	pointer pop();

	// returns true if no elements are in the stack
	bool empty() const;

	// returns the head that is thus a linked list of all items that were in the 
	// stack, leaving the stack empty.
	pointer pop_all();

private:

	tagged_pointer<T> head;
	char cache_line_padding[oCACHE_LINE_SIZE - sizeof(std::atomic_uintptr_t)];

	concurrent_intrusive_stack(const concurrent_intrusive_stack&); /* = delete; */
	const concurrent_intrusive_stack& operator=(const concurrent_intrusive_stack&); /* = delete; */
};

template<typename T>
concurrent_intrusive_stack<T>::concurrent_intrusive_stack()
{
	for (char& c : cache_line_padding)
		c = 0;
}

template<typename T>
concurrent_intrusive_stack<T>::concurrent_intrusive_stack(concurrent_intrusive_stack&& _That)
{
	head = std::move(_That.head);
	for (char& c : cache_line_padding)
		c = 0;
}

template<typename T>
concurrent_intrusive_stack<T>::~concurrent_intrusive_stack()
{
	if (!empty())
		throw std::length_error("concurrent_intrusive_stack not empty");
}

template<typename T>
typename concurrent_intrusive_stack<T>& concurrent_intrusive_stack<T>::operator=(concurrent_intrusive_stack&& _That)
{
	if (this != &_That)
		head = std::move(_That.head);
	return *this;
}

template<typename T>
void concurrent_intrusive_stack<T>::push(pointer element)
{
	tagged_pointer<T> n, o(head); 
	do 
	{	o = head;
		element->next = reinterpret_cast<pointer>(o.ptr());
		n = tagged_pointer<T>(element, o.tag() + 1);
	} while (!tagged_pointer<T>::CAS(&head, n, o));
}

template<typename T>
typename concurrent_intrusive_stack<T>::pointer concurrent_intrusive_stack<T>::peek() const
{
	tagged_pointer<T> o(head);
	return reinterpret_cast<pointer>(o.ptr());
}

template<typename T>
typename concurrent_intrusive_stack<T>::pointer concurrent_intrusive_stack<T>::pop()
{
	tagged_pointer<T> n, o(head); 
	pointer element = nullptr;
	do 
	{	o = head;
		if (!o.ptr())
			return nullptr;
		element = reinterpret_cast<pointer>(o.ptr());
		n = tagged_pointer<T>(element->next, o.tag() + 1);
	} while (!tagged_pointer<T>::CAS(&head, n, o));
	return element;
}

template<typename T>
bool concurrent_intrusive_stack<T>::empty() const
{
	tagged_pointer<T> o(head);
	return !o.ptr();
}

template<typename T>
typename concurrent_intrusive_stack<T>::pointer concurrent_intrusive_stack<T>::pop_all()
{
	pointer element = nullptr;
	tagged_pointer<T> n, o(head);
	do 
	{	o = head;
		element = reinterpret_cast<pointer>(o.ptr());
		n = tagged_pointer<T>(nullptr, o.tag() + 1);
	} while (!tagged_pointer<T>::CAS(&head, n, o));
	return element;
}

template<typename T>
typename concurrent_intrusive_stack<T>::size_type concurrent_intrusive_stack<T>::size()
{
	pointer p = peek();
	size_type n = 0;
	while (p)
	{
		n++;
		p = p->next;
	}
	return n;
}

} // namespace ouro

#endif