#include "ViewerLoader.h"

#include "Trace.h"
#include "ViewerPlan.h"

#include <objbase.h>

/* Время ожидания выхода рабочего потока при остановке. */
#define FV_LOADER_STOP_TIMEOUT_MS 10000U
/* Период повторного CancelSynchronousIo, пока поток не вышел. */
#define FV_LOADER_STOP_POLL_MS 50U

#define FV_LOADER_MS L"%I64u.%03I64u"
#define FV_LOADER_MS_ARGS(us) (ULONGLONG)(us) / 1000ULL, (ULONGLONG)(us) % 1000ULL

struct FvLoaderArchive {
    volatile LONG references;
    HANDLE file;
    ULONGLONG file_size;
};

/*
 * Состояние загрузчика. Поля под lock: очередь, выполняющееся задание и флаг
 * его отмены, список завершённых, признак отправленного уведомления, stopping.
 * Задание в очереди принадлежит загрузчику, выполняющееся — рабочему потоку,
 * завершённое — снова загрузчику до FvLoader_TakeCompleted.
 */
typedef struct FvLoaderState {
    SRWLOCK lock;
    CONDITION_VARIABLE wake;
    HANDLE thread;
    HWND window;
    UINT message;
    BOOL stopping;
    FvLoadJob* queue[FV_LOADER_MAX_QUEUE];
    SIZE_T queue_count;
    FvLoadJob* running;
    BOOL running_cancelled;
    FvLoadJob* completed_head;
    FvLoadJob* completed_tail;
    BOOL notify_posted;
} FvLoaderState;

static FvLoaderState g_loader = {SRWLOCK_INIT, CONDITION_VARIABLE_INIT};

static void* fv_loader_alloc(SIZE_T size) {
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size != 0 ? size : 1);
}

static void fv_loader_free(void* memory) {
    if (memory != NULL) {
        HeapFree(GetProcessHeap(), 0, memory);
    }
}

static ULONGLONG fv_loader_us_between(ULONGLONG from, ULONGLONG to) {
    return from != 0 && to >= from ? FvTrace_TicksToMicroseconds(to - from) : 0;
}

const WCHAR* FvLoader_ReasonName(int reason) {
    switch (reason) {
    case FV_PLAN_REASON_CURRENT:
        return L"current";
    case FV_PLAN_REASON_UPGRADE:
        return L"upgrade";
    default:
        return L"prefetch";
    }
}

FvLoaderArchive* FvLoaderArchive_Create(const ZipArchive* archive) {
    FvLoaderArchive* shared;

    if (!ZipArchive_IsOpen(archive)) {
        return NULL;
    }
    shared = (FvLoaderArchive*)fv_loader_alloc(sizeof(*shared));
    if (shared == NULL) {
        return NULL;
    }
    if (!ZipArchive_DuplicateFile(archive, &shared->file)) {
        fv_loader_free(shared);
        return NULL;
    }
    shared->file_size = archive->file_size;
    shared->references = 1;
    return shared;
}

static void fv_loader_archive_acquire(FvLoaderArchive* archive) {
    if (archive != NULL) {
        InterlockedIncrement(&archive->references);
    }
}

void FvLoaderArchive_Release(FvLoaderArchive* archive) {
    if (archive == NULL) {
        return;
    }
    if (InterlockedDecrement(&archive->references) == 0) {
        CloseHandle(archive->file);
        fv_loader_free(archive);
    }
}

void FvLoader_FreeJob(FvLoadJob* job) {
    if (job == NULL) {
        return;
    }
    DecodedImage_Free(&job->image);
    FvLoaderArchive_Release(job->archive);
    fv_loader_free(job->path);
    fv_loader_free(job);
}

static FvPlanJobKey fv_loader_job_key(const FvLoadJob* job) {
    FvPlanJobKey key;

    key.generation = job->generation;
    key.index = job->index;
    key.box_width = job->box_width;
    key.box_height = job->box_height;
    key.max_dimension = job->max_dimension;
    key.reason = job->reason;
    return key;
}

