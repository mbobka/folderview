#pragma once

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Диагностическая трасса FolderView без CRT.
 *
 * Включается переменной окружения FOLDERVIEW_TRACE=<путь к файлу>: строки
 * дописываются в файл в UTF-8, каждая начинается с отметки времени QPC в
 * миллисекундах от FvTrace_Initialize. У GUI-приложения нет консоли, поэтому
 * вывод идёт только в файл. Без переменной все вызовы сводятся к проверке
 * g_fv_trace_enabled: таймер не опрашивается, строки не форматируются.
 */

extern BOOL g_fv_trace_enabled;

#define FV_TRACE_ENABLED() (g_fv_trace_enabled)
/* Отметка QPC только при включённой трассе; иначе 0 без системного вызова. */
#define FV_TRACE_NOW() (g_fv_trace_enabled ? FvTrace_Now() : 0ULL)

void FvTrace_Initialize(void);
void FvTrace_Shutdown(void);
ULONGLONG FvTrace_Now(void);

/* Интервал QPC в микросекундах. */
ULONGLONG FvTrace_TicksToMicroseconds(ULONGLONG ticks);

/*
 * Формат — как у wvsprintfW (результат не длиннее 1024 символов, лишнее
 * обрезается). Перед строкой пишется "t=<мс> ", после — перевод строки.
 */
void FvTrace_Printf(const WCHAR* format, ...);

/* Последние max_chars символов строки: длинные пути обрезаются слева. */
const WCHAR* FvTrace_Tail(const WCHAR* text, int max_chars);

#ifdef __cplusplus
}
#endif
