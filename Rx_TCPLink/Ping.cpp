#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define PIO_APC_ROUTINE_DEFINED
#include <WinSock2.h>
#include <IPExport.h>
#include <winternl.h>
#include <IcmpAPI.h>
#include <WS2tcpip.h>
#include<unordered_map>
#include <string>
#include <atomic>
#include <mutex>
#include <forward_list>

// Forward declarations and aliases
struct PingContext;
using request_map = std::unordered_map<std::wstring, std::shared_ptr<PingContext>>;

// Constants
constexpr int PING_IN_PROGRESS = -1;
constexpr int PING_FAILED = -2;

unsigned char icmp_request_data[]{ "Renegade X Game Client Ping Data" };
constexpr size_t icmp_request_data_length = sizeof(icmp_request_data) - 1;
constexpr DWORD icmp_reply_buffer_size = sizeof(ICMP_ECHO_REPLY) + icmp_request_data_length + 8 + sizeof(IO_STATUS_BLOCK);
constexpr DWORD icmp_timeout = 1000; // 1 second

// Statics
std::atomic<unsigned int> s_pings_in_progress{ 0 }; // Note, this is ALL ping requests in progress, not necessarily the ones in `s_pings` (and therefore, not necessarily contexts that still exist)
std::atomic<bool> s_ping_thread_active{ false };
std::shared_ptr<request_map> s_pings;
std::forward_list<std::weak_ptr<PingContext>> s_pending_requests;
std::thread s_ping_thread;
//std::mutex s_pings_mutex;
std::mutex s_pending_requests_mutex;

struct PingContext {
	HANDLE icmp_handle{};
	IPAddr destination{};
	std::weak_ptr<request_map> request_group;
	std::unique_ptr<unsigned char[]> reply_buffer;
	std::atomic<int> ping_time_milliseconds{ PING_IN_PROGRESS };

	// Performs any single-run cleanup needed
	void finish() {
		// Cleanup handle
		IcmpCloseHandle(icmp_handle);
		icmp_handle = nullptr;

		// Mark no longer in progress
		--s_pings_in_progress;
	}

	PingContext() {
		++s_pings_in_progress;
	}

	~PingContext() {
		if (icmp_handle != nullptr) {
			IcmpCloseHandle(icmp_handle);
			--s_pings_in_progress;
		}
	}
};

IPAddr parse_address(const wchar_t* in_str) {
	IN_ADDR buffer_addr{};
	InetPtonW(AF_INET, in_str, &buffer_addr);

	return buffer_addr.S_un.S_addr;
}

void WINAPI icmp_ping_callback(void* in_context, PIO_STATUS_BLOCK in_status_block, ULONG) {
	std::unique_ptr<std::weak_ptr<PingContext>> weak_context{ static_cast<std::weak_ptr<PingContext>*>(in_context) };
	auto strong_context = weak_context->lock();

	if (strong_context == nullptr) {
		// Context has been destroyed already; s_pings must have changed
		return;
	}

	// Cleanup handle
	strong_context->finish();

	if (NT_ERROR(in_status_block->Status)) {
		// Ping failed somehow; set status to error
		strong_context->ping_time_milliseconds = PING_FAILED;
		return;
	}

	// Verify ping reply size
	if (in_status_block->Information < icmp_request_data_length) {
		// Ping did not reply with same data as sent; set status to error
		strong_context->ping_time_milliseconds = PING_FAILED;
		return;
	}

	DWORD reply_count = IcmpParseReplies(strong_context->reply_buffer.get(), icmp_reply_buffer_size);
	if (reply_count <= 0) {
		// No response was received; this should never happen since one of the above checks would certainly have been hit
		strong_context->ping_time_milliseconds = PING_FAILED;
		return;
	}

	ICMP_ECHO_REPLY* replies = reinterpret_cast<ICMP_ECHO_REPLY*>(strong_context->reply_buffer.get());
	for (DWORD index = 0; index < reply_count; ++index) { // reply_count should never be anything other than 1, but hey, whatever
		ICMP_ECHO_REPLY& reply = replies[index];

		// Verify success
		if (reply.Status != IP_SUCCESS) {
			strong_context->ping_time_milliseconds = PING_FAILED;
			return;
		}

		// Verify reply size; this is probably redundant, but hey whatever
		if (reply.DataSize != icmp_request_data_length) {
			strong_context->ping_time_milliseconds = PING_FAILED;
			return;
		}

		// Verify ping data patches
		if (std::memcmp(icmp_request_data, reply.Data, icmp_request_data_length) != 0) {
			strong_context->ping_time_milliseconds = PING_FAILED;
			return;
		}

		// Ping request successful; set it
		strong_context->ping_time_milliseconds = reply.RoundTripTime;
	}
}

