#pragma once

// ============================================================
//  HGI SLOT MOD - GameClass.h
//  IL2CPP class wrapper untuk Higgs Domino SLOT
//  Sumber: dump.cs Assembly-CSharp.dll AOT layer
// ============================================================

#include "config/Setup.h"
#include "struct/MonoString.h"
#include <android/log.h>
#include <dlfcn.h>

#define LOGI_CLASS(...) __android_log_print(ANDROID_LOG_INFO, "HGI_GameClass", __VA_ARGS__)

// ============================================================
//  HALL STARTUP STRUCT (TypeDefIndex 6460)
//  Class data login utama: token, userID, money
// ============================================================
struct HallStartup {
	void* klass;             // 0x0
	void* monitor;           // 0x8
	void* Json_backing;      // 0x10
	int iUserID;             // 0x18
	Il2CppString* szUserToken; // 0x20  <-- TOKEN LOGIN
	int iVipType;            // 0x28
	int iGameID;             // 0x2C
	int iFishingLobby;       // 0x30
	int iClientResVer;       // 0x34
	int iLaguageType;        // 0x38
	Il2CppString* szCocosWritablePath; // 0x40
	Il2CppString* szCocosSDCardPath;   // 0x48
	Il2CppString* szNickName;  // 0x50
	Il2CppString* szHeadUrl;   // 0x58
	int iSexType;            // 0x60
	int iShowID;             // 0x64
	Il2CppString* szDeviceId; // 0x68
	int iTotalRam;           // 0x70
	int bEventTrack;         // 0x74
	Il2CppString* szEventTrackSession; // 0x78
	int iEventTrackSeq;      // 0x80
	float fNowExp;           // 0x84
	long liMoney;            // 0x88  <-- UANG PEMAIN (JP DETECTION)
};

// ============================================================
//  SocketClient.Header struct (Core.Network.Sockets)
// ============================================================
struct SocketHeader {
	int routeCode;   // 0x0
	int msgId;       // 0x4
	short buff;      // 0x8
};

// ============================================================
//  Il2CppArray helper untuk byte[] packet
// ============================================================
struct ByteArray {
	void* klass;
	void* monitor;
	void* bounds;
	int max_length;
	uint8_t data[1];
};

// ============================================================
//  HELPER: Ambil HallStartup aktif via FishingAotEntry.get_HallStartup()
//  RVA: 0x28A7E40 (static method, no args)
// ============================================================
typedef HallStartup* (*GetHallStartup_t)();
static GetHallStartup_t pGetHallStartup = nullptr;

HallStartup* GetActiveHallStartup() {
	if (!pGetHallStartup) {
		if (!m_IL2CPP) return nullptr;
		pGetHallStartup = (GetHallStartup_t)(m_IL2CPP + HGIOffsets::FishingAotEntry_get_HallStartup_RVA);
	}
	if (pGetHallStartup) {
		return pGetHallStartup();
	}
	return nullptr;
}

// ============================================================
//  HELPER: Convert Il2CppString ke std::string (UTF-16 -> UTF-8)
// ============================================================
std::string Il2CppStringToUtf8(Il2CppString* str) {
	if (!str) return "";
	std::string out;
	int len = str->getLength();
	uint16_t* chars = (uint16_t*)&str->start_char;
	out.reserve(len);
	for (int i = 0; i < len; i++) {
		uint16_t c = chars[i];
		if (c < 0x80) out += (char)c;
		else if (c < 0x800) {
			out += (char)(0xC0 | (c >> 6));
			out += (char)(0x80 | (c & 0x3F));
		} else {
			out += (char)(0xE0 | (c >> 12));
			out += (char)(0x80 | ((c >> 6) & 0x3F));
			out += (char)(0x80 | (c & 0x3F));
		}
	}
	return out;
}

// ============================================================
//  HELPER: Buat Il2CppString baru
//  Via il2cpp_string_new (exported dari libunity.so)
// ============================================================
typedef Il2CppString* (*Il2CppStringNew_t)(const char*);
static Il2CppStringNew_t pIl2CppStringNew = nullptr;

