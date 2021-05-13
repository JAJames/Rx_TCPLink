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

#include <cinttypes>
#include "UDKArray.h"

#pragma pack(push, 4)
struct UDKColor
{
	union {
		struct {
			uint8_t b, g, r, a;
		};

		uint8_t bytes[4];
		uint32_t uint;
		int32_t sint;
	};
};

static_assert(sizeof(UDKColor) == 4, "UDKColor has unexpected extra data; must be exactly 4");

struct ReadPixelsMetadata
{
	int cubeFace;
	int compresssionMode;
	int convertLinearToGamma;
	float depth;
};

class Viewport
{
public:
	int readPixels(UDKArray<uint8_t>& result, ReadPixelsMetadata metadata)
	{
		int value = readPixels(reinterpret_cast<UDKArray<UDKColor>&>(result), metadata);

		// expand to bytes
		result.ArrayNum *= sizeof(UDKColor);
		result.ArrayMax *= sizeof(UDKColor);
		return value;
	}

	int readPixels(UDKArray<UDKColor>& result, ReadPixelsMetadata metadata)
	{
		return readPixelsFunction(this, result, metadata);
	}

	int getSizeX() const
	{
		return callVirtualFunction<2, int>();
	}

	int getSizeY() const
	{
		return callVirtualFunction<3, int>();
	}

	using ReadPixelsFunctionType = int(__thiscall*)(void*, UDKArray<UDKColor>&, ReadPixelsMetadata);

	static void setReadPixelsFunction(const ReadPixelsFunctionType value)
	{
		readPixelsFunction = value;
	}

	static ReadPixelsFunctionType readPixelsFunction;

	struct DllBindReference
	{
		Viewport* value;
	};

private:
	template<size_t Index, typename ReturnType, typename... ArgumentList> ReturnType callVirtualFunction(ArgumentList... arguments)
	{
		using FunctionType = ReturnType(__thiscall*)(void*, ArgumentList...);
		const auto p = (*reinterpret_cast<FunctionType**>(this))[Index];
		return p(this, arguments...);
	}

	template<size_t Index, typename ReturnType, typename... ArgumentList> ReturnType callVirtualFunction(ArgumentList... arguments) const
	{
		using FunctionType = ReturnType(__thiscall*)(const void*, ArgumentList...);
		const auto p = (*reinterpret_cast<FunctionType*const*>(this))[Index];
		return p(this, arguments...);
	}
};
#pragma pack(pop)