void process_pending_requests() {
	std::unique_lock<std::mutex> lock{ s_pending_requests_mutex };
	// Loop until there are no pings in progress
	while (s_pings_in_progress != 0) {
		// Process pending any requests in the queue
		while (!s_pending_requests.empty()) {
			// Pop front pending request
			auto pending_request = s_pending_requests.front().lock();
			s_pending_requests.pop_front();

			if (pending_request != nullptr) {
				// Kick off ICMP request
				IcmpSendEcho2(
					pending_request->icmp_handle,
					nullptr, // event
					&icmp_ping_callback,
					new std::weak_ptr<PingContext>(pending_request),
					pending_request->destination,
					icmp_request_data,
					icmp_request_data_length,
					nullptr, // options
					pending_request->reply_buffer.get(),
					icmp_reply_buffer_size,
					icmp_timeout);
			}
		}

		// Process any thread alerts / ICMP callbacks
		lock.unlock();
		SleepEx(10, TRUE);
		lock.lock();
	}

	s_ping_thread_active = false;
}

void queue_pending_request(std::weak_ptr<PingContext> in_pending_request) {
	std::lock_guard<std::mutex> lock{ s_pending_requests_mutex };
	s_pending_requests.push_front(std::move(in_pending_request));

	// Start thread if necessary
	if (!s_ping_thread_active) {
		// Join any existing thread
		if (s_ping_thread.joinable()) {
			s_ping_thread.join();
		}

		// Mark processing active
		s_ping_thread_active = true;

		// Spin up thread
		s_ping_thread = std::thread(&process_pending_requests);
	}
}

extern "C" __declspec(dllexport) bool start_ping_request(const wchar_t* in_str) {
	// Parse address to ping
	IPAddr destination_address = parse_address(in_str);
	if (destination_address == 0) {
		// Failed to parse
		return false;
	}

	// Get ICMP handle to send request
	HANDLE icmp_handle = IcmpCreateFile();
	if (icmp_handle == INVALID_HANDLE_VALUE) {
		// Failed to open handle for ICMP requests
		return false;
	}

	// Build request context
	auto context = std::make_shared<PingContext>();
	context->icmp_handle = icmp_handle;
	context->destination = destination_address;
	context->reply_buffer = std::make_unique<unsigned char[]>(icmp_reply_buffer_size);
	{
		//std::lock_guard<std::mutex> guard{ s_pings_mutex };
		
		if (s_pings == nullptr) {
			s_pings = std::make_shared<request_map>();
		}

		context->request_group = s_pings;
		s_pings->emplace(in_str, context);
	}

	// Queue request context
	queue_pending_request(context);

	return true;
}

extern "C" __declspec(dllexport) int get_ping(const wchar_t* in_str) {
	//std::lock_guard<std::mutex> guard{ s_pings_mutex };

	// Safety check that s_pings actually exists
	if (s_pings == nullptr) {
		// Ping request does not exist
		return PING_FAILED;
	}

	// Search for IP in ping list
	auto itr = s_pings->find(in_str);
	if (itr == s_pings->end()) {
		// Ping request does not exist
		return PING_FAILED;
	}

	// This is a massive hack, but we need to be able to join the processing thread when we're done to prevent a shutdown crash
	if (!s_ping_thread_active && s_ping_thread.joinable()) {
		s_ping_thread.join();
	}

	return itr->second->ping_time_milliseconds;
}

extern "C" __declspec(dllexport) void clear_pings() {
	//std::lock_guard<std::mutex> guard{ s_pings_mutex };
	s_pings = nullptr;

	if (!s_ping_thread_active && s_ping_thread.joinable()) {
		s_ping_thread.join();
	}
}
