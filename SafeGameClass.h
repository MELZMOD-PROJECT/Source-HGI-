#pragma once
// ============================================================
//  HGI SLOT MOD - SafeGameClass.h
//  Null-safe wrappers untuk akses game objects
//  Mencegah crash saat game belum ready / HallStartup null
// ============================================================

#include "feature/GameClass.h"
#include <android/log.h>

#define LOGI_SAFE(...) __android_log_print(ANDROID_LOG_INFO, "HGI_Safe", __VA_ARGS__)

class SafeHallStartup {
public:
    // Get HallStartup dengan full validation
    // Return nullptr jika tidak valid
    static HallStartup* Get() {
        HallStartup* hs = GetActiveHallStartup();
        if (!hs) return nullptr;

        // Validate pointer - cek klass tidak null (IL2CPP object harus punya klass)
        if (!hs->klass) return nullptr;

        return hs;
    }

    // Check apakah HallStartup valid dan punya token
    static bool IsValid(HallStartup* hs) {
        if (!hs) return false;
        if (!hs->klass) return false;
        return true;
    }

    // Check apakah token accessible
    static bool HasToken(HallStartup* hs) {
        if (!IsValid(hs)) return false;
        if (!hs->szUserToken) return false;
        return true;
    }

    // Safe read money (return 0 jika invalid)
    static long GetMoney(HallStartup* hs) {
        if (!IsValid(hs)) return 0;
        return hs->liMoney;
    }

    // Safe read UserID (return -1 jika invalid)
    static int GetUserId(HallStartup* hs) {
        if (!IsValid(hs)) return -1;
        return hs->iUserID;
    }

    // Safe read token string (return "" jika invalid)
    static std::string GetToken(HallStartup* hs) {
        if (!HasToken(hs)) return "";
        return Il2CppStringToUtf8(hs->szUserToken);
    }

    // Safe read nickname (return "" jika invalid)
    static std::string GetNickname(HallStartup* hs) {
        if (!IsValid(hs)) return "";
        if (!hs->szNickName) return "";
        return Il2CppStringToUtf8(hs->szNickName);
    }
};

