#include <iostream>
#include <vector>
#include <algorithm>
#include <Windows.h>
#include <psapi.h>
#include "discord_rpc.h"
#include "discord_register.h"
#include "Rx_TCPLink.h"
#include "DiscordRpc.h"
#include "Viewport.h"

extern "C" ReallocFunctionPtrType g_realloc_function = nullptr;

void* searchMemory(const std::vector<byte>& bytes)
{
	MODULEINFO mi;

	GetModuleInformation(GetCurrentProcess(), GetModuleHandle(nullptr), &mi, sizeof mi);

	byte* moduleHead = static_cast<byte*>(mi.lpBaseOfDll);
	byte* moduleTail = moduleHead + mi.SizeOfImage;

	byte* region = moduleHead;

	while (region < moduleTail)
	{
		MEMORY_BASIC_INFORMATION mbi;

		if (!VirtualQuery(region, &mbi, sizeof mbi))
		{
			break;
		}

		byte* regionTail = region + mbi.RegionSize;

		constexpr DWORD protectionFlags = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

		if (mbi.State & MEM_COMMIT && !(mbi.Protect & PAGE_GUARD) && mbi.AllocationProtect & protectionFlags)
		{
			void* result = std::search(region, regionTail, bytes.begin(), bytes.end());

			if (result != regionTail)
			{
				return result;
			}
		}

		region = regionTail;
	}

	return nullptr;
}

#pragma pack(push, 4)
struct FDLLBindInitData
{
	INT Version;
	ReallocFunctionPtrType ReallocFunctionPtr;
};
#pragma pack(pop)

void initViewport()
{
#if _WIN64
	const auto address = searchMemory({
		0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10, 0x48, 0x89,
		0x74, 0x24, 0x18, 0x57, 0x48, 0x83, 0xEC, 0x30, 0x33, 0xC0, 0x49, 0x8B,
		0xE8, 0x48, 0x8B, 0xFA, 0x48, 0x8B, 0xF1, 0x89, 0x42, 0x08, 0x39, 0x42,
		0x0C, 0x74, 0x19, 0x48, 0x8B, 0x0A, 0x89, 0x42, 0x0C, 0x48, 0x85, 0xC9,
		0x74, 0x0E, 0x44, 0x8D, 0x40, 0x08, 0x33, 0xD2, 0xE8, 0xA3, 0x5D, 0xC0 });
#else
	const auto address = searchMemory({
		0x53, 0x56, 0x8B, 0x74, 0x24, 0x0C, 0x83, 0x7E, 0x08, 0x00, 0x57, 0x8B,
		0xD9, 0xC7, 0x46, 0x04, 0x00, 0x00, 0x00, 0x00, 0x74, 0x1C, 0x8B, 0x06,
		0xC7, 0x46, 0x08, 0x00, 0x00, 0x00, 0x00, 0x85, 0xC0, 0x74, 0x0F, 0x6A,
		0x08, 0x6A, 0x00, 0x50, 0xE8, 0x53, 0x75, 0xC6, 0xFF, 0x83, 0xC4, 0x0C,
		0x89, 0x06, 0x8B, 0x03, 0x8B, 0x50, 0x0C, 0x8B, 0xCB, 0xFF, 0xD2, 0x8B });
#endif

	Viewport::setReadPixelsFunction(reinterpret_cast<Viewport::ReadPixelsFunctionType>(address));
}

extern "C" __declspec(dllexport) void DLLBindInit(FDLLBindInitData* in_init_data) {
	g_realloc_function = in_init_data->ReallocFunctionPtr;

	init_wsa();
	Discord_Initialize("482746941256499220", nullptr, 1, nullptr);
	UpdateDiscordRPC(L"", L"", 0, 0, 0, 0, 0, 0, L"");
	initViewport();
}
