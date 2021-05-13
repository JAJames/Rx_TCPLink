/**
 * Copyright (C) 2019 Jessica James.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Written by Jessica James <jessica.aj@outlook.com>
 */

#pragma once

#include <algorithm>
#include "Rx_TCPLink.h"

#pragma pack(push, 4)
template<typename DataT>
struct UDKArray
{
	void Reallocate(int NewNum)
	{
		ArrayMax = NewNum;
		Data = static_cast<DataT*>((*g_realloc_function)(Data, ArrayMax * sizeof(DataT), 8));
	}

	void reserve(int in_size) {
		if (in_size > ArrayMax) {
			Reallocate(in_size);
		}
	}

	DataT& operator[](const int index)
	{
		return Data[index];
	}

	const DataT& operator[](const int index) const
	{
		return Data[index];
	}

	void push_back(const DataT& in_item) {
		if (ArrayNum >= ArrayMax) {
			Reallocate(max(ArrayNum * 2, 8));
		}

		Data[ArrayNum] = in_item;
		++ArrayNum;
	}

	void pop_back() {
		--ArrayNum;
	}

	DataT& back() const {
		return Data[ArrayNum - 1];
	}

	DataT& front() const {
		return Data[0];
	}

	DataT* Data;
	int ArrayNum;
	int ArrayMax;
};
#pragma pack(pop)
