#pragma once
#include <cstdio>
#include <cstdarg>
#include <ctime>

// ─────────────────────────────────────────────────────────────────────────────
// Logger — writes timestamped entries to nanodb_execution.log
// and mirrors them to stdout.
// ─────────────────────────────────────────────────────────────────────────────
class Logger {
    static FILE* logFile;
    static bool  echoStdout;

public:
    static void init(const char* path = "logs/nanodb_execution.log", bool echo = true) {
        if (logFile) fclose(logFile);
        logFile    = fopen(path, "w");
        echoStdout = echo;
        if (!logFile) {
            fprintf(stderr, "WARNING: Could not open log file %s\n", path);
        }
    }

    static void log(const char* fmt, ...) {
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        // Timestamp
        time_t now = time(nullptr);
        char ts[32];
        strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&now));

        if (logFile)    fprintf(logFile,  "[%s] %s\n", ts, buf);
        if (echoStdout) printf(           "[LOG] %s\n", buf);

        if (logFile) fflush(logFile);
    }

    static void close() {
        if (logFile) { fclose(logFile); logFile = nullptr; }
    }
};

// Definition in Logger.cpp — we use inline statics here for header-only usage
FILE* Logger::logFile    = nullptr;
bool  Logger::echoStdout = true;
