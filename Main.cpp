#include <iostream>
#include <stdio.h>
#include <string>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>
#include <vector>
#include <map>
#include <chrono>
#include <fstream>
#include <thread>
#include <memory>
#include <atomic>
#include <pthread.h>
#include <dirent.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <jni.h>
#include <android/log.h>
#include <elf.h>
#include <dlfcn.h>
#include <sys/system_properties.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include "Includes/obfuscate.h"
#include "imgui.h"
#include "imgui_additional.h"
#include "backends/imgui_impl_android.h"
#include "backends/imgui_impl_opengl3.h"
#include "android_native_app_glue.h"
#include "fonts/GoogleSans.h"
#include "Icon.h"
#include "Iconcpp.h"
#include "struct/Vector3.hpp"
#include "struct/Vector2.h"
#include "struct/Color.h"
#include "struct/Rect.h"
#include "struct/Quaternion.h"
#include "struct/MonoString.h"
#include "unity/fake_dlfcn.h"
#include "unity/Il2Cpp.h"
#include "unity/Tools.h"
#include "unity/Unity.h"
#include "config/Setup.h"
#include "config/VersionDetector.h"    // NEW: Version detection
#include "feature/GameClass.h"
#include "feature/SafeGameClass.h"     // NEW: Null-safe wrappers
#include "feature/SlotTokenInterceptor.h"
#include "feature/Protection.h"
#include "Tools/ImGuiStuff.h"
#include "Tools/ThreadSafeScreenDimension.h"  // NEW: Thread-safe screen dim
#include "Tools/FileLogger.h"                 // NEW: File logging

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "HGI_SLOT", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "HGI_SLOT", __VA_ARGS__)

// ============================================================
//  GLOBAL VARIABLE DEFINITIONS
// ============================================================
uintptr_t m_IL2CPP = 0;
bool g_Initialized = false;
bool clearMousePos = true;
void* m_EGL = nullptr;
void* m_ANDROID = nullptr;
void* m_INPUT = nullptr;
float SetFieldOfView = 0;

JavaVM* jvm = nullptr;
static std::atomic<bool> g_modReady{false};     // FIX: atomic untuk thread safety
static bool g_autoLoginOnJP = true;
static int g_jpThreshold = 10000000;
static bool g_hookFailed = false;               // NEW: Track hook status
static GameVersionId g_detectedVersion = GameVersionId::UNKNOWN;  // NEW: Detected version

// FIX: Thread handle untuk proper cleanup
static std::unique_ptr<std::thread> g_scannerThread;

// NEW: Search/filter state untuk UI
static char searchBuffer[128] = {0};
static long minMoneyFilter = 0;
static bool showOnlyJP = false;
static int sortMode = 0; // 0=time, 1=money, 2=userID

// ============================================================
//  HOOK SocketClient.SendRawBytes (FIXED: null check)
// ============================================================
typedef bool (*SendRawBytes_t)(void* thisPtr, ByteArray* bytes);
static SendRawBytes_t orig_SendRawBytes = nullptr;

bool Hooked_SendRawBytes(void* thisPtr, ByteArray* bytes) {
    // FIX: Lebih ketat validasi
    if (bytes && bytes->max_length > 0 && bytes->max_length < 65536) {
        SlotTokenInterceptor::Instance().ScanPacket(bytes->data, bytes->max_length);
    }
    return orig_SendRawBytes(thisPtr, bytes);
}

// ============================================================
//  HOOK SocketClient.SendMessage (FIXED: null check)
// ============================================================
typedef void (*SendMessage_t)(void* thisPtr, int routeCode, int msgId, void* stream);
static SendMessage_t orig_SendMessage = nullptr;

void Hooked_SendMessage(void* thisPtr, int routeCode, int msgId, void* stream) {
    LOGI("SocketClient.SendMessage routeCode=%d msgId=%d", routeCode, msgId);
    FLOG_I("SendMessage routeCode=%d msgId=%d", routeCode, msgId);
    if (routeCode == 1) {
        LOGI("[HGI-SLOT] LOGIN packet detected!");
        FLOG_I("LOGIN packet detected!");
    }
    orig_SendMessage(thisPtr, routeCode, msgId, stream);
}

