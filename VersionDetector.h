#pragma once
// ============================================================
//  HGI SLOT MOD - VersionDetector.h (v3)
//  Auto-detect game version & load correct offsets
//  CRITICAL: V2.86 uses "libgame.so" (combined il2cpp+unity)
//            V2.85 uses separate "libil2cpp.so" + "libunity.so"
// ============================================================

#include <string>
#include <cstring>
#include <android/log.h>
#include "config/Setup.h"

#define LOGI_VER(...) __android_log_print(ANDROID_LOG_INFO, "HGI_Version", __VA_ARGS__)
#define LOGE_VER(...) __android_log_print(ANDROID_LOG_ERROR, "HGI_Version", __VA_ARGS__)

// ============================================================
//  LIBRARY NAME DEFINITIONS (runtime, not compile-time)
// ============================================================
const char* g_IL2CPPLibName = "libil2cpp.so";  // Default V2.85
const char* g_UnityLibName  = "libunity.so";   // Default V2.85

// ============================================================
//  ACTIVE OFFSETS DEFINITION (extern declared in Setup.h)
//  Default = V2.85 values
// ============================================================
namespace HGIOffsets {
    uint64_t FishingAotEntry_get_HallStartup_RVA = V285::FishingAotEntry_get_HallStartup_RVA;
    uint64_t SlotAotEntry_Start_RVA = V285::SlotAotEntry_Start_RVA;
    uint64_t SocketClient_Connect_RVA       = V285::SocketClient_Connect_RVA;
    uint64_t SocketClient_DoConnect_RVA     = V285::SocketClient_DoConnect_RVA;
    uint64_t SocketClient_SendRawBytes_RVA  = V285::SocketClient_SendRawBytes_RVA;
    uint64_t SocketClient_SendMessage_RVA   = V285::SocketClient_SendMessage_RVA;
    uint64_t SocketClient_Close_RVA         = V285::SocketClient_Close_RVA;
    uint64_t SocketClient_HandleReceive_RVA = V285::SocketClient_HandleReceive_RVA;
    uint64_t SocketClient_ReceiveThread_RVA = V285::SocketClient_ReceiveThread_RVA;
    uint64_t SocketManager_CreateSocketClient_RVA = V285::SocketManager_CreateSocketClient_RVA;
    uint64_t LoginCrypto_Encrypt_RVA = V285::LoginCrypto_Encrypt_RVA;
    uint64_t LoginCrypto_Decrypt_RVA = V285::LoginCrypto_Decrypt_RVA;
    uint64_t LoginCrypto_Sign_RVA    = V285::LoginCrypto_Sign_RVA;
    uint64_t JsonContentRequest_SetCustomHeader_RVA = V285::JsonContentRequest_SetCustomHeader_RVA;
    uint64_t JsonContentRequest_Get_RVA = V285::JsonContentRequest_Get_RVA;
    uint64_t JsonContentRequest_Post_RVA = V285::JsonContentRequest_Post_RVA;
    uint64_t ServerProfiles_GetHttpURL_RVA    = V285::ServerProfiles_GetHttpURL_RVA;
    uint64_t ServerProfiles_GetResourceURL_RVA = V285::ServerProfiles_GetResourceURL_RVA;
    uint64_t ServerProfiles_GetDNSInfo_RVA    = V285::ServerProfiles_GetDNSInfo_RVA;
}

class VersionDetector {
public:
    // ============================================================
    //  STEP 1: Detect which library exists (BEFORE m_IL2CPP is set!)
    //  Harus dipanggil PALING AWAL di main_thread
    //  Return: base address dari library yang ditemukan
    // ============================================================
    static uintptr_t DetectAndSetLibrary() {
        uintptr_t base = 0;

        // Coba libgame.so dulu (V2.86 style - combined library)
        base = Tools::GetBaseAddress("libgame.so");
        if (base) {
            g_IL2CPPLibName = "libgame.so";
            g_UnityLibName  = "libgame.so";  // Same lib!
            LOGI_VER("Found libgame.so at %p -> V2.86 (combined library)", (void*)base);
            return base;
        }

        // Coba libil2cpp.so (V2.85 style - separate libraries)
        base = Tools::GetBaseAddress("libil2cpp.so");
        if (base) {
            g_IL2CPPLibName = "libil2cpp.so";
            g_UnityLibName  = "libunity.so";
            LOGI_VER("Found libil2cpp.so at %p -> V2.85 (separate libraries)", (void*)base);
            return base;
        }

        LOGE_VER("CRITICAL: Neither libgame.so nor libil2cpp.so found!");
        return 0;
    }

