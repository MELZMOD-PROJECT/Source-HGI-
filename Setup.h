#pragma once
// ============================================================
//  HGI SLOT MOD - Setup Configuration (FIXED v3)
//  Target: Higgs Games Island / Higgs Domino Island 2.85 & 2.86
//  Engine: Unity IL2CPP
// ============================================================
#include <cstdint>
#include <cstddef>

// ---------------------------------------------------------------------------
//  CRITICAL FIX untuk V2.86:
//  
//  V2.85: libil2cpp.so (48.72 MB) + libunity.so (15.81 MB) = TERPISAH
//  V2.86: libgame.so   (79.8 MB) = GABUNGAN (il2cpp + unity jadi satu!)
//  
//  Jadi m_IL2CPPLIB harus DINAMIS:
//   - V2.85: "libil2cpp.so"
//   - V2.86: "libgame.so"
//  
//  Gunakan GameLibName() bukan macro hardcoded.
// ---------------------------------------------------------------------------

// Default library name (akan di-override oleh VersionDetector)
// CATATAN: Jangan pakai #define lagi, pakai extern variable
extern const char* g_IL2CPPLibName;    // Set di runtime oleh VersionDetector
extern const char* g_UnityLibName;     // "libunity.so" atau "libgame.so"

// Legacy compatibility macro (redirect ke variable)
#define m_IL2CPPLIB g_IL2CPPLibName

// GLOBAL VARIABLES (EXTERN DECLARATION ONLY)
// Definisi aktual ada di Main.cpp untuk mencegah multiple definition error
extern uintptr_t m_IL2CPP;
extern bool g_Initialized;
extern bool clearMousePos;
extern void* m_EGL;
extern void* m_ANDROID;
extern void* m_INPUT;
extern float SetFieldOfView;

// Screen dimensions (didefinisikan static di imgui/imconfig.h)
// extern int glWidth;
// extern int glHeight;

// ============================================================
//  HELPER: Screen Resolution
// ============================================================
#define Screen_get_width  ((uintptr_t)Il2CppGetMethodOffset("UnityEngine.dll", "UnityEngine", "Screen", "get_width"))
#define Screen_get_height ((uintptr_t)Il2CppGetMethodOffset("UnityEngine.dll", "UnityEngine", "Screen", "get_height"))
#define Screen_get_dpi    ((uintptr_t)Il2CppGetMethodOffset("UnityEngine.dll", "UnityEngine", "Screen", "get_dpi"))

inline int get_width() {
    if (!Screen_get_width) return 0;
    return reinterpret_cast<int(*)()>(Screen_get_width)();
}

inline int get_height() {
    if (!Screen_get_height) return 0;
    return reinterpret_cast<int(*)()>(Screen_get_height)();
}

inline int get_dpi() {
    if (!Screen_get_dpi) return 0;
    return reinterpret_cast<int(*)()>(Screen_get_dpi)();
}

// ============================================================
//  GAME VERSION ENUM - untuk multi-version support
// ============================================================
enum class GameVersionId {
    V2_85,
    V2_86,
    UNKNOWN
};

// ============================================================
//  HGI SLOT - RVA & Offset Constants
//  Sumber: dump.cs (Assembly-CSharp.dll AOT layer)
// ============================================================
namespace HGIOffsets {
    // ----- HallStartup Field Offsets (biasanya TETAP antar versi) -----
    constexpr uint64_t HallStartup_iUserID      = 0x18;
    constexpr uint64_t HallStartup_szUserToken  = 0x20;
    constexpr uint64_t HallStartup_iVipType     = 0x28;
    constexpr uint64_t HallStartup_iGameID      = 0x2C;
    constexpr uint64_t HallStartup_szNickName   = 0x50;
    constexpr uint64_t HallStartup_szHeadUrl    = 0x58;
    constexpr uint64_t HallStartup_szDeviceId   = 0x68;
    constexpr uint64_t HallStartup_liMoney      = 0x88;

    // ----- SocketManager Field Offsets -----
    constexpr uint64_t SocketManager_s_Instance_off   = 0x0;
    constexpr uint64_t SocketManager_m_Clients_off    = 0x10;
    constexpr uint64_t SocketManager_m_IdMap_off      = 0x18;

    // ----- JsonContentRequest Field Offsets -----
    constexpr uint64_t JsonContentRequest_m_Url_off      = 0x10;
    constexpr uint64_t JsonContentRequest_m_Method_off   = 0x18;
    constexpr uint64_t JsonContentRequest_Header_off     = 0x30;

    // ============================================================
    //  RVA OFFSETS - Version-dependent
    //  Ini yang BERUBAH setiap game update!
    // ============================================================

    // ---------- V2.85 RVA (library: libil2cpp.so ~48.72MB) ----------
    namespace V285 {
        constexpr uint64_t FishingAotEntry_get_HallStartup_RVA = 0x28A7E40;
        constexpr uint64_t SlotAotEntry_Start_RVA = 0x28BA4B0;