Il2CppString* HGI_CreateString(const char* str) {
	if (!str) return nullptr; // FIX: null check input
	if (!pIl2CppStringNew) {
		// FIX: Coba library yang aktif dulu (libgame.so untuk V2.86, libil2cpp.so untuk V2.85)
		void* h = dlopen_ex(g_IL2CPPLibName, RTLD_NOW);
		if (h) {
			pIl2CppStringNew = (Il2CppStringNew_t)dlsym_ex(h, "il2cpp_string_new");
		}
		// Fallback ke libunity.so jika berbeda dari IL2CPP lib
		if (!pIl2CppStringNew && strcmp(g_IL2CPPLibName, g_UnityLibName) != 0) {
			h = dlopen_ex(g_UnityLibName, RTLD_NOW);
			if (h) {
				pIl2CppStringNew = (Il2CppStringNew_t)dlsym_ex(h, "il2cpp_string_new");
			}
		}
		// Fallback terakhir: standard dlopen
		if (!pIl2CppStringNew) {
			h = dlopen(g_IL2CPPLibName, RTLD_NOW);
			if (h) {
				pIl2CppStringNew = (Il2CppStringNew_t)dlsym(h, "il2cpp_string_new");
			}
		}
	}
	if (pIl2CppStringNew) {
		return pIl2CppStringNew(str);
	}
	return nullptr;
}

// ============================================================
//  SOCKETCLIENT method typedefs
// ============================================================
typedef void (*SocketClient_Connect_t)(void* thisPtr);
typedef void (*SocketClient_Close_t)(void* thisPtr, Il2CppString* reason);
typedef bool (*SocketClient_SendRawBytes_t)(void* thisPtr, ByteArray* bytes);
typedef void (*SocketClient_SendMessage_t)(void* thisPtr, int routeCode, int msgId, void* stream);

static SocketClient_Connect_t      pSocketConnect = nullptr;
static SocketClient_Close_t        pSocketClose = nullptr;
static SocketClient_SendRawBytes_t pSocketSendRaw = nullptr;
static SocketClient_SendMessage_t  pSocketSendMsg = nullptr;

void InitSocketFunctions() {
	if (!m_IL2CPP) return;
	if (!pSocketConnect) {
		pSocketConnect = (SocketClient_Connect_t)(m_IL2CPP + HGIOffsets::SocketClient_Connect_RVA);
		pSocketClose   = (SocketClient_Close_t)(m_IL2CPP + HGIOffsets::SocketClient_Close_RVA);
		pSocketSendRaw = (SocketClient_SendRawBytes_t)(m_IL2CPP + HGIOffsets::SocketClient_SendRawBytes_RVA);
		pSocketSendMsg = (SocketClient_SendMessage_t)(m_IL2CPP + HGIOffsets::SocketClient_SendMessage_RVA);
	}
}

// ============================================================
//  Ambil SocketClient aktif dari SocketManager.m_Clients[0]
//  SocketManager TypeDefIndex 6392
//  s_Instance (static) : 0x0
//  m_Clients (List<SocketClient>) : 0x10
// ============================================================
// ============================================================
//  Ambil SocketClient aktif dari SocketManager.s_Instance
//  SocketManager TypeDefIndex 6392
//  static field: s_Instance
//  m_Clients (List<SocketClient>) : offset 0x10
// ============================================================
void* GetActiveSocketClient() {
	if (!m_IL2CPP) return nullptr;

	// Resolve SocketManager.s_Instance via il2cpp API
	void* mgr = nullptr;
	Il2CppGetStaticFieldValue(
		"Assembly-CSharp.dll",
		"Core.Network.Sockets",
		"SocketManager",
		"s_Instance",
		&mgr
	);
	if (!mgr) {
		LOGI_CLASS("SocketManager.s_Instance = null");
		return nullptr;
	}

	// m_Clients at offset 0x10 (List<SocketClient>*)
	Il2CppList<void*>* clients = *(Il2CppList<void*>**)((char*)mgr + HGIOffsets::SocketManager_m_Clients_off);
	if (!clients || clients->getSize() <= 0) {
		LOGI_CLASS("SocketManager.m_Clients empty");
		return nullptr;
	}

	return clients->getItems()[0];
}

// ============================================================
//  Reconnect SocketClient: Close -> sleep -> Connect
// ============================================================
void ReconnectSocket() {
	void* client = GetActiveSocketClient();
	if (!client) return;

	InitSocketFunctions();

	if (pSocketClose) {
		Il2CppString* reason = HGI_CreateString("");
		pSocketClose(client, reason);
	}
	usleep(500000); // 0.5s

	if (pSocketConnect) {
		pSocketConnect(client);
	}
}