static FvPlanJobKey fv_loader_request_key(const FvLoadRequest* request) {
    FvPlanJobKey key;

    key.generation = request->generation;
    key.index = request->index;
    key.box_width = request->box_width;
    key.box_height = request->box_height;
    key.max_dimension = request->max_dimension;
    key.reason = request->reason;
    return key;
}

static WCHAR* fv_loader_duplicate(const WCHAR* text) {
    SIZE_T length;
    WCHAR* copy;

    if (text == NULL) {
        return NULL;
    }
    length = (SIZE_T)lstrlenW(text);
    copy = (WCHAR*)fv_loader_alloc((length + 1) * sizeof(WCHAR));
    if (copy != NULL) {
        CopyMemory(copy, text, (length + 1) * sizeof(WCHAR));
    }
    return copy;
}

static FvLoadJob* fv_loader_create_job(const FvLoadRequest* request) {
    FvLoadJob* job;

    if ((request->archive == NULL || request->entry == NULL) && request->path == NULL) {
        return NULL;
    }
    job = (FvLoadJob*)fv_loader_alloc(sizeof(*job));
    if (job == NULL) {
        return NULL;
    }
    job->index = request->index;
    job->generation = request->generation;
    job->reason = request->reason;
    job->box_width = request->box_width;
    job->box_height = request->box_height;
    job->max_dimension = request->max_dimension;
    if (request->archive != NULL && request->entry != NULL) {
        job->archive = request->archive;
        fv_loader_archive_acquire(job->archive);
        job->entry = *request->entry;
        /* Имя принадлежит архиву UI-потока: заданию оно не нужно. */
        job->entry.name = NULL;
    } else {
        job->path = fv_loader_duplicate(request->path);
        if (job->path == NULL) {
            fv_loader_free(job);
            return NULL;
        }
    }
    job->queued_at = FV_TRACE_NOW();
    job->result = E_PENDING;
    return job;
}

/* Точка отмены выполняющегося задания (между чтением и декодированием). */
static BOOL fv_loader_running_cancelled(void) {
    BOOL cancelled;

    AcquireSRWLockShared(&g_loader.lock);
    cancelled = g_loader.running_cancelled || g_loader.stopping;
    ReleaseSRWLockShared(&g_loader.lock);
    return cancelled;
}

static void fv_loader_trace_decode(
    const FvLoadJob* job,
    HRESULT result,
    const ImageDecodeStats* stats,
    ULONGLONG started) {
    ULONGLONG elapsed_us = fv_loader_us_between(started, FvTrace_Now());
    ULONGLONG open_us = FvTrace_TicksToMicroseconds(stats->open_ticks);
    ULONGLONG pixels_us = FvTrace_TicksToMicroseconds(stats->pixels_ticks);

    FvTrace_Printf(
        L"decode index=%I64u ms=" FV_LOADER_MS L" hr=0x%08lX reason=%s gen=%I64u width=%u height=%u "
        L"src_width=%u src_height=%u box=%ux%u max=%u dct=1/%u dct_size=%ux%u scaled=%d "
        L"bytes=%I64u open_ms=" FV_LOADER_MS L" pixels_ms=" FV_LOADER_MS L" thread=worker",
        (ULONGLONG)job->index,
        FV_LOADER_MS_ARGS(elapsed_us),
        (DWORD)result,
        FvLoader_ReasonName(job->reason),
        job->generation,
        SUCCEEDED(result) ? job->image.width : 0U,
        SUCCEEDED(result) ? job->image.height : 0U,
        stats->source_width,
        stats->source_height,
        job->box_width,
        job->box_height,
        job->max_dimension,
        stats->dct_denominator,
        stats->transform_width,
        stats->transform_height,
        stats->scaled ? 1 : 0,
        SUCCEEDED(result) ? (ULONGLONG)job->image.pixels_size : 0ULL,
        FV_LOADER_MS_ARGS(open_us),
        FV_LOADER_MS_ARGS(pixels_us));
}

