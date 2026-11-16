#pragma once

// ============================================================
//  HGI SLOT MOD - SlotTokenInterceptor.h (FIXED v3)
//  Fitur: Deteksi token JP, intercept packet, auto-login
//  FIXES:
//   - Bounded collection (max 500 tokens, deque instead of vector)
//   - Buffer overflow protection di ExtractTokenFromJson
//   - Token persistence (save/load to disk)
//   - Duplicate check improvement
// ============================================================

#include "config/Setup.h"
#include "feature/GameClass.h"
#include "feature/SafeGameClass.h"
#include <deque>
#include <vector>
#include <mutex>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <android/log.h>

#define LOGI_TOKEN(...) __android_log_print(ANDROID_LOG_INFO, "HGI_Token", __VA_ARGS__)
#define LOGE_TOKEN(...) __android_log_print(ANDROID_LOG_ERROR, "HGI_Token", __VA_ARGS__)

// ============================================================
//  Intercepted Token Data
// ============================================================
struct InterceptedToken {
	std::string token;
	std::string source;    // "socket_packet" / "hall_startup" / "login_cipher"
	int userId;
	long money;
	std::string nickname;
	bool isJackpot;
	uint64_t timestamp;

	InterceptedToken() : userId(0), money(0), isJackpot(false), timestamp(0) {}
};

// ============================================================
//  SLOT TOKEN INTERCEPTOR (FIXED v3)
// ============================================================
class SlotTokenInterceptor {
public:
	static constexpr size_t MAX_TOKENS = 500;  // Limit agar tidak OOM

	static SlotTokenInterceptor& Instance() {
		static SlotTokenInterceptor inst;
		return inst;
	}

	// Scan packet bytes dari SocketClient.SendRawBytes
	void ScanPacket(const uint8_t* data, size_t len) {
		std::lock_guard<std::mutex> lock(m_mutex);
		if (!data || len < 16 || len > 1048576) return; // Max 1MB packet

		for (size_t i = 0; i + 3 < len; i++) {
			// Pattern LoginCrypto cipher magic "AFC5"
			if (data[i]=='A' && data[i+1]=='F' && data[i+2]=='C' && data[i+3]=='5') {
				size_t end = i;
				for (size_t j = i; j < len && j < i + 65536; j++) { // Max 64KB token
					if (data[j] == 0) break;
					end = j;
				}
				if (end - i > 8 && end - i < 65536) {
					InterceptedToken t;
					t.token = std::string((const char*)(data + i), end - i + 1);
					t.source = "login_cipher";
					t.timestamp = GetTimeMs();
					AddTokenInternal(t);
				}
			}
			// Pattern token JSON: szUserToken
			if (i + 11 < len && memcmp(data + i, "szUserToken", 11) == 0) {
				ExtractTokenFromJson(data, len, i, "szUserToken");
			}
			// Pattern token JSON: "token"
			if (i + 7 < len && memcmp(data + i, "\"token\"", 7) == 0) {
				ExtractTokenFromJson(data, len, i, "token");
			}
		}
	}

	// Baca token dari HallStartup memory (null-safe)
	void ScanHallStartup(HallStartup* hs) {
		std::lock_guard<std::mutex> lock(m_mutex);

		// FIX: Gunakan SafeHallStartup untuk null check
		if (!SafeHallStartup::IsValid(hs)) return;
		if (!SafeHallStartup::HasToken(hs)) return;

		std::string token = SafeHallStartup::GetToken(hs);
		if (token.empty()) return;

		// Cek duplikat
		for (auto& existing : m_tokens) {
			if (existing.token == token && existing.userId == hs->iUserID) {
				// Update money jika berubah
				existing.money = hs->liMoney;
				existing.isJackpot = (hs->liMoney > 10000000LL);
				return;
			}
		}

		InterceptedToken t;
		t.token = token;
		t.source = "hall_startup";
		t.userId = hs->iUserID;
		t.money = hs->liMoney;
		t.nickname = SafeHallStartup::GetNickname(hs);
		t.isJackpot = (hs->liMoney > 10000000LL);
		t.timestamp = GetTimeMs();

		AddTokenInternal(t);
	}