        constexpr uint64_t SocketClient_Connect_RVA       = 0x2887FA4;
        constexpr uint64_t SocketClient_DoConnect_RVA     = 0x2888200;
        constexpr uint64_t SocketClient_SendRawBytes_RVA  = 0x288B0C4;
        constexpr uint64_t SocketClient_SendMessage_RVA   = 0x288B2BC;
        constexpr uint64_t SocketClient_Close_RVA         = 0x28896D0;
        constexpr uint64_t SocketClient_HandleReceive_RVA = 0x288AFE8;
        constexpr uint64_t SocketClient_ReceiveThread_RVA = 0x288A7C8;

        constexpr uint64_t SocketManager_CreateSocketClient_RVA = 0x288C024;

        constexpr uint64_t LoginCrypto_Encrypt_RVA = 0x2824F3C;
        constexpr uint64_t LoginCrypto_Decrypt_RVA = 0x28252AC;
        constexpr uint64_t LoginCrypto_Sign_RVA    = 0x28247DC;

        constexpr uint64_t JsonContentRequest_SetCustomHeader_RVA = 0x2872E50;
        constexpr uint64_t JsonContentRequest_Get_RVA = 0x2873230;
        constexpr uint64_t JsonContentRequest_Post_RVA = 0x2873230;

        constexpr uint64_t ServerProfiles_GetHttpURL_RVA    = 0x281845C;
        constexpr uint64_t ServerProfiles_GetResourceURL_RVA = 0x2818330;
        constexpr uint64_t ServerProfiles_GetDNSInfo_RVA    = 0x2818970;
    }

    // ---------- V2.86 RVA (library: libgame.so ~79.8MB) ----------
    // CATATAN KRITIS: V2.86 menggabungkan libil2cpp.so + libunity.so
    //                 menjadi satu library "libgame.so" (79.8 MB)
    //                 SEMUA RVA berubah total!
    //                 HARUS dump ulang dengan Il2CppDumper!
    //                 Untuk sementara, gunakan IL2CPP API resolution (pattern scan)
    namespace V286 {
        // Akan diisi oleh pattern scan (Il2CppGetMethodOffset)
        // Lihat VersionDetector.h untuk implementasi
        constexpr uint64_t FishingAotEntry_get_HallStartup_RVA = 0x0; // TBD
        constexpr uint64_t SlotAotEntry_Start_RVA = 0x0; // TBD
        constexpr uint64_t SocketClient_Connect_RVA       = 0x0; // TBD
        constexpr uint64_t SocketClient_DoConnect_RVA     = 0x0; // TBD
        constexpr uint64_t SocketClient_SendRawBytes_RVA  = 0x0; // TBD
        constexpr uint64_t SocketClient_SendMessage_RVA   = 0x0; // TBD
        constexpr uint64_t SocketClient_Close_RVA         = 0x0; // TBD
        constexpr uint64_t SocketClient_HandleReceive_RVA = 0x0; // TBD
        constexpr uint64_t SocketClient_ReceiveThread_RVA = 0x0; // TBD
        constexpr uint64_t SocketManager_CreateSocketClient_RVA = 0x0; // TBD
        constexpr uint64_t LoginCrypto_Encrypt_RVA = 0x0; // TBD
        constexpr uint64_t LoginCrypto_Decrypt_RVA = 0x0; // TBD
        constexpr uint64_t LoginCrypto_Sign_RVA    = 0x0; // TBD
        constexpr uint64_t JsonContentRequest_SetCustomHeader_RVA = 0x0; // TBD
        constexpr uint64_t JsonContentRequest_Get_RVA = 0x0; // TBD
        constexpr uint64_t JsonContentRequest_Post_RVA = 0x0; // TBD
        constexpr uint64_t ServerProfiles_GetHttpURL_RVA    = 0x0; // TBD
        constexpr uint64_t ServerProfiles_GetResourceURL_RVA = 0x0; // TBD
        constexpr uint64_t ServerProfiles_GetDNSInfo_RVA    = 0x0; // TBD
    }

    // ============================================================
    //  ACTIVE OFFSETS - Set berdasarkan versi yang terdeteksi
    //  Default: V2.85 values
    // ============================================================
    extern uint64_t FishingAotEntry_get_HallStartup_RVA;
    extern uint64_t SlotAotEntry_Start_RVA;
    extern uint64_t SocketClient_Connect_RVA;
    extern uint64_t SocketClient_DoConnect_RVA;
    extern uint64_t SocketClient_SendRawBytes_RVA;
    extern uint64_t SocketClient_SendMessage_RVA;
    extern uint64_t SocketClient_Close_RVA;
    extern uint64_t SocketClient_HandleReceive_RVA;
    extern uint64_t SocketClient_ReceiveThread_RVA;
    extern uint64_t SocketManager_CreateSocketClient_RVA;
    extern uint64_t LoginCrypto_Encrypt_RVA;
    extern uint64_t LoginCrypto_Decrypt_RVA;
    extern uint64_t LoginCrypto_Sign_RVA;
    extern uint64_t JsonContentRequest_SetCustomHeader_RVA;
    extern uint64_t JsonContentRequest_Get_RVA;
    extern uint64_t JsonContentRequest_Post_RVA;
    extern uint64_t ServerProfiles_GetHttpURL_RVA;
    extern uint64_t ServerProfiles_GetResourceURL_RVA;
    extern uint64_t ServerProfiles_GetDNSInfo_RVA;
}
