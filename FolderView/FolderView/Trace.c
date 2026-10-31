#include "Trace.h"

#include <stdarg.h>

#define FV_TRACE_LINE_CHARS 1024
#define FV_TRACE_PREFIX_CHARS 48
/* UTF-8 занимает не больше 3 байт на UTF-16 code unit. */
#define FV_TRACE_UTF8_BYTES ((FV_TRACE_PREFIX_CHARS + FV_TRACE_LINE_CHARS + 2) * 3)

BOOL g_fv_trace_enabled;

static HANDLE g_fv_trace_file = INVALID_HANDLE_VALUE;
static LARGE_INTEGER g_fv_trace_frequency;
static LARGE_INTEGER g_fv_trace_start;

void FvTrace_Initialize(void) {
    WCHAR path[MAX_PATH * 4];
    DWORD length;
    HANDLE file;

    if (g_fv_trace_enabled) {
        return;
    }

    length = GetEnvironmentVariableW(
        L"FOLDERVIEW_TRACE",
        path,
        (DWORD)(sizeof(path) / sizeof(path[0])));
    if (length == 0 || length >= (DWORD)(sizeof(path) / sizeof(path[0]))) {
        return;
    }
    if (!QueryPerformanceFrequency(&g_fv_trace_frequency) ||
        g_fv_trace_frequency.QuadPart <= 0) {
        return;
    }

    /* Дописываем: один файл может собрать несколько запусков подряд. */
    file = CreateFileW(
        path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    g_fv_trace_file = file;
    QueryPerformanceCounter(&g_fv_trace_start);
    g_fv_trace_enabled = TRUE;
}

static SRWLOCK g_fv_trace_lock = SRWLOCK_INIT;

void FvTrace_Shutdown(void) {
    /* Под блокировкой: рабочий поток мог не завершиться (выход по таймауту). */
    AcquireSRWLockExclusive(&g_fv_trace_lock);
    if (g_fv_trace_file != INVALID_HANDLE_VALUE) {
        CloseHandle(g_fv_trace_file);
        g_fv_trace_file = INVALID_HANDLE_VALUE;
    }
    g_fv_trace_enabled = FALSE;
    ReleaseSRWLockExclusive(&g_fv_trace_lock);
}

ULONGLONG FvTrace_Now(void) {
    LARGE_INTEGER now;

    QueryPerformanceCounter(&now);
    return (ULONGLONG)now.QuadPart;
}

ULONGLONG FvTrace_TicksToMicroseconds(ULONGLONG ticks) {
    ULONGLONG frequency = (ULONGLONG)g_fv_trace_frequency.QuadPart;

    if (frequency == 0) {
        return 0;
    }
    /* Разбиение на целую и дробную части исключает переполнение ticks * 10^6. */
    return (ticks / frequency) * 1000000ULL +
        (ticks % frequency) * 1000000ULL / frequency;
}

const WCHAR* FvTrace_Tail(const WCHAR* text, int max_chars) {
    int length;

    if (text == NULL) {
        return L"";
    }
    length = lstrlenW(text);
    if (max_chars > 0 && length > max_chars) {
        return text + (length - max_chars);
    }
    return text;
}

/*
 * Буферы статические: кадр стека больше 4 КиБ потребовал бы __chkstk из CRT.
 * Трассу пишут UI-поток и рабочий поток декода, поэтому форматирование
 * и запись строки идут под SRW-блокировкой (статическая инициализация).
 */
static WCHAR g_fv_trace_line[FV_TRACE_PREFIX_CHARS + FV_TRACE_LINE_CHARS + 2];
static char g_fv_trace_utf8[FV_TRACE_UTF8_BYTES];

void FvTrace_Printf(const WCHAR* format, ...) {
    WCHAR* line = g_fv_trace_line;
    char* utf8 = g_fv_trace_utf8;
    ULONGLONG elapsed_us;
    int prefix;
    int body;
    int bytes;
    DWORD written;
    va_list arguments;

    if (!g_fv_trace_enabled || g_fv_trace_file == INVALID_HANDLE_VALUE) {
        return;
    }

    AcquireSRWLockExclusive(&g_fv_trace_lock);
    if (g_fv_trace_file == INVALID_HANDLE_VALUE) {
        ReleaseSRWLockExclusive(&g_fv_trace_lock);
        return;
    }
    elapsed_us = FvTrace_TicksToMicroseconds(
        FvTrace_Now() - (ULONGLONG)g_fv_trace_start.QuadPart);
    prefix = wsprintfW(
        line,
        L"t=%I64u.%03I64u ",
        elapsed_us / 1000ULL,
        elapsed_us % 1000ULL);
    if (prefix < 0) {
        ReleaseSRWLockExclusive(&g_fv_trace_lock);
        return;
    }

    va_start(arguments, format);
    body = wvsprintfW(line + prefix, format, arguments);
    va_end(arguments);
    if (body < 0) {
        ReleaseSRWLockExclusive(&g_fv_trace_lock);
        return;
    }
    line[prefix + body] = L'\n';

    bytes = WideCharToMultiByte(
        CP_UTF8,
        0,
        line,
        prefix + body + 1,
        utf8,
        (int)sizeof(g_fv_trace_utf8),
        NULL,
        NULL);
    if (bytes > 0) {
        WriteFile(g_fv_trace_file, utf8, (DWORD)bytes, &written, NULL);
    }
    ReleaseSRWLockExclusive(&g_fv_trace_lock);
}
