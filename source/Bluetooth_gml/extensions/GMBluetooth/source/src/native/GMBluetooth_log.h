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

        // extgen's core logger: logcat on Android, stderr everywhere else.
        ::gm::log::Write(::gm::log::Level::Info, "GMBluetooth", func, "%s", msg);

#if defined(__APPLE__)
        // On Apple the core logger only reaches stderr, which Xcode shows but
        // the unified log does not capture - so a device run started outside
        // Xcode (GameMaker's own device log, Console.app, a sysdiagnose) would
        // see nothing without this. %{public}s keeps the text out of the
        // <private> redaction that os_log applies to dynamic strings.
        os_log(OS_LOG_DEFAULT, "[GMBluetooth] %{public}s :: %{public}s", func, msg);
#endif
    }
} // namespace gmbluetooth::log

// Call sites are on user-driven paths only. Anything reached from Step/Draw
// every frame must log on state change instead, or it drowns the log at 60 Hz.
#define GMBT_LOG(fmt, ...) ::gmbluetooth::log::write(__func__, fmt, ##__VA_ARGS__)
