#define _CRT_SECURE_NO_WARNINGS

#include "DiscordRpc.h"
#include <cstdlib>
#include <cstring>
#include <chrono>
#include "discord_rpc.h"

static constexpr int GDI_TEAM_NUM = 0;
static constexpr int NOD_TEAM_NUM = 1;

int64_t now_seconds() {
	return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

extern "C" __declspec(dllexport) void UpdateDiscordRPC(wchar_t* in_server_name, wchar_t* in_level_name, int in_player_count, int in_max_players, int in_team_num, int in_time_elapsed, int in_time_remaining, int in_is_firestorm, wchar_t* in_image_name) {
	// Convert our input strings
	char server_name[128];
	size_t server_name_length{};
	char level_name[128];
	size_t level_name_length{};
	char image_name[128];
	size_t image_name_length{};

	server_name_length = std::wcstombs(server_name, in_server_name, sizeof(server_name));
	level_name_length = std::wcstombs(level_name, in_level_name, sizeof(level_name));
	image_name_length = std::wcstombs(image_name, in_image_name, sizeof(image_name));

	// Populate our presence
	DiscordRichPresence presence{};

	// state
	if (*level_name != '\0') {
		// We're in a match
		presence.state = level_name;
	}
	else {
		// We're not in a level; we must be at the main menu
		presence.state = "Main Menu";
	}

	// details, party info
	if (*server_name != '\0') {
		// We're in a multiplayer server
		presence.details = server_name;

		presence.partySize = in_player_count;
		presence.partyMax = in_max_players;
	}
	else {
		// We're not in any server
		if (strcmp(level_name, "Black Dawn") == 0) {
			// We're playing campaign
			presence.details = "Campaign";
		}
		else if (*level_name != '\0') {
			// We're playing skirmish
			presence.details = "Singleplayer Skirmish";
		}
		// else // we're sitting at the main menu
	}

	if (in_is_firestorm == 0) {
		presence.largeImageKey = "renegadex";
		presence.largeImageText = "Renegade X";

		if (in_team_num == GDI_TEAM_NUM) {
			presence.smallImageKey = "gdi";
			presence.smallImageText = "GDI";
		}
		else if (in_team_num == NOD_TEAM_NUM) {
			presence.smallImageKey = "nod";
			presence.smallImageText = "Nod";
		}
	}
	else {
		presence.largeImageKey = "fs";
		presence.largeImageText = "Renegade X: Firestorm";

		// For map-specfic images
		if (*image_name != '\0') {
			presence.largeImageKey = image_name;
		}

		if (in_team_num == GDI_TEAM_NUM) {
			presence.smallImageKey = "tsgdi";
			presence.smallImageText = "GDI";
		}
		else if (in_team_num == NOD_TEAM_NUM) {
			presence.smallImageKey = "tsnod";
			presence.smallImageText = "Nod";
		}
	}



	// Discord client logic:
	//	If endTimestamp is present: display time remaining

	// If the match is over (negative in_time_remaining); don't display anything
	if (presence.endTimestamp >= 0) {
		presence.startTimestamp = now_seconds() - in_time_elapsed;

		// If it's a timed match, display the time remaining
		if (in_time_remaining != 0) {
			presence.endTimestamp = now_seconds() + in_time_remaining;
		}
	}

	// Update our presence
	Discord_UpdatePresence(&presence);
}

//typedef struct DiscordRichPresence {
//	const char* state;   /* max 128 bytes */
//	const char* details; /* max 128 bytes */
//	int64_t startTimestamp;
//	int64_t endTimestamp;
//	const char* largeImageKey;  /* max 32 bytes */
//	const char* largeImageText; /* max 128 bytes */
//	const char* smallImageKey;  /* max 32 bytes */
//	const char* smallImageText; /* max 128 bytes */
//	const char* partyId;        /* max 128 bytes */
//	int partySize;
//	int partyMax;
//	const char* matchSecret;    /* max 128 bytes */
//	const char* joinSecret;     /* max 128 bytes */
//	const char* spectateSecret; /* max 128 bytes */
//	int8_t instance;
//} DiscordRichPresence;

/*char buffer[256];
        DiscordRichPresence discordPresence;
        memset(&discordPresence, 0, sizeof(discordPresence));
        discordPresence.state = "West of House";
        sprintf(buffer, "Frustration level: %d", FrustrationLevel);
        discordPresence.details = buffer;
        discordPresence.startTimestamp = StartTime;
        discordPresence.endTimestamp = time(0) + 5 * 60;
        discordPresence.largeImageKey = "canary-large";
        discordPresence.smallImageKey = "ptb-small";
        discordPresence.partyId = "party1234";
        discordPresence.partySize = 1;
        discordPresence.partyMax = 6;
        discordPresence.matchSecret = "xyzzy";
        discordPresence.joinSecret = "join";
        discordPresence.spectateSecret = "look";
        discordPresence.instance = 0;
		Discord_UpdatePresence(&discordPresence);*/