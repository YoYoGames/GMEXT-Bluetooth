#pragma once

// Diagnostic logging for the GMBluetooth implementation.
//
// Lives in src/ rather than code_gen/ so extgen regeneration cannot clobber it.

#include <cstdarg>
#include <cstdio>

#include "GMExtUtils.h"

#if defined(__APPLE__)
#include <os/log.h>
#endif

namespace gmbluetooth::log
{
    // Formats once, then fans out to every sink the platform's tooling reads.
    inline void write(const char* func, const char* fmt, ...)
    {
        char msg[1024];

        va_list args;
        va_start(args, fmt);
        std::vsnprintf(msg, sizeof(msg), fmt, args);
        va_end(args);

#if defined(__APPLE__)
        // os_log ONLY on Apple. The core logger's stderr write would be a second
        // copy of the same line in the Xcode console, and os_log is the strictly
        // more useful of the two: it is also what GameMaker's own device log,
        // Console.app and a sysdiagnose capture. %{public}s keeps the text out
        // of the <private> redaction os_log applies to dynamic strings.
        os_log(OS_LOG_DEFAULT, "[GMBluetooth] %{public}s :: %{public}s", func, msg);
#else
        // extgen's core logger: logcat on Android, stderr elsewhere.
        ::gm::log::Write(::gm::log::Level::Info, "GMBluetooth", func, "%s", msg);
#endif
    }
} // namespace gmbluetooth::log

// Call sites are on user-driven paths only. Anything reached from Step/Draw
// every frame must log on state change instead, or it drowns the log at 60 Hz.
#define GMBT_LOG(fmt, ...) ::gmbluetooth::log::write(__func__, fmt, ##__VA_ARGS__)
