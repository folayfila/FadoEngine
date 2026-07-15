// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef FADO_LOG_H
#define FADO_LOG_H

#include "fado_types.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

// ────────────────────────────────────────────────────────────────────────

/*
* Fado Logger
  - A simple logger that prints into a .txt.
  - The game and engine have seperate files, since the game is a dll. Hopefully we won't need both simultaneously :)
  - To use the log simply include tis header and use FLOG().
*/

// ────────────────────────────────────────────────────────────────────────

// Log levels
#define FLOG_INFO    0
#define FLOG_WARNING 1
#define FLOG_ERROR   2

// The one and only logging function, calls FLogInternal()
// // Works like a printf().
// Enabled only in debug builds.
#if FADO_DEBUG
#define FLOG(level, fmt, ...) FLogInternal(level, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define FLOG(level, fmt, ...)
#endif
// ──────────────────────────────────────────

global_variable FILE* GlobalLogFile;

// ──────────────────────────────────────────
inline void FLogInternal(u32 level, cc8* file, i32 line, cc8* fmt, ...)
{
    if (!GlobalLogFile)
    {
#ifdef GAME_DLL
        GlobalLogFile = fopen("GameLog.txt", "w");
#else
        GlobalLogFile = fopen("EditorLog.txt", "w");
#endif
    }

    // Timestamp
    tm tmInfo{};
    time_t t = time(nullptr);
#ifdef _WIN32
    localtime_s(&tmInfo, &t);
#else
    localtime_r(&t, &tmInfo);
#endif

    c8 timeBuffer[16];
    strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &tmInfo);

    // Level string
    cc8* levelStr = nullptr;
    switch (level)
    {
    case FLOG_INFO:    levelStr = "INFO";    break;
    case FLOG_WARNING: levelStr = "WARNING"; break;
    case FLOG_ERROR:   levelStr = "ERROR";   break;
    default:           levelStr = "UNKNOWN"; break;
    }

    // Format the user message
    c8 msgBuffer[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msgBuffer, sizeof(msgBuffer), fmt, args);
    va_end(args);

    // Final output: [HH:MM:SS] [LEVEL] message (file:line)
    c8 outBuffer[2048];
    snprintf(outBuffer, sizeof(outBuffer), "[%s] [%s] %s (%s:%d)\n",
        timeBuffer, levelStr, msgBuffer, file, line);

    // Write to file
    if (GlobalLogFile)
    {
        fprintf(GlobalLogFile, "%s", outBuffer);
        fflush(GlobalLogFile);
    }
}

// ────────────────────────────────────────────────────────────────────────

#endif	// FADO_LOG_H