/* Выполняет задание в рабочем потоке: чтение записи, точка отмены, декод. */
static void fv_loader_execute(
    FvLoadJob* job,
    const ImageDecoderContext* context,
    HRESULT context_status,
    ZipReadScratch* scratch) {
    ImageDecodeOptions options;
    ImageDecodeStats stats;
    HRESULT result;
    ULONGLONG started;

    ZeroMemory(&options, sizeof(options));
    ZeroMemory(&stats, sizeof(stats));
    options.box_width = job->box_width;
    options.box_height = job->box_height;
    options.max_dimension = job->max_dimension;
    options.interpolation = IMAGE_DECODER_DEFAULT_INTERPOLATION;
    options.dct_slack_percent = IMAGE_DECODER_DEFAULT_DCT_SLACK_PERCENT;

    if (FAILED(context_status)) {
        job->result = context_status;
        return;
    }

    if (job->archive != NULL) {
        BYTE* data = NULL;
        SIZE_T size = 0;
        ZipIoStats io_before = scratch->io;
        BOOL extracted;
        DWORD error;

        started = FV_TRACE_NOW();
        extracted = ZipArchive_ExtractEntryFromFile(
            job->archive->file,
            job->archive->file_size,
            &job->entry,
            FV_LOADER_MAX_ZIP_IMAGE_BYTES,
            scratch,
            &data,
            &size);
        error = extracted ? ERROR_SUCCESS : GetLastError();
        if (FV_TRACE_ENABLED()) {
            ULONGLONG elapsed_us = fv_loader_us_between(started, FvTrace_Now());

            FvTrace_Printf(
                L"entry-read index=%I64u ms=" FV_LOADER_MS
                L" bytes=%I64u compressed=%I64u method=%u reads=%I64u read_bytes=%I64u error=%lu"
                L" reason=%s gen=%I64u thread=worker",
                (ULONGLONG)job->index,
                FV_LOADER_MS_ARGS(elapsed_us),
                (ULONGLONG)size,
                job->entry.compressed_size,
                (UINT)job->entry.compression_method,
                scratch->io.read_calls - io_before.read_calls,
                scratch->io.read_bytes - io_before.read_bytes,
                error,
                FvLoader_ReasonName(job->reason),
                job->generation);
        }
        if (!extracted) {
            job->result = error != ERROR_SUCCESS ? HRESULT_FROM_WIN32(error) : E_FAIL;
            return;
        }
        /* С кадра ушли, пока он читался: декод (30–60 мс) не нужен. */
        if (fv_loader_running_cancelled()) {
            ZipArchive_FreeBuffer(data);
            job->cancelled = TRUE;
            job->result = E_ABORT;
            return;
        }
        started = FV_TRACE_NOW();
        result = ImageDecoder_DecodeMemoryWith(context, data, size, &options, &job->image, &stats);
        ZipArchive_FreeBuffer(data);
    } else {
        /* Файл папки WIC читает сам по ходу декода: точка отмены — до него. */
        if (fv_loader_running_cancelled()) {
            job->cancelled = TRUE;
            job->result = E_ABORT;
            return;
        }
        started = FV_TRACE_NOW();
        result = ImageDecoder_DecodeFileWith(context, job->path, &options, &job->image, &stats);
    }

    if (FV_TRACE_ENABLED()) {
        fv_loader_trace_decode(job, result, &stats, started);
    }
    if (SUCCEEDED(result) && !DecodedImage_IsValid(&job->image)) {
        result = E_FAIL;
    }
    if (FAILED(result)) {
        DecodedImage_Free(&job->image);
    }
    job->result = result;
}

