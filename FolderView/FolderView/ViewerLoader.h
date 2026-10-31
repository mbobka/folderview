#pragma once

#include <windows.h>

#include "ImageDecoder.h"
#include "ZipArchive.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Фоновая загрузка кадров вьювера: один рабочий поток читает запись
 * ZIP (или файл папки) и декодирует её через WIC в собственном COM-контексте
 * и со своей WIC-фабрикой. Владение:
 * - задание несёт копии всего нужного (индекс, поколение списка, рамку,
 *   копию ZipEntry, ссылку на FvLoaderArchive или копию пути) и не трогает
 *   g_files/g_cache UI-потока;
 * - готовые задания складываются в список завершённых, UI-поток получает
 *   уведомление (PostMessage без указателей) и забирает их сам —
 *   уведомление можно потерять без утечки, а результаты, не забранные до
 *   остановки, освобождает FvLoader_Stop;
 * - D2D-битмапы, кэш кадров и вытеснение остаются в UI-потоке.
 */

/* Предел размера записи-изображения (как у синхронного пути вьювера). */
#define FV_LOADER_MAX_ZIP_IMAGE_BYTES ((ULONGLONG)1024 * 1024 * 1024)
/* Предел очереди: текущий кадр, передекод и окно prefetch. */
#define FV_LOADER_MAX_QUEUE 24

/*
 * Открытый ZIP для рабочего потока: дубликат хэндла g_zip со счётчиком
 * ссылок. Закрытие или замена g_zip в UI-потоке его не затрагивает; хэндл
 * закрывается, когда отпущена последняя ссылка (UI или задание).
 */
typedef struct FvLoaderArchive FvLoaderArchive;

FvLoaderArchive* FvLoaderArchive_Create(const ZipArchive* archive);
void FvLoaderArchive_Release(FvLoaderArchive* archive);

/* Запрос задания: всё копируется в задание при постановке в очередь. */
typedef struct FvLoadRequest {
    SIZE_T index;
    ULONGLONG generation;
    /* FV_PLAN_REASON_* (ViewerPlan.h). */
    int reason;
    UINT box_width;
    UINT box_height;
    UINT max_dimension;
    /* ZIP: архив (задание берёт свою ссылку) и запись; name не копируется. */
    FvLoaderArchive* archive;
    const ZipEntry* entry;
    /* Папка: путь к файлу (копируется). */
    const WCHAR* path;
} FvLoadRequest;

typedef struct FvLoadJob {
    struct FvLoadJob* next;
    SIZE_T index;
    ULONGLONG generation;
    int reason;
    UINT box_width;
    UINT box_height;
    UINT max_dimension;
    FvLoaderArchive* archive;
    ZipEntry entry;
    WCHAR* path;
    /* Отметки QPC (только при включённой трассе). */
    ULONGLONG queued_at;
    ULONGLONG started_at;
    ULONGLONG finished_at;
    /* Результат: заполняется рабочим потоком, читается UI после FvLoader_TakeCompleted. */
    HRESULT result;
    BOOL cancelled;
    DecodedImage image;
} FvLoadJob;

/* Запускает рабочий поток; уведомления — message окну window. */
BOOL FvLoader_Start(HWND window, UINT message);

/*
 * Останавливает поток: отменяет очередь, прерывает синхронный ReadFile
 * (CancelSynchronousIo) и ждёт выхода; освобождает все незабранные задания.
 * TerminateThread не используется: если поток не вышел за отведённое время
 * (зависшее сетевое чтение), он остаётся до ExitProcess, а его задание —
 * за ним.
 */
void FvLoader_Stop(void);

/*
 * Заменяет очередь списком requests в порядке приоритета. Задание с тем же
 * ключом (FvPlan_JobKeyEqual), уже стоящее в очереди, переиспользуется (и
 * повышается в приоритете), выполняющееся — не дублируется. Задания, которых
 * нет в новом списке, отменяются: из очереди — сразу, выполняющееся — в
 * точке отмены между чтением и декодированием.
 */
void FvLoader_Replace(const FvLoadRequest* requests, SIZE_T count);

/* Забирает завершённые задания (список через next) в порядке завершения. */
FvLoadJob* FvLoader_TakeCompleted(void);

/* Освобождает задание (изображение, если не забрано, ссылку на архив, путь). */
void FvLoader_FreeJob(FvLoadJob* job);

/* Имя причины для трассы: current|upgrade|prefetch. */
const WCHAR* FvLoader_ReasonName(int reason);

#ifdef __cplusplus
}
#endif