// ============================================================
//  HallStartup Scanner Thread (FIXED: proper lifecycle)
// ============================================================
void HallStartupScannerThread() {
    LOGI("HallStartupScannerThread started");
    FLOG_I("Scanner thread started");

    while (g_modReady.load()) {
        usleep(2000000); // 2 seconds

        if (!g_modReady.load()) break; // Check setelah sleep

        // FIX: Gunakan SafeHallStartup
        HallStartup* hs = SafeHallStartup::Get();
        if (hs) {
            SlotTokenInterceptor::Instance().ScanHallStartup(hs);

            if (g_autoLoginOnJP && hs->liMoney > (long)g_jpThreshold) {
                LOGI("!!! JP DETECTED: UserID=%d Money=%lld !!!",
                     hs->iUserID, hs->liMoney);
                FLOG_I("JP DETECTED: UserID=%d Money=%lld",
                       hs->iUserID, hs->liMoney);
            }
        }
    }

    LOGI("HallStartupScannerThread stopped");
    FLOG_I("Scanner thread stopped");
}

// ============================================================
//  Install Hooks via Dobby (FIXED: error handling + return value)
// ============================================================
bool InstallHooks() {
    LOGI("Installing HGI SLOT hooks...");
    FLOG_I("Installing hooks...");
    bool success = true;

    // Hook 1: SendRawBytes
    void* sendRawAddr = (void*)(m_IL2CPP + HGIOffsets::SocketClient_SendRawBytes_RVA);
    // FIX: Validate address sebelum hook
    if (Tools::IsPtrValid(sendRawAddr)) {
        int ret = DobbyHook(sendRawAddr, (void*)Hooked_SendRawBytes, (void**)&orig_SendRawBytes);
        if (ret != 0) {
            LOGE("Hook SendRawBytes FAILED! ret=%d addr=%p", ret, sendRawAddr);
            FLOG_E("Hook SendRawBytes FAILED! ret=%d", ret);
            success = false;
        } else {
            LOGI("Hook SendRawBytes OK: addr=%p", sendRawAddr);
            FLOG_I("Hook SendRawBytes OK at %p", sendRawAddr);
        }
    } else {
        LOGE("SendRawBytes address INVALID: %p", sendRawAddr);
        FLOG_E("SendRawBytes address invalid");
        success = false;
    }

    // Hook 2: SendMessage
    void* sendMsgAddr = (void*)(m_IL2CPP + HGIOffsets::SocketClient_SendMessage_RVA);
    if (Tools::IsPtrValid(sendMsgAddr)) {
        int ret = DobbyHook(sendMsgAddr, (void*)Hooked_SendMessage, (void**)&orig_SendMessage);
        if (ret != 0) {
            LOGE("Hook SendMessage FAILED! ret=%d addr=%p", ret, sendMsgAddr);
            FLOG_E("Hook SendMessage FAILED! ret=%d", ret);
            success = false;
        } else {
            LOGI("Hook SendMessage OK: addr=%p", sendMsgAddr);
            FLOG_I("Hook SendMessage OK at %p", sendMsgAddr);
        }
    } else {
        LOGE("SendMessage address INVALID: %p", sendMsgAddr);
        success = false;
    }

    // DNS bypass hooks (with individual error handling)
    auto hookDns = [&](const char* methodName, int paramCount, void* hookFunc, void** origFunc) -> bool {
        void* addr = Il2CppGetMethodOffset(
            OBFUSCATE("Assembly-CSharp.dll"), "System.Net",
            OBFUSCATE("Dns"), methodName, paramCount);
        if (!addr) {
            LOGE("Cannot resolve Dns::%s (paramCount=%d)", methodName, paramCount);
            FLOG_E("DNS resolve fail: %s", methodName);
            return false;
        }
        Tools::Hook(addr, hookFunc, origFunc);
        LOGI("Hook Dns::%s OK", methodName);
        return true;
    };

    hookDns(OBFUSCATE("GetHostByName_internal"), 4,
            (void*)Dns_GetHostByName_internal, (void**)&oDns_GetHostByName_internal);
    hookDns(OBFUSCATE("GetHostByAddr_internal"), 4,
            (void*)Dns_GetHostByAddr_internal, (void**)&oDns_GetHostByAddr_internal);
    hookDns(OBFUSCATE("hostent_to_IPHostEntry"), 3,
            (void*)Dns_hostent_to_IPHostEntry, (void**)&oDns_hostent_to_IPHostEntry);
    hookDns(OBFUSCATE("GetHostByAddressFromString"), 2,
            (void*)Dns_GetHostByAddressFromString, (void**)&oDns_GetHostByAddressFromString);
    hookDns(OBFUSCATE("GetHostEntry"), 1,
            (void*)Dns_GetHostEntry, (void**)&oDns_GetHostEntry);
    hookDns(OBFUSCATE("GetHostAddresses"), 1,
            (void*)Dns_GetHostAddresses, (void**)&oDns_GetHostAddresses);
    hookDns(OBFUSCATE("GetHostByName"), 1,
            (void*)Dns_GetHostByName, (void**)&oDns_GetHostByName);

    if (!success) {
        LOGE("Some hooks failed, mod may work in LIMITED MODE");
        FLOG_E("Hook installation partially failed");
    } else {
        LOGI("All HGI SLOT hooks installed successfully!");
        FLOG_I("All hooks installed OK");
    }

    return success;
}