static DWORD WINAPI fv_loader_thread(LPVOID parameter) {
    ImageDecoderContext context;
    ZipReadScratch scratch;
    HRESULT context_status;

    (void)parameter;
    ZeroMemory(&context, sizeof(context));
    /* Свой COM-контекст и своя WIC-фабрика; MTA — у потока нет цикла сообщений. */
    context_status = ImageDecoderContext_Create(&context, COINIT_MULTITHREADED);
    ZipReadScratch_Init(&scratch);
    if (FV_TRACE_ENABLED()) {
        FvTrace_Printf(L"loader-start hr=0x%08lX threads=1", (DWORD)context_status);
    }

    AcquireSRWLockExclusive(&g_loader.lock);
    for (;;) {
        FvLoadJob* job;
        BOOL notify;
        BOOL cancelled;

        while (!g_loader.stopping && g_loader.queue_count == 0) {
            SleepConditionVariableSRW(&g_loader.wake, &g_loader.lock, INFINITE, 0);
        }
        if (g_loader.stopping) {
            break;
        }
        job = g_loader.queue[0];
        MoveMemory(
            g_loader.queue,
            g_loader.queue + 1,
            (g_loader.queue_count - 1) * sizeof(g_loader.queue[0]));
        --g_loader.queue_count;
        g_loader.running = job;
        g_loader.running_cancelled = FALSE;
        ReleaseSRWLockExclusive(&g_loader.lock);

        job->started_at = FV_TRACE_NOW();
        if (FV_TRACE_ENABLED()) {
            FvTrace_Printf(
                L"job-start index=%I64u reason=%s gen=%I64u queued_ms=" FV_LOADER_MS,
                (ULONGLONG)job->index,
                FvLoader_ReasonName(job->reason),
                job->generation,
                FV_LOADER_MS_ARGS(fv_loader_us_between(job->queued_at, job->started_at)));
        }
        fv_loader_execute(job, &context, context_status, &scratch);
        job->finished_at = FV_TRACE_NOW();
        cancelled = job->cancelled;
        if (FV_TRACE_ENABLED()) {
            if (cancelled) {
                FvTrace_Printf(
                    L"job-cancel index=%I64u reason=%s gen=%I64u where=running queued_ms=" FV_LOADER_MS
                    L" work_ms=" FV_LOADER_MS,
                    (ULONGLONG)job->index,
                    FvLoader_ReasonName(job->reason),
                    job->generation,
                    FV_LOADER_MS_ARGS(fv_loader_us_between(job->queued_at, job->started_at)),
                    FV_LOADER_MS_ARGS(fv_loader_us_between(job->started_at, job->finished_at)));
            } else {
                FvTrace_Printf(
                    L"job-done index=%I64u reason=%s gen=%I64u hr=0x%08lX queued_ms=" FV_LOADER_MS
                    L" work_ms=" FV_LOADER_MS,
                    (ULONGLONG)job->index,
                    FvLoader_ReasonName(job->reason),
                    job->generation,
                    (DWORD)job->result,
                    FV_LOADER_MS_ARGS(fv_loader_us_between(job->queued_at, job->started_at)),
                    FV_LOADER_MS_ARGS(fv_loader_us_between(job->started_at, job->finished_at)));
            }
        }

        AcquireSRWLockExclusive(&g_loader.lock);
        g_loader.running = NULL;
        g_loader.running_cancelled = FALSE;
        job->next = NULL;
        if (g_loader.completed_tail != NULL) {
            g_loader.completed_tail->next = job;
        } else {
            g_loader.completed_head = job;
        }
        g_loader.completed_tail = job;
        notify = !g_loader.notify_posted && !g_loader.stopping;
        if (notify) {
            g_loader.notify_posted = TRUE;
            ReleaseSRWLockExclusive(&g_loader.lock);
            /*
             * Уведомление без указателей: если окно уже уничтожено или очередь
             * сообщений переполнена, результат остаётся в списке и будет
             * забран следующим уведомлением или освобождён FvLoader_Stop.
             */
            if (!PostMessageW(g_loader.window, g_loader.message, 0, 0)) {
                AcquireSRWLockExclusive(&g_loader.lock);
                g_loader.notify_posted = FALSE;
                ReleaseSRWLockExclusive(&g_loader.lock);
            }
            AcquireSRWLockExclusive(&g_loader.lock);
        }
    }
    ReleaseSRWLockExclusive(&g_loader.lock);

    ZipReadScratch_Free(&scratch);
    ImageDecoderContext_Destroy(&context);
    if (FV_TRACE_ENABLED()) {
        FvTrace_Printf(L"loader-exit");
    }
    return 0;
}