    // ============================================================
    //  STEP 2: Detect game version setelah IL2CPP attached
    // ============================================================
    static GameVersionId DetectVersion() {
        // Identify berdasarkan library name
        if (strcmp(g_IL2CPPLibName, "libgame.so") == 0) {
            LOGI_VER("Detected V2.86 (libgame.so)");
            return GameVersionId::V2_86;
        }
        if (strcmp(g_IL2CPPLibName, "libil2cpp.so") == 0) {
            LOGI_VER("Detected V2.85 (libil2cpp.so)");
            return GameVersionId::V2_85;
        }

        return GameVersionId::UNKNOWN;
    }

    // ============================================================
    //  STEP 3: Load offsets untuk versi tertentu
    //  V2.85 = pakai hardcoded RVA (sudah proven)
    //  V2.86 = HARUS pakai IL2CPP API resolution (pattern scan)
    // ============================================================
    static bool LoadOffsets(GameVersionId version) {
        switch (version) {
            case GameVersionId::V2_85:
                LoadV285();
                LOGI_VER("Loaded V2.85 hardcoded offsets");
                return true;

            case GameVersionId::V2_86:
                LOGI_VER("V2.86 detected - using IL2CPP API resolution for ALL offsets");
                return ResolveAllOffsetsViaAPI();

            default:
                LOGE_VER("Unknown version, trying IL2CPP API resolution...");
                return ResolveAllOffsetsViaAPI();
        }
    }

    static const char* VersionString(GameVersionId ver) {
        switch (ver) {
            case GameVersionId::V2_85: return "2.85";
            case GameVersionId::V2_86: return "2.86";
            default: return "Unknown";
        }
    }

private:
    static void LoadV285() {
        using namespace HGIOffsets;
        FishingAotEntry_get_HallStartup_RVA = V285::FishingAotEntry_get_HallStartup_RVA;
        SlotAotEntry_Start_RVA = V285::SlotAotEntry_Start_RVA;
        SocketClient_Connect_RVA       = V285::SocketClient_Connect_RVA;
        SocketClient_DoConnect_RVA     = V285::SocketClient_DoConnect_RVA;
        SocketClient_SendRawBytes_RVA  = V285::SocketClient_SendRawBytes_RVA;
        SocketClient_SendMessage_RVA   = V285::SocketClient_SendMessage_RVA;
        SocketClient_Close_RVA         = V285::SocketClient_Close_RVA;
        SocketClient_HandleReceive_RVA = V285::SocketClient_HandleReceive_RVA;
        SocketClient_ReceiveThread_RVA = V285::SocketClient_ReceiveThread_RVA;
        SocketManager_CreateSocketClient_RVA = V285::SocketManager_CreateSocketClient_RVA;
        LoginCrypto_Encrypt_RVA = V285::LoginCrypto_Encrypt_RVA;
        LoginCrypto_Decrypt_RVA = V285::LoginCrypto_Decrypt_RVA;
        LoginCrypto_Sign_RVA    = V285::LoginCrypto_Sign_RVA;
        JsonContentRequest_SetCustomHeader_RVA = V285::JsonContentRequest_SetCustomHeader_RVA;
        JsonContentRequest_Get_RVA = V285::JsonContentRequest_Get_RVA;
        JsonContentRequest_Post_RVA = V285::JsonContentRequest_Post_RVA;
        ServerProfiles_GetHttpURL_RVA    = V285::ServerProfiles_GetHttpURL_RVA;
        ServerProfiles_GetResourceURL_RVA = V285::ServerProfiles_GetResourceURL_RVA;
        ServerProfiles_GetDNSInfo_RVA    = V285::ServerProfiles_GetDNSInfo_RVA;
    }

