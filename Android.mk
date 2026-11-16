LOCAL_PATH := $(call my-dir)
MAIN_LOCAL_PATH := $(call my-dir)

# ============================================================
#  Dobby (hooking framework) - PREBUILT STATIC
# ============================================================
include $(CLEAR_VARS)
LOCAL_MODULE := libdobby
LOCAL_SRC_FILES := dobby/libraries/$(TARGET_ARCH_ABI)/libdobby.a
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/dobby/
include $(PREBUILT_STATIC_LIBRARY)

# ============================================================
#  CATATAN: libcurl / libssl / libcrypto SENGAJA TIDAK di-link.
#  Dependency itu hanya dipakai Tools/Jni_Stuff.h (fitur lisensi
#  "vipkey" warisan source MLBB) yang TIDAK dipakai mod HGI.
#  Jika di-link, libcrypto.a (OpenSSL 1.1 ARM32) butuh
#  __sync_add_and_fetch_4 -> linker error tanpa libatomic.
#  File prebuilt-nya tetap ada di folder Tools/curl/ kalau
#  suatu hari dibutuhkan.
# ============================================================

# ============================================================
#  libXnish.so - MOD UTAMA
# ============================================================
include $(CLEAR_VARS)
LOCAL_MODULE    := Xnish

# -DIMGUI_IMPL_OPENGL_ES3 : pakai <GLES3/gl3.h>, SKIP
#   imgui_impl_opengl3_loader.h (file itu tidak ada di source ini)
# -DGL_GLEXT_PROTOTYPES   : prototype glBindVertexArray dll di GLES3
LOCAL_CFLAGS := -Wno-error=format-security -fvisibility=hidden \
                -ffunction-sections -fdata-sections -w -fpermissive \
                -DIMGUI_IMPL_OPENGL_ES3 -DGL_GLEXT_PROTOTYPES
LOCAL_CPPFLAGS := -Wno-error=format-security -fvisibility=hidden \
                  -ffunction-sections -fdata-sections -w -std=c++17 \
                  -fexceptions -frtti -fpermissive \
                  -Wno-error=c++11-narrowing -fms-extensions \
                  -DIMGUI_IMPL_OPENGL_ES3 -DGL_GLEXT_PROTOTYPES
LOCAL_LDFLAGS += -Wl,--gc-sections,--strip-all
LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES := imgui/imgui.cpp \
imgui/imgui_demo.cpp \
imgui/imgui_draw.cpp \
imgui/imgui_tables.cpp \
imgui/imgui_widgets.cpp \
imgui/backends/imgui_impl_android.cpp \
imgui/backends/imgui_impl_opengl3.cpp \
struct/MonoString.cpp \
unity/fake_dlfcn.cpp \
unity/Il2Cpp.cpp \
unity/Tools.cpp \
Main.cpp

# PENTING: pakai += (bukan :=) agar tidak menimpa include path
LOCAL_C_INCLUDES += $(MAIN_LOCAL_PATH)
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/imgui
LOCAL_C_INCLUDES += $(LOCAL_PATH)/imgui/backends
LOCAL_C_INCLUDES += $(LOCAL_PATH)/Tools/curl/curl-android-$(TARGET_ARCH_ABI)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/Tools/curl/openssl-android-$(TARGET_ARCH_ABI)/include

LOCAL_CPP_FEATURES       := exceptions
LOCAL_LDLIBS             := -llog -landroid -lEGL -lGLESv2 -lGLESv3 -lGLESv1_CM -lz
LOCAL_STATIC_LIBRARIES   := libdobby

include $(BUILD_SHARED_LIBRARY)