BOOL FvLoader_Start(HWND window, UINT message) {
    if (g_loader.thread != NULL) {
        return TRUE;
    }
    g_loader.window = window;
    g_loader.message = message;
    g_loader.stopping = FALSE;
    g_loader.queue_count = 0;
    g_loader.running = NULL;
    g_loader.running_cancelled = FALSE;
    g_loader.completed_head = NULL;
    g_loader.completed_tail = NULL;
    g_loader.notify_posted = FALSE;
    g_loader.thread = CreateThread(NULL, 0, fv_loader_thread, NULL, 0, NULL);
    if (g_loader.thread == NULL) {
        return FALSE;
    }
    /* Декод не должен отнимать время у UI-потока при отрисовке и вводе. */
    SetThreadPriority(g_loader.thread, THREAD_PRIORITY_BELOW_NORMAL);
    return TRUE;
}

static void fv_loader_free_list(FvLoadJob* job) {
    while (job != NULL) {
        FvLoadJob* next = job->next;

        FvLoader_FreeJob(job);
        job = next;
    }
}

void FvLoader_Stop(void) {
    FvLoadJob* cancelled[FV_LOADER_MAX_QUEUE];
    SIZE_T cancelled_count;
    SIZE_T index;
    FvLoadJob* completed;
    DWORD waited = 0;
    BOOL exited = FALSE;

    if (g_loader.thread == NULL) {
        return;
    }

    AcquireSRWLockExclusive(&g_loader.lock);
    g_loader.stopping = TRUE;
    g_loader.running_cancelled = TRUE;
    cancelled_count = g_loader.queue_count;
    CopyMemory(cancelled, g_loader.queue, cancelled_count * sizeof(cancelled[0]));
    g_loader.queue_count = 0;
    ReleaseSRWLockExclusive(&g_loader.lock);
    WakeAllConditionVariable(&g_loader.wake);

    for (;;) {
        DWORD status = WaitForSingleObject(g_loader.thread, FV_LOADER_STOP_POLL_MS);

        if (status == WAIT_OBJECT_0) {
            exited = TRUE;
            break;
        }
        if (status != WAIT_TIMEOUT || waited >= FV_LOADER_STOP_TIMEOUT_MS) {
            break;
        }
        waited += FV_LOADER_STOP_POLL_MS;
        /* Прерывает синхронный ReadFile (медленный SMB) рабочего потока. */
        CancelSynchronousIo(g_loader.thread);
    }
    if (FV_TRACE_ENABLED()) {
        FvTrace_Printf(
            L"loader-stop exited=%d waited_ms=%lu cancelled=%I64u",
            exited ? 1 : 0,
            waited,
            (ULONGLONG)cancelled_count);
    }
    CloseHandle(g_loader.thread);
    g_loader.thread = NULL;

    for (index = 0; index < cancelled_count; ++index) {
        FvLoader_FreeJob(cancelled[index]);
    }
    AcquireSRWLockExclusive(&g_loader.lock);
    completed = g_loader.completed_head;
    g_loader.completed_head = NULL;
    g_loader.completed_tail = NULL;
    ReleaseSRWLockExclusive(&g_loader.lock);
    fv_loader_free_list(completed);
}