    // ============================================================
    //  RESOLVE SEMUA OFFSET VIA IL2CPP API
    //  Ini cara yang BENAR dan VERSION-INDEPENDENT.
    //  Tidak perlu hardcode RVA — langsung tanya il2cpp runtime!
    // ============================================================
    static bool ResolveAllOffsetsViaAPI() {
        using namespace HGIOffsets;
        bool success = true;
        int resolved = 0;
        int failed = 0;

        // Helper lambda
        auto resolveMethod = [&](uint64_t& rvaOut, const char* image,
                                 const char* ns, const char* klass,
                                 const char* method, int paramCount) -> bool {
            void* addr = Il2CppGetMethodOffset(image, ns, klass, method, paramCount);
            if (addr && m_IL2CPP) {
                rvaOut = (uint64_t)((uintptr_t)addr - m_IL2CPP);
                LOGI_VER("  Resolved %s::%s -> RVA 0x%llX", klass, method, rvaOut);
                resolved++;
                return true;
            }
            LOGE_VER("  FAILED to resolve %s::%s (ns=%s)", klass, method, ns);
            failed++;
            return false;
        };

        LOGI_VER("=== Resolving offsets via IL2CPP API ===");

        // --- SocketClient methods ---
        const char* asmDll = OBFUSCATE("Assembly-CSharp.dll");
        const char* socketNs = "Core.Network.Sockets";

        resolveMethod(SocketClient_SendRawBytes_RVA, asmDll,
            socketNs, OBFUSCATE("SocketClient"), OBFUSCATE("SendRawBytes"), 1);
        resolveMethod(SocketClient_SendMessage_RVA, asmDll,
            socketNs, OBFUSCATE("SocketClient"), OBFUSCATE("SendMessage"), 3);
        resolveMethod(SocketClient_Connect_RVA, asmDll,
            socketNs, OBFUSCATE("SocketClient"), OBFUSCATE("Connect"), 0);
        resolveMethod(SocketClient_Close_RVA, asmDll,
            socketNs, OBFUSCATE("SocketClient"), OBFUSCATE("Close"), 1);

        // --- FishingAotEntry ---
        // Namespace mungkin berbeda antar versi, coba beberapa
        bool hallFound = resolveMethod(FishingAotEntry_get_HallStartup_RVA, asmDll,
            "", OBFUSCATE("FishingAotEntry"), OBFUSCATE("get_HallStartup"), 0);
        if (!hallFound) {
            // Coba namespace lain
            hallFound = resolveMethod(FishingAotEntry_get_HallStartup_RVA, asmDll,
                "AOT", OBFUSCATE("FishingAotEntry"), OBFUSCATE("get_HallStartup"), 0);
        }
        if (!hallFound) {
            // Coba tanpa obfuscate
            hallFound = resolveMethod(FishingAotEntry_get_HallStartup_RVA, asmDll,
                "", "FishingAotEntry", "get_HallStartup", 0);
        }

        if (!hallFound) {
            LOGE_VER("CRITICAL: Cannot resolve get_HallStartup! Token features will fail.");
            success = false;
        }

        // --- LoginCrypto ---
        resolveMethod(LoginCrypto_Encrypt_RVA, asmDll,
            "", OBFUSCATE("LoginCrypto"), OBFUSCATE("Encrypt"), -1);
        resolveMethod(LoginCrypto_Decrypt_RVA, asmDll,
            "", OBFUSCATE("LoginCrypto"), OBFUSCATE("Decrypt"), -1);

        LOGI_VER("=== Resolution complete: %d resolved, %d failed ===", resolved, failed);

        // Untuk critical hooks (SendRawBytes, get_HallStartup), return false jika gagal
        if (SocketClient_SendRawBytes_RVA == 0) {
            LOGE_VER("CRITICAL: SendRawBytes not resolved!");
            success = false;
        }

        return success;
    }
};
