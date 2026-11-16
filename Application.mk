APP_ABI := arm64-v8a armeabi-v7a
APP_STL := c++_static
APP_PLATFORM := android-24

# C++ Flags
APP_CPPFLAGS := -std=c++17 -fexceptions -frtti -fpermissive
APP_CPPFLAGS += -Wno-error=format-security
APP_CPPFLAGS += -Wno-error=c++11-narrowing
APP_CPPFLAGS += -fms-extensions

# C Flags - REMOVED -w to keep important warnings visible
APP_CFLAGS := -Wno-error
APP_CFLAGS += -Wno-error=format-security
APP_CFLAGS += -fpermissive

# Strip mode: use --strip-debug during development for easier troubleshooting
# Change to --strip-all only for final release
APP_STRIP_MODE := --strip-debug