	// Auto login ke akun target (null-safe)
	void TriggerAutoLogin(const InterceptedToken& target) {
		// FIX: Null-safe access
		HallStartup* myHs = SafeHallStartup::Get();
		if (!myHs) {
			LOGE_TOKEN("TriggerAutoLogin: HallStartup not ready!");
			return;
		}

		// Backup
		m_myBackup.token = SafeHallStartup::GetToken(myHs);
		m_myBackup.userId = myHs->iUserID;
		m_myBackup.money = myHs->liMoney;
		m_myBackup.nickname = SafeHallStartup::GetNickname(myHs);

		// Inject token target
		Il2CppString* newToken = HGI_CreateString(target.token.c_str());
		if (newToken) {
			myHs->szUserToken = newToken;
		} else {
			LOGE_TOKEN("TriggerAutoLogin: Failed to create Il2CppString for token!");
			return;
		}

		myHs->iUserID = target.userId;

		if (!target.nickname.empty()) {
			Il2CppString* nickStr = HGI_CreateString(target.nickname.c_str());
			if (nickStr) myHs->szNickName = nickStr;
		}

		// Reconnect with retry
		ReconnectSocketWithRetry(3, 500);

		LOGI_TOKEN("Auto-login triggered for UserID=%d", target.userId);
	}

	// Restore ke akun sendiri
	void RestoreOwnAccount() {
		if (m_myBackup.token.empty()) {
			LOGE_TOKEN("No backup available!");
			return;
		}

		HallStartup* myHs = SafeHallStartup::Get();
		if (!myHs) return;

		Il2CppString* oldToken = HGI_CreateString(m_myBackup.token.c_str());
		if (oldToken) myHs->szUserToken = oldToken;
		myHs->iUserID = m_myBackup.userId;

		if (!m_myBackup.nickname.empty()) {
			Il2CppString* nickStr = HGI_CreateString(m_myBackup.nickname.c_str());
			if (nickStr) myHs->szNickName = nickStr;
		}

		ReconnectSocketWithRetry(3, 500);
		LOGI_TOKEN("Restored own account UserID=%d", m_myBackup.userId);
	}

	std::vector<InterceptedToken> GetTokens() {
		std::lock_guard<std::mutex> lock(m_mutex);
		return std::vector<InterceptedToken>(m_tokens.begin(), m_tokens.end());
	}

	void ClearTokens() {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_tokens.clear();
	}

	size_t TokenCount() {
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_tokens.size();
	}

	// ============================================================
	//  TOKEN PERSISTENCE - Save/Load ke disk
	// ============================================================
	void SaveToDisk() {
		std::lock_guard<std::mutex> lock(m_mutex);
		std::ofstream file(STORAGE_PATH);
		if (!file.is_open()) {
			LOGE_TOKEN("Cannot write tokens to %s", STORAGE_PATH);
			return;
		}

		// Simple CSV format: token|source|userId|money|nickname|isJackpot|timestamp
		file << "# HGI Token Storage v1\n";
		for (const auto& t : m_tokens) {
			file << t.token << "|"
			     << t.source << "|"
			     << t.userId << "|"
			     << t.money << "|"
			     << t.nickname << "|"
			     << (t.isJackpot ? 1 : 0) << "|"
			     << t.timestamp << "\n";
		}

		file.close();
		LOGI_TOKEN("Saved %zu tokens to disk", m_tokens.size());
	}

	void LoadFromDisk() {
		std::lock_guard<std::mutex> lock(m_mutex);
		std::ifstream file(STORAGE_PATH);
		if (!file.is_open()) {
			LOGI_TOKEN("No saved tokens found at %s", STORAGE_PATH);
			return;
		}

		std::string line;
		size_t loaded = 0;
		while (std::getline(file, line)) {
			if (line.empty() || line[0] == '#') continue;
			if (m_tokens.size() >= MAX_TOKENS) break;

			InterceptedToken t;
			if (ParseTokenLine(line, t)) {
				// Check duplikat
				bool dup = false;
				for (const auto& existing : m_tokens) {
					if (existing.token == t.token && existing.userId == t.userId) {
						dup = true;
						break;
					}
				}
				if (!dup) {
					m_tokens.push_back(t);
					loaded++;
				}
			}
		}

		file.close();
		LOGI_TOKEN("Loaded %zu tokens from disk", loaded);
	}

	bool HasBackup() const { return !m_myBackup.token.empty(); }

private:
	std::mutex m_mutex;
	std::deque<InterceptedToken> m_tokens;  // FIX: deque untuk efficient pop_front
	InterceptedToken m_myBackup;

