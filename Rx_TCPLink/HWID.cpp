#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <Windows.h>
#include <Iphlpapi.h>
#include <string>
#include <iomanip>
#include <sstream>
#include "UDKArray.h"

static std::string s_hwid;

struct HWID {
	union {
		uint64_t hwid;
		struct {
			uint32_t left;
			uint32_t right;
		};
	};
} hwid;

#pragma pack(push, 4)
struct ByteArrayWrapper {
	UDKArray<uint8_t> data;
};
#pragma pack(pop)

std::string GetSystemDriveGUID() {
	char* system_drive;
	char system_drive_mount_point[64];
	char system_drive_volume_name[64];
	char* system_drive_guid_start;
	char* system_drive_guid_end;

	system_drive = getenv("SystemDrive");
	if (system_drive != NULL) {
		if (GetVolumePathNameA(system_drive, system_drive_mount_point, sizeof(system_drive_mount_point))) {
			if (GetVolumeNameForVolumeMountPointA(system_drive_mount_point, system_drive_volume_name, sizeof(system_drive_volume_name))) {
				// Find start of guid
				system_drive_guid_start = system_drive_volume_name;
				while (*system_drive_guid_start != '\0') {
					if (*system_drive_guid_start == '{') {
						// Found start of guid; interate over '{' and break
						++system_drive_guid_start;
						break;
					}

					++system_drive_guid_start;
				}

				// Find end of guid
				system_drive_guid_end = system_drive_guid_start;
				while (*system_drive_guid_end != '\0') {
					if (*system_drive_guid_end == '}') {
						// Found end of guid; break
						break;
					}

					++system_drive_guid_end;
				}

				// Copy guid to out_guid
				return std::string{ system_drive_guid_start, system_drive_guid_end };
			}
		}
	}

	return {};
}

void fill_mac_hwid() {
	IP_ADAPTER_ADDRESSES addresses[32];
	PIP_ADAPTER_ADDRESSES node;
	ULONG addresses_size = sizeof(addresses);

	if (GetAdaptersAddresses(AF_UNSPEC, 0, NULL, addresses, &addresses_size) == ERROR_SUCCESS) {
		for (node = addresses; node != NULL; node = node->Next) {
			if (node->PhysicalAddressLength > 0) {
				static_assert(MAX_ADAPTER_ADDRESS_LENGTH <= sizeof(struct HWID), "HWID struct is not large enough to fit adapter physical address");
				memcpy(&hwid, node->PhysicalAddress, node->PhysicalAddressLength);

				if (hwid.hwid != 0) {
					return;
				}
			}
		}
	}

	hwid.hwid = 0;
}

/** Integer to string (hexadecimal) conversion tables */

const char hexadecimal_rep_table_upper[][3] =
{
	"00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "0A", "0B", "0C", "0D", "0E", "0F",
	"10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "1A", "1B", "1C", "1D", "1E", "1F",
	"20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "2A", "2B", "2C", "2D", "2E", "2F",
	"30", "31", "32", "33", "34", "35", "36", "37", "38", "39", "3A", "3B", "3C", "3D", "3E", "3F",
	"40", "41", "42", "43", "44", "45", "46", "47", "48", "49", "4A", "4B", "4C", "4D", "4E", "4F",
	"50", "51", "52", "53", "54", "55", "56", "57", "58", "59", "5A", "5B", "5C", "5D", "5E", "5F",
	"60", "61", "62", "63", "64", "65", "66", "67", "68", "69", "6A", "6B", "6C", "6D", "6E", "6F",
	"70", "71", "72", "73", "74", "75", "76", "77", "78", "79", "7A", "7B", "7C", "7D", "7E", "7F",
	"80", "81", "82", "83", "84", "85", "86", "87", "88", "89", "8A", "8B", "8C", "8D", "8E", "8F",
	"90", "91", "92", "93", "94", "95", "96", "97", "98", "99", "9A", "9B", "9C", "9D", "9E", "9F",
	"A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8", "A9", "AA", "AB", "AC", "AD", "AE", "AF",
	"B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7", "B8", "B9", "BA", "BB", "BC", "BD", "BE", "BF",
	"C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8", "C9", "CA", "CB", "CC", "CD", "CE", "CF",
	"D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9", "DA", "DB", "DC", "DD", "DE", "DF",
	"E0", "E1", "E2", "E3", "E4", "E5", "E6", "E7", "E8", "E9", "EA", "EB", "EC", "ED", "EE", "EF",
	"F0", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "FA", "FB", "FC", "FD", "FE", "FF"
};

template<typename T>
std::string to_hex(T in_integer) {
	std::string result;
	uint8_t* begin = reinterpret_cast<uint8_t*>(&in_integer);
	uint8_t* itr = begin + sizeof(T);

	result.reserve(sizeof(in_integer) * 2);
	while (itr != begin) {
		--itr;
		result += hexadecimal_rep_table_upper[*itr];
	}

	return result;
}

extern "C" __declspec(dllexport) void c_token(ByteArrayWrapper* out_hwid) {
	std::string hwid_str;

	// Try mac address
	fill_mac_hwid();
	if (hwid.hwid != 0) {
		hwid_str = 'm';
		hwid_str += to_hex(hwid.left) + to_hex(hwid.right); // keep this format so it matches old
	}
	else {
		// Fallback to System Drive GUID
		hwid_str = 'd';
		hwid_str += GetSystemDriveGUID();
	}

	// Return hwid_str through out_hwid
	out_hwid->data.reserve(hwid_str.size());
	out_hwid->data.ArrayNum = hwid_str.size();
	memcpy(out_hwid->data.Data, hwid_str.data(), hwid_str.size());
}
