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

#include <atlimage.h>
#include <string>
#include <ctime>
#include <fstream>
#include "Viewport.h"

Viewport::ReadPixelsFunctionType Viewport::readPixelsFunction = nullptr;

/** Filename related constants */
static constexpr wchar_t prefix[]{ L"ScreenCapture " };
static constexpr size_t prefix_length = sizeof(prefix) / sizeof(wchar_t) - 1;
static constexpr wchar_t extension[]{ L".jpg" };
static constexpr size_t extension_length = sizeof(extension) / sizeof(wchar_t) - 1;
static constexpr size_t max_date_length = FILENAME_MAX - prefix_length - extension_length;

#pragma pack(push, 4)
struct ScreenCapture
{
	UDKArray<uint8_t> data;

	// For pushing data to captures
	template<typename T>
	void push(const T& in_data) {
		// Expand capture as necessary
		int required_size = data.ArrayNum + sizeof(T);
		if (required_size >= data.ArrayMax) {
			// Expand to minimum required size, or double array size, whichever is larger
			data.Reallocate(max(required_size, data.ArrayMax * 2));
		}

		// Copy data to capture
		std::memcpy(data.Data + data.ArrayNum, &in_data, sizeof(T));
		data.ArrayNum += sizeof(T);
	}

	// For popping from captures
	template<typename T>
	T pop() {
		T result{};
		if (sizeof(T) > data.ArrayNum) {
			return result;
		}

		// Copy item to result
		uint8_t* item_begin = data.Data + data.ArrayNum - sizeof(T);
		std::memcpy(&result, item_begin, sizeof(T));

		// Pop item
		data.ArrayNum -= sizeof(T);

		// Return result
		return result;
	}
};
#pragma pack(pop)

extern "C" __declspec(dllexport) void take_ss(Viewport::DllBindReference* viewport, ScreenCapture* result)
{
	// Read in screenshot data from UDK
	viewport->value->readPixels(result->data, { 6, 0, 1, 16000.0F });
	int sizeX = viewport->value->getSizeX();
	int sizeY = viewport->value->getSizeY();

	// Import screenCapture data into CImage
	CImage outputImage;
	outputImage.Create(sizeX, sizeY, 32);
	for (int row = 0; row < sizeY; ++row)
	{
		const void* screenCaptureRowAddress = &result->data[row * sizeX * sizeof(UDKColor)];
		void* outputRowAddress = outputImage.GetPixelAddress(0, row);
		memcpy(outputRowAddress, screenCaptureRowAddress, sizeX * 4);
	}

	// Save JPEG image data to memory stream
	auto stream = SHCreateMemStream(NULL, 0);
	outputImage.Save(stream, Gdiplus::ImageFormatJPEG);
	stream->Seek({}, STREAM_SEEK_SET, 0);

	// Get stream size
	STATSTG stream_stat{};
	stream->Stat(&stream_stat, STATFLAG::STATFLAG_NONAME);
	auto stream_size = stream_stat.cbSize.QuadPart;

	// Read data from stream into buffer
	result->data.ArrayNum = stream_size;
	stream->Read(result->data.Data, result->data.ArrayMax, nullptr);
	stream->Release();
}

static_assert(sizeof(UDKColor) == 4, "UDKColor size != 4");

extern "C" __declspec(dllexport) void write_ss(ScreenCapture* screenCapture)
{
	// Get current date
	auto currentTime = std::time(nullptr);
	auto currentTm = localtime(&currentTime);
	wchar_t timeString[max_date_length + 1];
	std::wcsftime(timeString, max_date_length, L"%F - %H-%M-%S", currentTm);

	// Build filename
	std::wstring filename{ prefix };
	filename += timeString;
	filename += extension;

	// Write to file
	std::ofstream file{ filename, std::ios::binary };
	file.write(reinterpret_cast<char*>(screenCapture->data.Data), screenCapture->data.ArrayNum);
}

constexpr size_t byte_buffer_size = 256;

/**
 * Copies data from a dynamic UDK byte array to a static UDK byte array
 *
 * @param out_destination Byte buffer to copy data to
 * @param in_source Byte array to copy data from
 * @param in_offset Position of in_source to start copying from
 * @return Total number of bytes copied (will never exceed byte_buffer_size)
 */
extern "C" __declspec(dllexport) int copy_array_to_buffer(uint8_t* out_destination, ScreenCapture* in_source, int in_offset) {
	// Sanity check length and offset
	if (in_offset < 0
		|| in_source == nullptr
		|| in_source->data.ArrayNum <= in_offset
		|| out_destination == nullptr) {
		return 0;
	}

	size_t length = min(byte_buffer_size, in_source->data.ArrayNum - in_offset); // Remaining bytes in array or buffer size, whichever is smaller
	std::memcpy(out_destination, in_source->data.Data + in_offset, length);
	return length;
}

/** static shit */

uint8_t* s_cap_buffer = nullptr;
ScreenCapture* s_capture = nullptr;

extern "C" __declspec(dllexport) void set_cap(uint8_t* buffer, ScreenCapture* screenCapture) {
	s_cap_buffer = buffer;
	s_capture = screenCapture;
}

extern "C" __declspec(dllexport) int read_cap(int in_offset) {
	int result = copy_array_to_buffer(s_cap_buffer, s_capture, in_offset);
	return result;
}