	static constexpr const char* STORAGE_PATH = "/sdcard/Android/data/com.higgs.domino/.hgi_tokens";

	static uint64_t GetTimeMs() {
		auto now = std::chrono::system_clock::now();
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			now.time_since_epoch()).count();
	}

	// FIX: Bounded add - remove oldest jika sudah MAX_TOKENS
	void AddTokenInternal(const InterceptedToken& token) {
		// Check duplikat
		for (auto& existing : m_tokens) {
			if (existing.token == token.token &&
			    existing.userId == token.userId) {
				// Update existing
				existing.money = token.money;
				existing.isJackpot = token.isJackpot;
				existing.timestamp = token.timestamp;
				if (!token.nickname.empty()) existing.nickname = token.nickname;
				return;
			}
		}

		// Remove oldest jika penuh
		while (m_tokens.size() >= MAX_TOKENS) {
			m_tokens.pop_front();
		}
		m_tokens.push_back(token);
	}

	// FIX: Buffer overflow protection di ExtractTokenFromJson
	void ExtractTokenFromJson(const uint8_t* data, size_t len, size_t pos, const char* key) {
		size_t keyLen = strlen(key);

		// Validate bounds
		if (pos + keyLen >= len) return;

		// Cari ":" setelah key
		size_t colon = pos + keyLen;
		while (colon < len && data[colon] != ':') colon++;

		// FIX: Check bounds setelah skip ':'
		if (colon >= len) return;
		colon++; // skip :
		if (colon >= len) return;

		// Skip whitespace & quotes
		while (colon < len && (data[colon] == ' ' || data[colon] == '"')) colon++;
		if (colon >= len) return;

		// Find end
		size_t end = colon;
		while (end < len && data[end] != '"' && data[end] != 0 &&
			   data[end] != '\n' && data[end] != '\r') {
			end++;
		}

		// FIX: Final validation - max 10KB token, minimum 4 chars
		if (end > colon && (end - colon) >= 4 && (end - colon) < 10240) {
			InterceptedToken t;
			t.token = std::string((const char*)(data + colon), end - colon);
			t.source = "socket_json";
			t.timestamp = GetTimeMs();
			AddTokenInternal(t);
		}
	}

	// Parse CSV line back to token
	bool ParseTokenLine(const std::string& line, InterceptedToken& t) {
		// Format: token|source|userId|money|nickname|isJackpot|timestamp
		size_t pos = 0;
		std::vector<std::string> parts;

		std::string part;
		for (size_t i = 0; i < line.size(); i++) {
			if (line[i] == '|') {
				parts.push_back(part);
				part.clear();
			} else {
				part += line[i];
			}
		}
		parts.push_back(part);

		if (parts.size() < 7) return false;

		t.token = parts[0];
		t.source = parts[1];
		try {
			t.userId = std::stoi(parts[2]);
			t.money = std::stol(parts[3]);
		} catch (...) {
			return false;
		}
		t.nickname = parts[4];
		t.isJackpot = (parts[5] == "1");
		try {
			t.timestamp = std::stoull(parts[6]);
		} catch (...) {
			t.timestamp = GetTimeMs();
		}

		return !t.token.empty();
	}

	// FIX: Reconnect with retry
	void ReconnectSocketWithRetry(int maxRetries, int delayMs) {
		void* client = GetActiveSocketClient();
		if (!client) {
			LOGE_TOKEN("Cannot get active socket client for reconnect");
			return;
		}

		InitSocketFunctions();

		// Close current connection
		if (pSocketClose) {
			Il2CppString* reason = HGI_CreateString("Reconnect");
			if (reason) pSocketClose(client, reason);
			LOGI_TOKEN("Socket closed for reconnect");
		}

		// Retry connect
		for (int attempt = 1; attempt <= maxRetries; attempt++) {
			usleep(delayMs * 1000);

			LOGI_TOKEN("Reconnect attempt %d/%d...", attempt, maxRetries);

			if (pSocketConnect) {
				pSocketConnect(client);
				LOGI_TOKEN("Reconnect attempt %d sent", attempt);
				// Tunggu sebentar untuk cek koneksi
				usleep(500000); // 500ms
				// Untuk saat ini, assume connected setelah Connect() call
				LOGI_TOKEN("Reconnect attempt %d assumed success", attempt);
				return;
			}
		}

		LOGE_TOKEN("All reconnect attempts failed!");
	}
};