void FvLoader_Replace(const FvLoadRequest* requests, SIZE_T count) {
    FvLoadJob* new_queue[FV_LOADER_MAX_QUEUE];
    FvLoadJob* cancelled[FV_LOADER_MAX_QUEUE];
    FvLoadJob* created[FV_LOADER_MAX_QUEUE];
    BOOL taken[FV_LOADER_MAX_QUEUE];
    SIZE_T new_count = 0;
    SIZE_T cancelled_count = 0;
    SIZE_T created_count = 0;
    SIZE_T queued_after;
    SIZE_T request_index;
    SIZE_T index;
    BOOL running_wanted = FALSE;
    BOOL has_running;
    BOOL running_cancel_requested = FALSE;
    FvPlanJobKey running_key;

    if (g_loader.thread == NULL) {
        return;
    }
    if (count > FV_LOADER_MAX_QUEUE) {
        count = FV_LOADER_MAX_QUEUE;
    }

    AcquireSRWLockExclusive(&g_loader.lock);
    if (g_loader.stopping) {
        ReleaseSRWLockExclusive(&g_loader.lock);
        return;
    }
    for (index = 0; index < g_loader.queue_count; ++index) {
        taken[index] = FALSE;
    }
    has_running = g_loader.running != NULL;
    if (has_running) {
        /*
         * Поля ключа выполняющегося задания после постановки не меняются;
         * поля результата пишет рабочий поток без lock — их не читаем.
         */
        running_key = fv_loader_job_key(g_loader.running);
    } else {
        ZeroMemory(&running_key, sizeof(running_key));
    }

    for (request_index = 0; request_index < count; ++request_index) {
        const FvLoadRequest* request = &requests[request_index];
        FvPlanJobKey key = fv_loader_request_key(request);
        BOOL found = FALSE;

        if (has_running) {
            if (FvPlan_JobKeyEqual(&running_key, &key)) {
                running_wanted = TRUE;
                continue;
            }
        }
        for (index = 0; index < g_loader.queue_count; ++index) {
            FvPlanJobKey queued_key;

            if (taken[index]) {
                continue;
            }
            queued_key = fv_loader_job_key(g_loader.queue[index]);
            if (FvPlan_JobKeyEqual(&queued_key, &key)) {
                FvLoadJob* job = g_loader.queue[index];

                taken[index] = TRUE;
                /* prefetch, ставший текущим кадром, повышается в приоритете. */
                if (request->reason < job->reason) {
                    job->reason = request->reason;
                }
                new_queue[new_count++] = job;
                found = TRUE;
                break;
            }
        }
        if (!found) {
            FvLoadJob* job = fv_loader_create_job(request);

            if (job != NULL) {
                new_queue[new_count++] = job;
                created[created_count++] = job;
            }
        }
    }
    for (index = 0; index < g_loader.queue_count; ++index) {
        if (!taken[index]) {
            cancelled[cancelled_count++] = g_loader.queue[index];
        }
    }
    if (has_running) {
        running_cancel_requested = !running_wanted && !g_loader.running_cancelled;
        g_loader.running_cancelled = !running_wanted;
    }
    CopyMemory(g_loader.queue, new_queue, new_count * sizeof(new_queue[0]));
    g_loader.queue_count = new_count;
    queued_after = new_count;
    /*
     * Трасса — под lock: созданные задания уже видны рабочему потоку, но
     * освободить их он может только после выполнения, а для этого нужен lock.
     */
    if (FV_TRACE_ENABLED()) {
        for (index = 0; index < created_count; ++index) {
            FvTrace_Printf(
                L"job-queue index=%I64u reason=%s gen=%I64u box=%ux%u queue=%I64u",
                (ULONGLONG)created[index]->index,
                FvLoader_ReasonName(created[index]->reason),
                created[index]->generation,
                created[index]->box_width,
                created[index]->box_height,
                (ULONGLONG)queued_after);
        }
        if (running_cancel_requested) {
            FvTrace_Printf(
                L"job-cancel-request index=%I64u reason=%s gen=%I64u where=running",
                (ULONGLONG)running_key.index,
                FvLoader_ReasonName(running_key.reason),
                running_key.generation);
        }
    }
    ReleaseSRWLockExclusive(&g_loader.lock);
    if (new_count != 0) {
        WakeConditionVariable(&g_loader.wake);
    }

    for (index = 0; index < cancelled_count; ++index) {
        if (FV_TRACE_ENABLED()) {
            FvTrace_Printf(
                L"job-cancel index=%I64u reason=%s gen=%I64u where=queue queued_ms=" FV_LOADER_MS,
                (ULONGLONG)cancelled[index]->index,
                FvLoader_ReasonName(cancelled[index]->reason),
                cancelled[index]->generation,
                FV_LOADER_MS_ARGS(fv_loader_us_between(cancelled[index]->queued_at, FvTrace_Now())));
        }
        FvLoader_FreeJob(cancelled[index]);
    }
}

FvLoadJob* FvLoader_TakeCompleted(void) {
    FvLoadJob* completed;

    AcquireSRWLockExclusive(&g_loader.lock);
    completed = g_loader.completed_head;
    g_loader.completed_head = NULL;
    g_loader.completed_tail = NULL;
    g_loader.notify_posted = FALSE;
    ReleaseSRWLockExclusive(&g_loader.lock);
    return completed;
}