// ============================================================
//  ImGui Menu - SLOT Token Interceptor (FIXED + NEW FEATURES)
// ============================================================
void RenderSlotMenu() {
    // FIX: Gunakan ThreadSafeScreenDimension
    int width = ScreenDimension::Instance().Width();
    int height = ScreenDimension::Instance().Height();
    if (width <= 0 || height <= 0) {
        width = glWidth;
        height = glHeight;
    }

    ImGui::SetNextWindowSize(ImVec2((float)width * 0.5f, (float)height * 0.7f), ImGuiCond_Once);
    if (ImGui::Begin(OBFUSCATE("HGI SLOT MOD"), nullptr, ImGuiWindowFlags_NoBringToFrontOnFocus)) {

        // Show version info
        ImGui::TextColored(ImColor(100, 200, 255),
            "Version: v3.0 | Game: %s | %s",
            VersionDetector::VersionString(g_detectedVersion),
            g_hookFailed ? "LIMITED MODE" : "FULL MODE");

        if (ImGui::BeginTabBar("Tab", ImGuiTabBarFlags_FittingPolicyScroll)) {

            // ============================================================
            // TAB: Token Interceptor (UPDATED with search/filter)
            // ============================================================
            if (ImGui::BeginTabItem("Token Interceptor")) {
                ImGui::BeginGroupPanel("Slot Token Interceptor", ImVec2(0, 0));
                ImGui::TextColored(ImColor(255, 200, 0), "HIGGS DOMINO - SLOT GAME");
                ImGui::Separator();
                ImGui::Text("Status: ");
                ImGui::SameLine();
                if (g_hookFailed) {
                    ImGui::TextColored(ImColor(255, 200, 0), "LIMITED");
                } else {
                    ImGui::TextColored(ImColor(0, 255, 0), "ACTIVE");
                }

                auto tokens = SlotTokenInterceptor::Instance().GetTokens();
                ImGui::Text("Tokens captured: %d / %d", (int)tokens.size(),
                           (int)SlotTokenInterceptor::MAX_TOKENS);

                // Buttons row
                if (ImGui::Button("Clear Tokens")) SlotTokenInterceptor::Instance().ClearTokens();
                ImGui::SameLine();
                if (ImGui::Button("Scan Now")) {
                    HallStartup* hs = SafeHallStartup::Get();
                    if (hs) SlotTokenInterceptor::Instance().ScanHallStartup(hs);
                }
                ImGui::SameLine();
                if (ImGui::Button("Save")) SlotTokenInterceptor::Instance().SaveToDisk();
                ImGui::SameLine();
                if (ImGui::Button("Load")) SlotTokenInterceptor::Instance().LoadFromDisk();

                // NEW: Restore own account button
                if (SlotTokenInterceptor::Instance().HasBackup()) {
                    ImGui::SameLine();
                    if (ImGui::Button("Restore My Account")) {
                        SlotTokenInterceptor::Instance().RestoreOwnAccount();
                    }
                }

                ImGui::Separator();

                // NEW: Search & Filter
                ImGui::Text("Filters:");
                ImGui::InputText("Search", searchBuffer, sizeof(searchBuffer));
                ImGui::Checkbox("JP Only", &showOnlyJP);
                ImGui::SameLine();
                ImGui::Combo("Sort", &sortMode, "Timestamp\0Money\0UserID\0");
                ImGui::SameLine();
                if (ImGui::Button("Reset")) {
                    memset(searchBuffer, 0, sizeof(searchBuffer));
                    minMoneyFilter = 0;
                    showOnlyJP = false;
                    sortMode = 0;
                }

                // Apply filters
                std::vector<InterceptedToken> filtered;
                for (const auto& t : tokens) {
                    if (showOnlyJP && !t.isJackpot) continue;
                    if (t.money < minMoneyFilter) continue;

                    if (searchBuffer[0] != '\0') {
                        std::string search = searchBuffer;
                        std::string nick = t.nickname;
                        std::string userIdStr = std::to_string(t.userId);

                        // Case-insensitive search
                        std::transform(search.begin(), search.end(), search.begin(), ::tolower);
                        std::transform(nick.begin(), nick.end(), nick.begin(), ::tolower);

                        if (nick.find(search) == std::string::npos &&
                            userIdStr.find(search) == std::string::npos &&
                            t.token.find(searchBuffer) == std::string::npos) {
                            continue;
                        }
                    }
                    filtered.push_back(t);
                }

                // Sort
                if (sortMode == 1) {
                    std::sort(filtered.begin(), filtered.end(),
                        [](const auto& a, const auto& b) { return a.money > b.money; });
                } else if (sortMode == 2) {
                    std::sort(filtered.begin(), filtered.end(),
                        [](const auto& a, const auto& b) { return a.userId < b.userId; });
                }

                ImGui::Text("Showing %d / %d tokens", (int)filtered.size(), (int)tokens.size());

                // Display list
                ImGui::BeginChild("tokens", ImVec2(0, 250), true);
                for (size_t i = 0; i < filtered.size(); i++) {
                    auto& t = filtered[i];
                    ImGui::PushID((int)i);
                    ImColor color = t.isJackpot ? ImColor(255, 50, 50) : ImColor(200, 200, 200);
                    ImGui::TextColored(color, "[%d] User: %d | Money: %lld",
                                      (int)i, t.userId, t.money);
                    if (t.isJackpot) {
                        ImGui::SameLine();
                        ImGui::TextColored(ImColor(255, 0, 0), " *** JACKPOT ***");
                    }
                    ImGui::Text("  Nick: %s", t.nickname.c_str());
                    ImGui::Text("  Source: %s", t.source.c_str());
                    std::string tokDisplay = t.token.length() > 50
                        ? (t.token.substr(0, 50) + "...") : t.token;
                    ImGui::Text("  Token: %s", tokDisplay.c_str());

                    if (ImGui::Button("Auto Login"))
                        SlotTokenInterceptor::Instance().TriggerAutoLogin(t);
                    ImGui::SameLine();
                    if (ImGui::Button("Copy Token"))
                        ImGui::SetClipboardText(t.token.c_str());

                    ImGui::Separator();
                    ImGui::PopID();
                }
                ImGui::EndChild();
                ImGui::EndGroupPanel();
                ImGui::EndTabItem();
            }

            // ============================================================
            // TAB: JP Detection
            // ============================================================
            if (ImGui::BeginTabItem("JP Detection")) {
                ImGui::BeginGroupPanel("Jackpot Detection", ImVec2(0, 0));
                ImGui::Text("JP Threshold (money):");
                ImGui::SliderInt("##threshold", &g_jpThreshold, 1000000, 100000000, "%d");
                ImGui::TextColored(ImColor(255, 150, 0),
                    "Current threshold: %d coins", g_jpThreshold);
                ImGui::Checkbox("Auto Login on JP", &g_autoLoginOnJP);
                ImGui::SameLine();
                ImGui::TextColored(g_autoLoginOnJP ? ImColor(0, 255, 0) : ImColor(255, 0, 0),
                    g_autoLoginOnJP ? "ENABLED" : "DISABLED");
                ImGui::Separator();
                ImGui::Text("JP = pemain dengan money > threshold");
                ImGui::Text("Saat JP terdeteksi, token otomatis disimpan");
                ImGui::Text("Tekan 'Auto Login' untuk login ke akun JP");
                ImGui::EndGroupPanel();
                ImGui::EndTabItem();
            }

            // ============================================================
            // TAB: Game Info (FIXED: null-safe)
            // ============================================================
            if (ImGui::BeginTabItem("Game Info")) {
                ImGui::BeginGroupPanel("HallStartup Active", ImVec2(0, 0));
                ImGui::Text("libil2cpp.so base: %p", (void*)m_IL2CPP);
                ImGui::Text("Game version: %s",
                    VersionDetector::VersionString(g_detectedVersion));
                ImGui::Text("Mod ready: %s", g_modReady.load() ? "YES" : "NO");
                ImGui::Text("Hook status: %s", g_hookFailed ? "PARTIAL" : "OK");

                // FIX: Gunakan SafeHallStartup
                HallStartup* hs = SafeHallStartup::Get();
                if (hs) {
                    ImGui::Separator();
                    ImGui::Text("Active HallStartup:");
                    ImGui::Text("  UserID: %d", SafeHallStartup::GetUserId(hs));
                    ImGui::Text("  GameID: %d", hs->iGameID);
                    ImGui::Text("  VipType: %d", hs->iVipType);
                    ImGui::Text("  Money: %lld", SafeHallStartup::GetMoney(hs));
                    ImGui::Text("  HasToken: %s",
                        SafeHallStartup::HasToken(hs) ? "YES" : "NO");

                    std::string nick = SafeHallStartup::GetNickname(hs);
                    ImGui::Text("  Nickname: %s", nick.c_str());

                    if (SafeHallStartup::HasToken(hs)) {
                        std::string token = SafeHallStartup::GetToken(hs);
                        std::string tokDisp = token.length() > 40
                            ? (token.substr(0, 40) + "...") : token;
                        ImGui::Text("  Token: %s", tokDisp.c_str());
                    }
                } else {
                    ImGui::TextColored(ImColor(255, 100, 100),
                        "HallStartup not found (game belum ready)");
                }
                ImGui::EndGroupPanel();
                ImGui::EndTabItem();
            }

            // ============================================================
            // TAB: Network
            // ============================================================
            if (ImGui::BeginTabItem("Network")) {
                ImGui::BeginGroupPanel("Network Status", ImVec2(0, 0));
                ImGui::Text("SocketClient Hook: ");
                ImGui::SameLine();
                ImGui::TextColored(
                    orig_SendRawBytes ? ImColor(0, 255, 0) : ImColor(255, 100, 100),
                    orig_SendRawBytes ? "HOOKED" : "PENDING");
                ImGui::Text("DNS Bypass: ");
                ImGui::SameLine();
                ImGui::TextColored(
                    BypassDNS ? ImColor(0, 255, 0) : ImColor(255, 100, 100),
                    BypassDNS ? "ON" : "OFF");
                ImGui::Separator();
                ImGui::Text("Burp Intercept Bridge: ACTIVE");
                ImGui::Text("Server: i.foqtx.com");
                ImGui::Text("All HTTP traffic visible in Burp Proxy");
                ImGui::EndGroupPanel();
                ImGui::EndTabItem();
            }

            // ============================================================
            // TAB: Settings (NEW)
            // ============================================================
            if (ImGui::BeginTabItem("Settings")) {
                ImGui::BeginGroupPanel("Mod Settings", ImVec2(0, 0));

                ImGui::Checkbox("DNS Bypass", &BypassDNS);
                ImGui::Text("File Logging: %s",
                    FileLogger::Instance().IsEnabled() ? "ON" : "OFF");

                ImGui::Separator();
                ImGui::TextColored(ImColor(150, 150, 150), "HGI SLOT MOD v3.0");
                ImGui::TextColored(ImColor(150, 150, 150),
                    "Detected: %s", VersionDetector::VersionString(g_detectedVersion));
                ImGui::TextColored(ImColor(150, 150, 150),
                    "Tokens in memory: %zu", SlotTokenInterceptor::Instance().TokenCount());

                ImGui::EndGroupPanel();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

// ============================================================
//  eglSwapBuffers Hook - Render ImGui (FIXED: thread-safe dims)
// ============================================================
EGLBoolean (*orig_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);

EGLBoolean _eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    EGLint w = 0, h = 0;
    eglQuerySurface(dpy, surface, EGL_WIDTH, &w);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &h);

    if (w <= 0 || h <= 0) {
        return orig_eglSwapBuffers(dpy, surface);
    }

    // FIX: Update both static vars AND thread-safe singleton
    glWidth = w;
    glHeight = h;
    ScreenDimension::Instance().Update(w, h);

    if (!g_Initialized) {
        LOGI("Initializing ImGui with size %dx%d", w, h);
        ImGui::CreateContext();
        ImGuiIO* io = &ImGui::GetIO();
        io->IniFilename = nullptr;

        ImGui_ImplAndroid_Init();
        ImGui_ImplOpenGL3_Init("#version 300 es");

        ImFontConfig font_cfg;
        io->Fonts->AddFontFromMemoryCompressedTTF(
            GoogleSans_compressed_data, GoogleSans_compressed_size,
            27.0f, &font_cfg);

        g_Initialized = true;
        LOGI("ImGui initialized successfully");
        FLOG_I("ImGui initialized %dx%d", w, h);
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame(glWidth, glHeight);
    ImGui::NewFrame();

    RenderSlotMenu();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    return orig_eglSwapBuffers(dpy, surface);
}

// ============================================================
//  Main Thread - Init mod (FIXED: version detect + proper lifecycle)
// ============================================================
void* main_thread(void*) {
    LOGI("HGI SLOT MOD main_thread started (v3.0)");
    FLOG_I("Main thread started v3.0");

    // ============================================================
    //  STEP 1: Detect which library exists (V2.85 vs V2.86)
    //  V2.85: libil2cpp.so (48MB) + libunity.so (16MB) = separate
    //  V2.86: libgame.so  (80MB) = combined (il2cpp + unity)
    // ============================================================
    LOGI("Waiting for game library to load...");
    while (!m_IL2CPP) {
        // VersionDetector coba libgame.so dulu, lalu libil2cpp.so
        m_IL2CPP = VersionDetector::DetectAndSetLibrary();
        if (!m_IL2CPP) sleep(1);
    }
    LOGI("Game library '%s' found at %p", g_IL2CPPLibName, (void*)m_IL2CPP);
    FLOG_I("Library '%s' at %p", g_IL2CPPLibName, (void*)m_IL2CPP);

    // Load libraries
    while (!m_ANDROID) {
        m_ANDROID = dlopen_ex("libandroid.so", RTLD_NOW);
        sleep(1);
    }
    LOGI("libandroid.so loaded");

    while (!m_INPUT) {
        m_INPUT = dlopen_ex("libinput.so", RTLD_NOW);
        sleep(1);
    }
    LOGI("libinput.so loaded");

    while (!m_EGL) {
        m_EGL = dlopen_ex("libEGL.so", RTLD_NOW);
        sleep(1);
    }
    LOGI("libEGL.so loaded");

    // Attach IL2CPP
    Il2CppAttach(m_IL2CPPLIB);
    LOGI("IL2CPP attached, waiting 5s for stabilization...");
    FLOG_I("IL2CPP attached, stabilizing...");
    sleep(5);

    // NEW: Detect game version
    g_detectedVersion = VersionDetector::DetectVersion();
    LOGI("Detected game version: %s",
         VersionDetector::VersionString(g_detectedVersion));
    FLOG_I("Game version: %s", VersionDetector::VersionString(g_detectedVersion));

    // NEW: Load correct offsets
    if (!VersionDetector::LoadOffsets(g_detectedVersion)) {
        LOGE("Failed to load offsets for detected version, using V2.85 defaults");
        FLOG_E("Offset load failed, using V2.85 defaults");
        // V2.85 is already loaded as default, continue
    }

    // Hook input dengan validasi null
    void* inputAddr = dlsym_ex(m_INPUT,
        "_ZN7android11MotionEvent8copyFromEPKS0_b");
    if (inputAddr) {
        Tools::Hook(inputAddr, (void*)myInput, (void**)&origInput);
        LOGI("Input hook installed at %p", inputAddr);
    } else {
        LOGE("WARNING: Input symbol not found, skipping input hook");
        FLOG_W("Input hook skipped - symbol not found");
    }

    // Hook screen dengan validasi null
    void* getWidthAddr = dlsym_ex(m_ANDROID, "ANativeWindow_getWidth");
    void* getHeightAddr = dlsym_ex(m_ANDROID, "ANativeWindow_getHeight");
    if (getWidthAddr && getHeightAddr) {
        Tools::Hook(getWidthAddr, (void*)_ANativeWindow_getWidth,
                   (void**)&orig_ANativeWindow_getWidth);
        Tools::Hook(getHeightAddr, (void*)_ANativeWindow_getHeight,
                   (void**)&orig_ANativeWindow_getHeight);
        LOGI("ANativeWindow hooks installed");
    } else {
        LOGE("WARNING: ANativeWindow symbols not found");
        FLOG_W("ANativeWindow hooks skipped");
    }

    // Hook eglSwapBuffers dengan fallback
    void* eglAddr = dlsym_ex(m_EGL, "eglSwapBuffers");
    if (!eglAddr) {
        LOGI("eglSwapBuffers NOT in libEGL.so, trying libGLESv2.so...");
        void* gles2 = dlopen_ex("libGLESv2.so", RTLD_NOW);
        if (gles2) eglAddr = dlsym_ex(gles2, "eglSwapBuffers");
    }
    if (!eglAddr) {
        LOGI("Trying libGLESv3.so...");
        void* gles3 = dlopen_ex("libGLESv3.so", RTLD_NOW);
        if (gles3) eglAddr = dlsym_ex(gles3, "eglSwapBuffers");
    }

    if (eglAddr) {
        Tools::Hook(eglAddr, (void*)_eglSwapBuffers, (void**)&orig_eglSwapBuffers);
        LOGI("eglSwapBuffers hooked at %p", eglAddr);
        FLOG_I("eglSwapBuffers hooked at %p", eglAddr);
    } else {
        LOGE("FATAL: Cannot find eglSwapBuffers! Menu will NOT appear.");
        FLOG_E("FATAL: eglSwapBuffers not found");
    }

    // Install HGI SLOT hooks (FIXED: check return value)
    if (!InstallHooks()) {
        g_hookFailed = true;
        LOGE("Running in LIMITED MODE (some hooks failed)");
        FLOG_E("Running in LIMITED MODE");
    }

    g_modReady.store(true);

    // FIX: Load saved tokens dari disk
    SlotTokenInterceptor::Instance().LoadFromDisk();

    // FIX: Start scanner thread dengan proper handle (bukan detach!)
    g_scannerThread = std::make_unique<std::thread>(HallStartupScannerThread);

    LOGI("=== HGI SLOT MOD v3.0 READY ===");
    FLOG_I("=== MOD READY ===");

    // Wait for scanner thread to finish (blocked until mod unload)
    if (g_scannerThread && g_scannerThread->joinable()) {
        g_scannerThread->join();
    }

    return nullptr;
}

// ============================================================
//  Entry Point
// ============================================================
__attribute__((constructor))
void lib_main() {
    pthread_t ptid;
    pthread_create(&ptid, nullptr, main_thread, nullptr);
}

// ============================================================
//  JNI_OnLoad / JNI_OnUnload (FIXED: proper cleanup)
// ============================================================
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    jvm = vm;
    LOGI("JNI_OnLoad HGI SLOT MOD v3.0");
    return JNI_VERSION_1_6;
}

// FIX: Proper cleanup saat library unload
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    LOGI("JNI_OnUnload: cleaning up...");
    FLOG_I("Mod unloading...");

    // Signal scanner thread to stop
    g_modReady.store(false);

    // Save tokens sebelum exit
    SlotTokenInterceptor::Instance().SaveToDisk();

    // Wait for scanner thread
    if (g_scannerThread && g_scannerThread->joinable()) {
        g_scannerThread->join();
    }
    g_scannerThread.reset();

    LOGI("JNI_OnUnload: cleanup complete");
    FLOG_I("Mod unloaded cleanly");
}
