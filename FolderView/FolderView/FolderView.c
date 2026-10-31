#define WIN32_LEAN_AND_MEAN
#ifndef COBJMACROS
#define COBJMACROS
#endif
#ifndef CINTERFACE
#define CINTERFACE
#endif

#include "targetver.h"
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <objidl.h>
#include <shlobj.h>
#include <d2d1.h>

#include "ImageDecoder.h"
#include "Trace.h"
#include "ViewerLoader.h"
#include "ViewerPlan.h"
#include "ZipArchive.h"

/*
 * Windows SDK публикует Direct2D-интерфейсы как opaque-типы для C. Ниже
 * описана только используемая часть ABI vtable. Неиспользуемые методы всё
 * равно занимают свои слоты, поэтому порядок полностью совпадает с d2d1.h.
 */
typedef void (STDMETHODCALLTYPE *FvD2DUnusedMethod)(void);

typedef struct FvD2DFactoryVtbl {
    FvD2DUnusedMethod QueryInterface;
    FvD2DUnusedMethod AddRef;
    ULONG (STDMETHODCALLTYPE *Release)(ID2D1Factory *self);
    FvD2DUnusedMethod ReloadSystemMetrics;
    FvD2DUnusedMethod GetDesktopDpi;
    FvD2DUnusedMethod CreateRectangleGeometry;
    FvD2DUnusedMethod CreateRoundedRectangleGeometry;
    FvD2DUnusedMethod CreateEllipseGeometry;
    FvD2DUnusedMethod CreateGeometryGroup;
    FvD2DUnusedMethod CreateTransformedGeometry;
    FvD2DUnusedMethod CreatePathGeometry;
    FvD2DUnusedMethod CreateStrokeStyle;
    FvD2DUnusedMethod CreateDrawingStateBlock;
    FvD2DUnusedMethod CreateWicBitmapRenderTarget;
    HRESULT (STDMETHODCALLTYPE *CreateHwndRenderTarget)(
        ID2D1Factory *self,
        const D2D1_RENDER_TARGET_PROPERTIES *render_target_properties,
        const D2D1_HWND_RENDER_TARGET_PROPERTIES *window_properties,
        ID2D1HwndRenderTarget **render_target);
    FvD2DUnusedMethod CreateDxgiSurfaceRenderTarget;
    FvD2DUnusedMethod CreateDCRenderTarget;
} FvD2DFactoryVtbl;

struct ID2D1Factory {
    const FvD2DFactoryVtbl *lpVtbl;
};

typedef struct FvD2DHwndRenderTargetVtbl {
    FvD2DUnusedMethod QueryInterface;
    FvD2DUnusedMethod AddRef;
    ULONG (STDMETHODCALLTYPE *Release)(ID2D1HwndRenderTarget *self);
    FvD2DUnusedMethod GetFactory;
    HRESULT (STDMETHODCALLTYPE *CreateBitmap)(
        ID2D1HwndRenderTarget *self,
        D2D1_SIZE_U size,
        const void *source_data,
        UINT32 pitch,
        const D2D1_BITMAP_PROPERTIES *properties,
        ID2D1Bitmap **bitmap);
    FvD2DUnusedMethod CreateBitmapFromWicBitmap;
    FvD2DUnusedMethod CreateSharedBitmap;
    FvD2DUnusedMethod CreateBitmapBrush;
    FvD2DUnusedMethod CreateSolidColorBrush;
    FvD2DUnusedMethod CreateGradientStopCollection;
    FvD2DUnusedMethod CreateLinearGradientBrush;
    FvD2DUnusedMethod CreateRadialGradientBrush;
    FvD2DUnusedMethod CreateCompatibleRenderTarget;
    FvD2DUnusedMethod CreateLayer;
    FvD2DUnusedMethod CreateMesh;
    FvD2DUnusedMethod DrawLine;
    FvD2DUnusedMethod DrawRectangle;
    FvD2DUnusedMethod FillRectangle;
    FvD2DUnusedMethod DrawRoundedRectangle;
    FvD2DUnusedMethod FillRoundedRectangle;
    FvD2DUnusedMethod DrawEllipse;
    FvD2DUnusedMethod FillEllipse;
    FvD2DUnusedMethod DrawGeometry;
    FvD2DUnusedMethod FillGeometry;
    FvD2DUnusedMethod FillMesh;
    FvD2DUnusedMethod FillOpacityMask;
    void (STDMETHODCALLTYPE *DrawBitmap)(
        ID2D1HwndRenderTarget *self,
        ID2D1Bitmap *bitmap,
        const D2D1_RECT_F *destination,
        FLOAT opacity,
        D2D1_BITMAP_INTERPOLATION_MODE interpolation,
        const D2D1_RECT_F *source);
    FvD2DUnusedMethod DrawText;
    FvD2DUnusedMethod DrawTextLayout;
    FvD2DUnusedMethod DrawGlyphRun;
    FvD2DUnusedMethod SetTransform;
    FvD2DUnusedMethod GetTransform;
    FvD2DUnusedMethod SetAntialiasMode;
    FvD2DUnusedMethod GetAntialiasMode;
    FvD2DUnusedMethod SetTextAntialiasMode;
    FvD2DUnusedMethod GetTextAntialiasMode;
    FvD2DUnusedMethod SetTextRenderingParams;
    FvD2DUnusedMethod GetTextRenderingParams;
    FvD2DUnusedMethod SetTags;
    FvD2DUnusedMethod GetTags;
    FvD2DUnusedMethod PushLayer;
    FvD2DUnusedMethod PopLayer;
    FvD2DUnusedMethod Flush;
    FvD2DUnusedMethod SaveDrawingState;
    FvD2DUnusedMethod RestoreDrawingState;
    FvD2DUnusedMethod PushAxisAlignedClip;
    FvD2DUnusedMethod PopAxisAlignedClip;
    void (STDMETHODCALLTYPE *Clear)(
        ID2D1HwndRenderTarget *self,
        const D2D1_COLOR_F *color);
    void (STDMETHODCALLTYPE *BeginDraw)(ID2D1HwndRenderTarget *self);
    HRESULT (STDMETHODCALLTYPE *EndDraw)(
        ID2D1HwndRenderTarget *self,
        D2D1_TAG *tag1,
        D2D1_TAG *tag2);
    FvD2DUnusedMethod GetPixelFormat;
    FvD2DUnusedMethod SetDpi;
    FvD2DUnusedMethod GetDpi;
    FvD2DUnusedMethod GetSize;
    FvD2DUnusedMethod GetPixelSize;
    UINT32 (STDMETHODCALLTYPE *GetMaximumBitmapSize)(ID2D1HwndRenderTarget *self);
    FvD2DUnusedMethod IsSupported;
    D2D1_WINDOW_STATE (STDMETHODCALLTYPE *CheckWindowState)(ID2D1HwndRenderTarget *self);
    HRESULT (STDMETHODCALLTYPE *Resize)(
        ID2D1HwndRenderTarget *self,
        const D2D1_SIZE_U *pixel_size);
    FvD2DUnusedMethod GetHwnd;
} FvD2DHwndRenderTargetVtbl;

struct ID2D1HwndRenderTarget {
    const FvD2DHwndRenderTargetVtbl *lpVtbl;
};

typedef struct FvIUnknownVtbl {
    FvD2DUnusedMethod QueryInterface;
    ULONG (STDMETHODCALLTYPE *AddRef)(void *self);
    ULONG (STDMETHODCALLTYPE *Release)(void *self);
} FvIUnknownVtbl;

struct ID2D1Bitmap {
    const FvIUnknownVtbl *lpVtbl;
};

#define FV_FACTORY_VT(object) ((object)->lpVtbl)
#define FV_TARGET_VT(object) ((object)->lpVtbl)
#define FV_UNKNOWN_VT(object) ((object)->lpVtbl)

/*
 * Кэш кадров ограничен и радиусом вокруг текущего кадра, и бюджетом в байтах
 * (пиксели + D2D-битмапы, ViewerPlan.h); вытесняются самые дальние кадры.
 */
#define FV_CACHE_RADIUS ((SIZE_T)50)
/* Максимальный размер D2D-битмапа до создания render target (типичный). */
#define FV_DEFAULT_MAX_BITMAP_SIZE ((UINT32)16384)
#define FV_MAX_ZIP_IMAGE_BYTES FV_LOADER_MAX_ZIP_IMAGE_BYTES
/* Уведомление рабочего потока: есть завершённые задания (ViewerLoader.h). */
#define WM_FV_LOADED (WM_APP + 1)
/* Окно prefetch плюс текущий кадр и передекод. */
#define FV_MAX_LOAD_REQUESTS ((SIZE_T)(2U * FV_PLAN_PREFETCH_MAX + 2U))

typedef struct FvStringList {
    WCHAR **items;
    SIZE_T count;
    SIZE_T capacity;
} FvStringList;

typedef struct FvWideBuilder {
    WCHAR *data;
    SIZE_T length;
    SIZE_T capacity;
} FvWideBuilder;

/*
 * Кадр в кэше: декодированные пиксели (обычно под размер монитора) и
 * созданный по ним D2D-битмап. Битмап живёт рядом с кадром, чтобы возврат к
 * уже показанному кадру не стоил повторной загрузки в GPU; освобождается при
 * вытеснении кадра, передекодировании и пересоздании render target.
 */
typedef struct FvCacheEntry {
    DecodedImage image;
    ID2D1Bitmap *bitmap;
    SIZE_T bitmap_bytes;
    /*
     * Фоновая загрузка кадра не удалась: prefetch его больше не просит,
     * текущим кадром он перезапрашивается только при явном переходе к нему.
     */
    BOOL load_failed;
    /*
     * Фактический размер кадра в кэше (пиксели + битмап) после фонового
     * декода под рамку known_box_*: переживает вытеснение (в пределах
     * поколения — кэш пересоздаётся со списком) и заменяет в бюджете
     * prefetch оценку по текущему кадру (FvPlan_FitPrefetch).
     */
    ULONGLONG known_bytes;
    UINT known_box_width;
    UINT known_box_height;
    /* Вытеснен по бюджету сразу после prefetch: не заказывать при тех же условиях. */
    BOOL prefetch_suppressed;
} FvCacheEntry;

static HANDLE g_heap;
static ID2D1Factory *g_d2d_factory;
static ID2D1HwndRenderTarget *g_render_target;
/* Предел стороны D2D-битмапа текущего render target. */
static UINT32 g_max_bitmap_size = FV_DEFAULT_MAX_BITMAP_SIZE;
/* Бюджет кэша кадров (пиксели + битмапы) и его часть под битмапы, байт. */
static ULONGLONG g_cache_budget;
static ULONGLONG g_bitmap_budget;
static FvStringList g_files;
static FvCacheEntry *g_cache;
/* Кадр, ленивое передекодирование которого не удалось (до смены кадра). */
static SIZE_T g_upgrade_failed_image = ~(SIZE_T)0;
static ZipArchive g_zip;
static const ZipEntry **g_zip_entries;
static BOOL g_zip_mode;
static SIZE_T g_current_image;
static BOOL g_block_navigation;
static BOOL g_has_image_shift;
static POINT g_image_shift;
static POINTS g_drag_origin;
static FLOAT g_scale = 1.0f;
/* Трасса FOLDERVIEW_TRACE: начало текущего перехода и признак первого кадра. */
static ULONGLONG g_trace_frame_started;
static BOOL g_trace_frame_pending;
static BOOL g_trace_first_paint_done;
/*
 * Фоновая загрузка. g_generation — поколение списка кадров: меняется
 * при каждой смене g_files/g_cache (ZIP, папка), результаты чужого
 * поколения отбрасываются. g_loader_archive — ссылка UI-потока на дубликат
 * хэндла g_zip для рабочего потока.
 */
static BOOL g_loader_active;
static ULONGLONG g_generation;
static FvLoaderArchive *g_loader_archive;
/* Текущий кадр показан (масштаб, заголовок, размер окна посчитаны по нему). */
static BOOL g_current_activated;
/* Направление последнего перехода: +1 вперёд, -1 назад (окно prefetch). */
static int g_nav_direction = 1;
static SIZE_T g_prefetch_ahead = FV_PLAN_PREFETCH_AHEAD;
static SIZE_T g_prefetch_behind = FV_PLAN_PREFETCH_BEHIND;
/*
 * Условия, при которых кадры с FvCacheEntry.prefetch_suppressed были
 * вытеснены сразу после prefetch; при других условиях пометки снимаются.
 */
static FvPlanPrefetchContext g_prefetch_suppress_context;
static BOOL g_prefetch_suppress_any;
/* Зум и 1:1, нажатые до готовности кадра: применяются при его показе. */
static int g_deferred_zoom_steps;
static BOOL g_deferred_actual_size;
/*
 * Последний показанный кадр (своя ссылка на битмап и прямоугольник): его
 * рисует отрисовка, пока новый текущий кадр загружается, — без серого фона.
 */
static ID2D1Bitmap *g_shown_bitmap;
static D2D1_RECT_F g_shown_rect;

/* Микросекунды как "мс.ддд" для wsprintfW: формат FV_MS, аргументы FV_MS_ARGS. */
#define FV_MS L"%I64u.%03I64u"
#define FV_MS_ARGS(us) (ULONGLONG)(us) / 1000ULL, (ULONGLONG)(us) % 1000ULL
#define FV_TRACE_PATH_CHARS 240

static LRESULT CALLBACK FvWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
static void fv_set_current_image(HWND window, BOOL repaint);
static void fv_previous_image(HWND window);
static void fv_next_image(HWND window);
static void fv_draw_image(HWND window);
static void fv_update_window_title(HWND window);
static void fv_release_entry_bitmap(FvCacheEntry *entry);
static void fv_release_all_bitmaps(void);
static void fv_cancel_loads(void);
static void fv_schedule_loads(HWND window);

static BOOL fv_size_multiply(SIZE_T left, SIZE_T right, SIZE_T *result) {
    SIZE_T maximum = ~(SIZE_T)0;

    if (left != 0 && right > maximum / left) {
        return FALSE;
    }

    *result = left * right;
    return TRUE;
}

static void *fv_heap_allocate(SIZE_T size, BOOL zero_memory) {
    DWORD flags = zero_memory ? HEAP_ZERO_MEMORY : 0;

    if (size == 0) {
        size = 1;
    }
    return HeapAlloc(g_heap, flags, size);
}

static void fv_heap_free(void *memory) {
    if (memory != NULL) {
        HeapFree(g_heap, 0, memory);
    }
}

static WCHAR *fv_wide_duplicate_n(const WCHAR *text, SIZE_T length) {
    SIZE_T byte_count;
    WCHAR *copy;
    SIZE_T index;

    if (length == ~(SIZE_T)0 ||
        !fv_size_multiply(length + 1, sizeof(WCHAR), &byte_count)) {
        return NULL;
    }

    copy = (WCHAR *)fv_heap_allocate(byte_count, FALSE);
    if (copy == NULL) {
        return NULL;
    }

    for (index = 0; index < length; ++index) {
        copy[index] = text[index];
    }
    copy[length] = L'\0';
    return copy;
}

static WCHAR *fv_wide_duplicate(const WCHAR *text) {
    if (text == NULL) {
        return NULL;
    }
    return fv_wide_duplicate_n(text, (SIZE_T)lstrlenW(text));
}

static void fv_string_list_init(FvStringList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static void fv_string_list_clear(FvStringList *list) {
    SIZE_T index;

    for (index = 0; index < list->count; ++index) {
        fv_heap_free(list->items[index]);
    }
    fv_heap_free(list->items);
    fv_string_list_init(list);
}

static BOOL fv_string_list_reserve(FvStringList *list, SIZE_T required) {
    SIZE_T new_capacity;
    SIZE_T byte_count;
    WCHAR **new_items;

    if (required <= list->capacity) {
        return TRUE;
    }

    new_capacity = list->capacity == 0 ? 32 : list->capacity;
    while (new_capacity < required) {
        if (new_capacity > (~(SIZE_T)0) / 2) {
            return FALSE;
        }
        new_capacity *= 2;
    }

    if (!fv_size_multiply(new_capacity, sizeof(WCHAR *), &byte_count)) {
        return FALSE;
    }

    if (list->items == NULL) {
        new_items = (WCHAR **)fv_heap_allocate(byte_count, FALSE);
    } else {
        new_items = (WCHAR **)HeapReAlloc(g_heap, 0, list->items, byte_count);
    }

    if (new_items == NULL) {
        return FALSE;
    }

    list->items = new_items;
    list->capacity = new_capacity;
    return TRUE;
}

static BOOL fv_string_list_add_owned(FvStringList *list, WCHAR *text) {
    if (!fv_string_list_reserve(list, list->count + 1)) {
        fv_heap_free(text);
        return FALSE;
    }

    list->items[list->count++] = text;
    return TRUE;
}

static BOOL fv_string_list_add(FvStringList *list, const WCHAR *text) {
    WCHAR *copy = fv_wide_duplicate(text);

    if (copy == NULL) {
        return FALSE;
    }
    return fv_string_list_add_owned(list, copy);
}

static const WCHAR *fv_file_name(const WCHAR *path) {
    const WCHAR *name = path;
    const WCHAR *cursor = path;

    while (*cursor != L'\0') {
        if (*cursor == L'\\' || *cursor == L'/') {
            name = cursor + 1;
        }
        ++cursor;
    }
    return name;
}

static const WCHAR *fv_extension(const WCHAR *path) {
    const WCHAR *name = fv_file_name(path);
    const WCHAR *extension = NULL;

    while (*name != L'\0') {
        if (*name == L'.') {
            extension = name;
        }
        ++name;
    }
    return extension;
}

static BOOL fv_is_supported_image(const WCHAR *path) {
    static const WCHAR *extensions[] = {
        L".jpg", L".jpeg", L".png", L".bmp", L".gif", L".tiff",
        L".tif", L".ico", L".webp", L".heif", L".heic"
    };
    const WCHAR *extension = fv_extension(path);
    SIZE_T index;

    if (extension == NULL) {
        return FALSE;
    }

    for (index = 0; index < sizeof(extensions) / sizeof(extensions[0]); ++index) {
        if (lstrcmpiW(extension, extensions[index]) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL fv_is_zip_file(const WCHAR *path) {
    const WCHAR *extension = fv_extension(path);
    return extension != NULL && lstrcmpiW(extension, L".zip") == 0;
}

static int fv_compare_name_parts(
    const WCHAR *left,
    SIZE_T left_length,
    const WCHAR *right,
    SIZE_T right_length) {
    int result;

    if (left_length > 0x7fffffff || right_length > 0x7fffffff) {
        return left_length < right_length ? -1 : left_length != right_length;
    }

    /* lstrcmpiW использует лингвистическое сравнение текущей локали. */
    result = CompareStringW(
        GetThreadLocale(),
        NORM_IGNORECASE,
        left,
        (int)left_length,
        right,
        (int)right_length);
    if (result == CSTR_LESS_THAN) {
        return -1;
    }
    if (result == CSTR_GREATER_THAN) {
        return 1;
    }
    return 0;
}

static int fv_compare_paths(const WCHAR *left_path, const WCHAR *right_path) {
    const WCHAR *left = fv_file_name(left_path);
    const WCHAR *right = fv_file_name(right_path);
    const WCHAR *left_dot = NULL;
    const WCHAR *right_dot = NULL;
    const WCHAR *cursor;
    int result;

    for (cursor = left; *cursor != L'\0'; ++cursor) {
        if (*cursor == L'.') {
            left_dot = cursor;
        }
    }
    for (cursor = right; *cursor != L'\0'; ++cursor) {
        if (*cursor == L'.') {
            right_dot = cursor;
        }
    }

    result = fv_compare_name_parts(
        left,
        (SIZE_T)((left_dot != NULL ? left_dot : left + lstrlenW(left)) - left),
        right,
        (SIZE_T)((right_dot != NULL ? right_dot : right + lstrlenW(right)) - right));
    if (result != 0) {
        return result;
    }

    return lstrcmpiW(left, right);
}

static void fv_string_list_swap(
    FvStringList *list,
    const ZipEntry **zip_entries,
    SIZE_T left,
    SIZE_T right) {
    WCHAR *text = list->items[left];

    list->items[left] = list->items[right];
    list->items[right] = text;
    if (zip_entries != NULL) {
        const ZipEntry *entry = zip_entries[left];

        zip_entries[left] = zip_entries[right];
        zip_entries[right] = entry;
    }
}

static void fv_string_list_sort_with_entries(
    FvStringList *list,
    const ZipEntry **zip_entries) {
    SIZE_T start;
    SIZE_T end;

    if (list->count < 2) {
        return;
    }

    /* Heap sort сохраняет O(n log n) без CRT, рекурсии и временного буфера. */
    for (start = list->count / 2; start != 0; --start) {
        SIZE_T root = start - 1;

        while (root <= (list->count - 2) / 2) {
            SIZE_T child = root * 2 + 1;

            if (child + 1 < list->count &&
                fv_compare_paths(list->items[child], list->items[child + 1]) < 0) {
                ++child;
            }
            if (fv_compare_paths(list->items[root], list->items[child]) >= 0) {
                break;
            }
            fv_string_list_swap(list, zip_entries, root, child);
            root = child;
        }
    }

    for (end = list->count; end > 1; --end) {
        SIZE_T root = 0;
        SIZE_T heap_count = end - 1;

        fv_string_list_swap(list, zip_entries, 0, heap_count);

        while (heap_count > 1 && root <= (heap_count - 2) / 2) {
            SIZE_T child = root * 2 + 1;

            if (child + 1 < heap_count &&
                fv_compare_paths(list->items[child], list->items[child + 1]) < 0) {
                ++child;
            }
            if (fv_compare_paths(list->items[root], list->items[child]) >= 0) {
                break;
            }
            fv_string_list_swap(list, zip_entries, root, child);
            root = child;
        }
    }
}

static void fv_string_list_sort(FvStringList *list) {
    fv_string_list_sort_with_entries(list, NULL);
}

static WCHAR *fv_join_path(const WCHAR *directory, const WCHAR *name) {
    SIZE_T directory_length = (SIZE_T)lstrlenW(directory);
    SIZE_T name_length = (SIZE_T)lstrlenW(name);
    BOOL needs_separator = directory_length != 0 &&
        directory[directory_length - 1] != L'\\' &&
        directory[directory_length - 1] != L'/';
    SIZE_T total_length = directory_length + name_length + (needs_separator ? 1 : 0);
    SIZE_T byte_count;
    WCHAR *result;
    SIZE_T position;
    SIZE_T index;

    if (total_length == ~(SIZE_T)0 ||
        !fv_size_multiply(total_length + 1, sizeof(WCHAR), &byte_count)) {
        return NULL;
    }
    result = (WCHAR *)fv_heap_allocate(byte_count, FALSE);
    if (result == NULL) {
        return NULL;
    }

    for (index = 0; index < directory_length; ++index) {
        result[index] = directory[index];
    }
    position = directory_length;
    if (needs_separator) {
        result[position++] = L'\\';
    }
    for (index = 0; index < name_length; ++index) {
        result[position++] = name[index];
    }
    result[position] = L'\0';
    return result;
}

static WCHAR *fv_parent_path(const WCHAR *path) {
    SIZE_T length = (SIZE_T)lstrlenW(path);
    SIZE_T separator;

    while (length > 0 && (path[length - 1] == L'\\' || path[length - 1] == L'/')) {
        if (length == 3 && path[1] == L':') {
            break;
        }
        --length;
    }

    separator = length;
    while (separator > 0 && path[separator - 1] != L'\\' && path[separator - 1] != L'/') {
        --separator;
    }

    if (separator == 0) {
        return fv_wide_duplicate(L".");
    }
    if (separator == 3 && path[1] == L':') {
        return fv_wide_duplicate_n(path, 3);
    }
    return fv_wide_duplicate_n(path, separator - 1);
}

static WCHAR *fv_full_path(const WCHAR *path) {
    DWORD required = GetFullPathNameW(path, 0, NULL, NULL);
    WCHAR *result;
    DWORD actual;

    if (required == 0 || required >= 0x7fffffff) {
        return NULL;
    }

    result = (WCHAR *)fv_heap_allocate(((SIZE_T)required + 1) * sizeof(WCHAR), FALSE);
    if (result == NULL) {
        return NULL;
    }

    actual = GetFullPathNameW(path, required + 1, result, NULL);
    if (actual == 0 || actual > required) {
        fv_heap_free(result);
        return NULL;
    }
    return result;
}

static WCHAR *fv_current_directory(void) {
    DWORD required = GetCurrentDirectoryW(0, NULL);
    WCHAR *result;

    if (required == 0 || required >= 0x7fffffff) {
        return NULL;
    }

    result = (WCHAR *)fv_heap_allocate(((SIZE_T)required + 1) * sizeof(WCHAR), FALSE);
    if (result == NULL) {
        return NULL;
    }
    if (GetCurrentDirectoryW(required + 1, result) == 0) {
        fv_heap_free(result);
        return NULL;
    }
    return result;
}

static BOOL fv_builder_reserve(FvWideBuilder *builder, SIZE_T extra) {
    SIZE_T required;
    SIZE_T capacity;
    SIZE_T bytes;
    WCHAR *data;

    if (extra > (~(SIZE_T)0) - builder->length - 1) {
        return FALSE;
    }
    required = builder->length + extra + 1;
    if (required <= builder->capacity) {
        return TRUE;
    }

    capacity = builder->capacity == 0 ? 128 : builder->capacity;
    while (capacity < required) {
        if (capacity > (~(SIZE_T)0) / 2) {
            return FALSE;
        }
        capacity *= 2;
    }
    if (!fv_size_multiply(capacity, sizeof(WCHAR), &bytes)) {
        return FALSE;
    }

    if (builder->data == NULL) {
        data = (WCHAR *)fv_heap_allocate(bytes, FALSE);
    } else {
        data = (WCHAR *)HeapReAlloc(g_heap, 0, builder->data, bytes);
    }
    if (data == NULL) {
        return FALSE;
    }

    builder->data = data;
    builder->capacity = capacity;
    return TRUE;
}

static BOOL fv_builder_append_n(FvWideBuilder *builder, const WCHAR *text, SIZE_T length) {
    SIZE_T index;

    if (!fv_builder_reserve(builder, length)) {
        return FALSE;
    }
    for (index = 0; index < length; ++index) {
        builder->data[builder->length++] = text[index];
    }
    builder->data[builder->length] = L'\0';
    return TRUE;
}

static BOOL fv_builder_append(FvWideBuilder *builder, const WCHAR *text) {
    return fv_builder_append_n(builder, text, (SIZE_T)lstrlenW(text));
}

static BOOL fv_builder_append_u64(FvWideBuilder *builder, ULONGLONG value) {
    WCHAR reversed[32];
    SIZE_T count = 0;

    do {
        reversed[count++] = (WCHAR)(L'0' + (value % 10));
        value /= 10;
    } while (value != 0);

    if (!fv_builder_reserve(builder, count)) {
        return FALSE;
    }
    while (count > 0) {
        builder->data[builder->length++] = reversed[--count];
    }
    builder->data[builder->length] = L'\0';
    return TRUE;
}

static void fv_builder_dispose(FvWideBuilder *builder) {
    fv_heap_free(builder->data);
    builder->data = NULL;
    builder->length = 0;
    builder->capacity = 0;
}

static void fv_free_cache_entry(FvCacheEntry *entry) {
    fv_release_entry_bitmap(entry);
    DecodedImage_Free(&entry->image);
}

static void fv_free_cache(void) {
    SIZE_T index;

    if (g_cache == NULL) {
        return;
    }
    for (index = 0; index < g_files.count; ++index) {
        fv_free_cache_entry(&g_cache[index]);
    }
    fv_heap_free(g_cache);
    g_cache = NULL;
}

static void fv_clear_files_and_cache(void) {
    /* Индексы и записи прежнего списка больше не действительны. */
    ++g_generation;
    fv_cancel_loads();
    fv_free_cache();
    fv_string_list_clear(&g_files);
    fv_heap_free(g_zip_entries);
    g_zip_entries = NULL;
    g_current_image = 0;
    g_has_image_shift = FALSE;
    g_image_shift.x = 0;
    g_image_shift.y = 0;
}

static BOOL fv_allocate_cache_for_count(SIZE_T count, FvCacheEntry **cache) {
    SIZE_T bytes;

    *cache = NULL;
    if (count == 0) {
        return TRUE;
    }
    if (!fv_size_multiply(count, sizeof(FvCacheEntry), &bytes)) {
        return FALSE;
    }

    *cache = (FvCacheEntry *)fv_heap_allocate(bytes, TRUE);
    return *cache != NULL;
}

static BOOL fv_allocate_cache(void) {
    return fv_allocate_cache_for_count(g_files.count, &g_cache);
}

static BOOL fv_enumerate_matching_files(
    const WCHAR *directory,
    BOOL zip_files,
    FvStringList *destination) {
    WCHAR *pattern = fv_join_path(directory, L"*");
    WIN32_FIND_DATAW find_data;
    HANDLE find_handle;
    DWORD find_error;
    BOOL success = TRUE;

    if (pattern == NULL) {
        return FALSE;
    }

    find_handle = FindFirstFileW(pattern, &find_data);
    find_error = find_handle == INVALID_HANDLE_VALUE ? GetLastError() : ERROR_SUCCESS;
    fv_heap_free(pattern);
    if (find_handle == INVALID_HANDLE_VALUE) {
        return find_error == ERROR_FILE_NOT_FOUND;
    }

    do {
        WCHAR *full_path;
        BOOL matches;

        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }

        matches = zip_files
            ? fv_is_zip_file(find_data.cFileName)
            : fv_is_supported_image(find_data.cFileName);
        if (!matches) {
            continue;
        }

        full_path = fv_join_path(directory, find_data.cFileName);
        if (full_path == NULL || !fv_string_list_add_owned(destination, full_path)) {
            success = FALSE;
            break;
        }
    } while (FindNextFileW(find_handle, &find_data));

    FindClose(find_handle);
    return success;
}

static BOOL fv_build_disk_file_list(const WCHAR *input) {
    DWORD attributes = GetFileAttributesW(input);
    WCHAR *directory = NULL;
    WCHAR *selected_file = NULL;
    SIZE_T index;

    if (attributes == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(NULL, L"Путь не найден", L"FolderView", MB_OK | MB_ICONERROR);
        return FALSE;
    }

    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        directory = fv_wide_duplicate(input);
    } else {
        directory = fv_parent_path(input);
        selected_file = fv_wide_duplicate(input);
    }
    if (directory == NULL || (selected_file == NULL && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)) {
        fv_heap_free(directory);
        fv_heap_free(selected_file);
        return FALSE;
    }

    if (!fv_enumerate_matching_files(directory, FALSE, &g_files)) {
        fv_heap_free(directory);
        fv_heap_free(selected_file);
        return FALSE;
    }
    fv_heap_free(directory);

    fv_string_list_sort(&g_files);
    if (selected_file != NULL) {
        for (index = 0; index < g_files.count; ++index) {
            if (lstrcmpiW(g_files.items[index], selected_file) == 0) {
                g_current_image = index;
                break;
            }
        }
    }
    fv_heap_free(selected_file);
    return fv_allocate_cache();
}

static BOOL fv_virtual_path_equal(const WCHAR *left, const WCHAR *right) {
    WCHAR left_character;
    WCHAR right_character;

    for (;;) {
        left_character = *left++;
        right_character = *right++;
        if (left_character == L'/') {
            left_character = L'\\';
        }
        if (right_character == L'/') {
            right_character = L'\\';
        }
        if (left_character != right_character &&
            CompareStringOrdinal(
                &left_character,
                1,
                &right_character,
                1,
                TRUE) != CSTR_EQUAL) {
            return FALSE;
        }
        if (left_character == L'\0') {
            return TRUE;
        }
    }
}

static ULONGLONG fv_trace_us_since(ULONGLONG started) {
    return FvTrace_TicksToMicroseconds(FvTrace_Now() - started);
}

static void fv_trace_zip_open(const ZipArchive *archive, ULONGLONG started) {
    ZipIoStats io;
    ZipOpenStats open_stats;
    ULONGLONG total_us = fv_trace_us_since(started);
    ULONGLONG tail_us;
    ULONGLONG directory_us;

    ZipArchive_GetIoStats(archive, &io);
    ZipArchive_GetOpenStats(archive, &open_stats);
    tail_us = FvTrace_TicksToMicroseconds(open_stats.tail_ticks);
    directory_us = FvTrace_TicksToMicroseconds(open_stats.directory_ticks);
    FvTrace_Printf(
        L"tail-read ms=" FV_MS L" reads=%I64u bytes=%I64u",
        FV_MS_ARGS(tail_us),
        open_stats.tail_io.read_calls,
        open_stats.tail_io.read_bytes);
    FvTrace_Printf(
        L"cd-parse ms=" FV_MS L" reads=%I64u bytes=%I64u entries=%I64u",
        FV_MS_ARGS(directory_us),
        open_stats.directory_io.read_calls,
        open_stats.directory_io.read_bytes,
        (ULONGLONG)ZipArchive_EntryCount(archive));
    FvTrace_Printf(
        L"open ms=" FV_MS L" reads=%I64u bytes=%I64u file_size=%I64u path=%s",
        FV_MS_ARGS(total_us),
        io.read_calls,
        io.read_bytes,
        archive->file_size,
        FvTrace_Tail(ZipArchive_Path(archive), FV_TRACE_PATH_CHARS));
}

/*
 * Ссылка рабочего потока на открытый g_zip: дубликат хэндла (тот же file
 * object, без нового сетевого открытия). Пересоздаётся при каждом новом
 * открытии g_zip; при переиспользовании того же ZIP (fv_can_reuse_open_zip,
 * сверка размера и времени изменения) остаётся прежней.
 */
static void fv_refresh_loader_archive(void) {
    FvLoaderArchive_Release(g_loader_archive);
    g_loader_archive = NULL;
    if (g_loader_active && g_zip_mode && ZipArchive_IsOpen(&g_zip)) {
        g_loader_archive = FvLoaderArchive_Create(&g_zip);
        if (g_loader_archive == NULL && FV_TRACE_ENABLED()) {
            FvTrace_Printf(L"loader-archive-failed error=%lu", GetLastError());
        }
    }
}

static BOOL fv_can_reuse_open_zip(const WCHAR *zip_path) {
    const WCHAR *open_path;

    if (!g_zip_mode || zip_path == NULL || !ZipArchive_IsOpen(&g_zip)) {
        return FALSE;
    }
    open_path = ZipArchive_Path(&g_zip);
    if (open_path == NULL ||
        CompareStringOrdinal(open_path, -1, zip_path, -1, TRUE) != CSTR_EQUAL) {
        return FALSE;
    }
    return ZipArchive_IsUnchanged(&g_zip);
}

static BOOL fv_build_zip_file_list(
    const WCHAR *zip_path,
    const WCHAR *start_entry,
    BOOL require_image) {
    ZipArchive temporary_zip;
    /* Архив, из которого строится список: новый temporary_zip или уже открытый g_zip. */
    ZipArchive *archive = &temporary_zip;
    BOOL reuse_open_zip;
    FvStringList temporary_files;
    const ZipEntry **temporary_zip_entries = NULL;
    FvCacheEntry *temporary_cache = NULL;
    SIZE_T temporary_current_image = 0;
    SIZE_T entry_count;
    SIZE_T byte_count;
    SIZE_T index;
    BOOL success = FALSE;
    ULONGLONG trace_started;

    ZipArchive_Init(&temporary_zip);
    fv_string_list_init(&temporary_files);
    trace_started = FV_TRACE_NOW();
    /*
     * Если путь совпадает с уже открытым архивом и файл не изменился,
     * central directory и записи переиспользуются без повторного открытия
     * и разбора.
     */
    reuse_open_zip = fv_can_reuse_open_zip(zip_path);
    if (reuse_open_zip) {
        archive = &g_zip;
        if (FV_TRACE_ENABLED()) {
            ULONGLONG elapsed_us = fv_trace_us_since(trace_started);

            FvTrace_Printf(
                L"archive-reuse ms=" FV_MS L" entries=%I64u path=%s",
                FV_MS_ARGS(elapsed_us),
                (ULONGLONG)ZipArchive_EntryCount(archive),
                FvTrace_Tail(ZipArchive_Path(archive), FV_TRACE_PATH_CHARS));
        }
    } else if (!ZipArchive_Open(&temporary_zip, zip_path)) {
        if (FV_TRACE_ENABLED()) {
            DWORD error = GetLastError();

            FvTrace_Printf(
                L"open-failed error=%lu path=%s",
                error,
                FvTrace_Tail(zip_path, FV_TRACE_PATH_CHARS));
        }
        MessageBoxW(NULL, L"Не удалось открыть ZIP-архив", L"FolderView", MB_OK | MB_ICONERROR);
        goto cleanup;
    } else if (FV_TRACE_ENABLED()) {
        fv_trace_zip_open(&temporary_zip, trace_started);
    }

    entry_count = ZipArchive_EntryCount(archive);
    if (entry_count != 0) {
        if (!fv_size_multiply(entry_count, sizeof(*temporary_zip_entries), &byte_count)) {
            goto cleanup;
        }
        temporary_zip_entries =
            (const ZipEntry **)fv_heap_allocate(byte_count, FALSE);
        if (temporary_zip_entries == NULL) {
            goto cleanup;
        }
    }

    for (index = 0; index < entry_count; ++index) {
        const ZipEntry *entry = ZipArchive_EntryAt(archive, index);
        const WCHAR *name = entry != NULL ? ZipEntry_Name(entry) : NULL;

        if (name != NULL && fv_is_supported_image(name)) {
            SIZE_T image_index = temporary_files.count;

            if (!fv_string_list_add(&temporary_files, name)) {
                goto cleanup;
            }
            temporary_zip_entries[image_index] = entry;
        }
    }

    if (require_image && temporary_files.count == 0) {
        goto cleanup;
    }

    fv_string_list_sort_with_entries(&temporary_files, temporary_zip_entries);
    if (start_entry != NULL && *start_entry != L'\0') {
        for (index = 0; index < temporary_files.count; ++index) {
            if (fv_virtual_path_equal(temporary_files.items[index], start_entry)) {
                temporary_current_image = index;
                break;
            }
        }
    }
    if (!fv_allocate_cache_for_count(temporary_files.count, &temporary_cache)) {
        goto cleanup;
    }

    /*
     * Старое состояние уничтожается только после полной подготовки нового.
     * При переиспользовании g_zip остаётся открытым: записи, на которые
     * ссылается новый список, принадлежат ему.
     */
    fv_clear_files_and_cache();
    if (!reuse_open_zip) {
        ZipArchive_Close(&g_zip);
        g_zip = temporary_zip;
        ZipArchive_Init(&temporary_zip);
    }
    g_files = temporary_files;
    fv_string_list_init(&temporary_files);
    g_zip_entries = temporary_zip_entries;
    temporary_zip_entries = NULL;
    g_cache = temporary_cache;
    temporary_cache = NULL;
    g_current_image = temporary_current_image;
    g_zip_mode = TRUE;
    if (!reuse_open_zip || g_loader_archive == NULL) {
        fv_refresh_loader_archive();
    }
    success = TRUE;

cleanup:
    fv_heap_free(temporary_cache);
    fv_heap_free(temporary_zip_entries);
    fv_string_list_clear(&temporary_files);
    ZipArchive_Close(&temporary_zip);
    return success;
}

static BOOL fv_split_zip_virtual_path(
    const WCHAR *input,
    WCHAR **zip_path,
    WCHAR **inner_entry) {
    WCHAR *candidate = fv_wide_duplicate(input);
    SIZE_T length;

    *zip_path = NULL;
    *inner_entry = NULL;
    if (candidate == NULL) {
        return FALSE;
    }

    length = (SIZE_T)lstrlenW(candidate);
    while (length > 0) {
        DWORD attributes;

        candidate[length] = L'\0';
        if (fv_is_zip_file(candidate)) {
            attributes = GetFileAttributesW(candidate);
            if (attributes != INVALID_FILE_ATTRIBUTES &&
                (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                const WCHAR *inner = input + length;

                while (*inner == L'\\' || *inner == L'/') {
                    ++inner;
                }
                *zip_path = candidate;
                *inner_entry = fv_wide_duplicate(inner);
                if (*inner_entry == NULL) {
                    fv_heap_free(candidate);
                    *zip_path = NULL;
                    return FALSE;
                }
                return TRUE;
            }
        }

        while (length > 0 && candidate[length - 1] != L'\\' && candidate[length - 1] != L'/') {
            --length;
        }
        while (length > 0 && (candidate[length - 1] == L'\\' || candidate[length - 1] == L'/')) {
            if (length == 3 && candidate[1] == L':') {
                length = 0;
                break;
            }
            --length;
        }
    }

    fv_heap_free(candidate);
    return FALSE;
}

static FLOAT fv_calculate_scale(HWND window, UINT width, UINT height) {
    WINDOWPLACEMENT placement;
    RECT client;
    FLOAT result = 1.0f;
    LONG client_width;
    LONG client_height;

    placement.length = sizeof(placement);
    if (!GetWindowPlacement(window, &placement) || placement.showCmd != SW_MAXIMIZE) {
        return 1.0f;
    }
    if (!GetClientRect(window, &client)) {
        return 1.0f;
    }

    client_width = client.right - client.left;
    client_height = client.bottom - client.top;
    if (client_width <= 0 || client_height <= 0 || width == 0 || height == 0) {
        return 1.0f;
    }
    if (width > (UINT)client_width) {
        result = (FLOAT)client_width / (FLOAT)width;
    }
    if ((FLOAT)height * result > (FLOAT)client_height) {
        result = (FLOAT)client_height / (FLOAT)height;
    }
    return result;
}

static BOOL fv_float_almost_equal(FLOAT left, FLOAT right) {
    FLOAT difference = left - right;

    if (difference < 0.0f) {
        difference = -difference;
    }
    return difference < 0.000001f;
}

static void fv_update_window_title(HWND window) {
    const DecodedImage *image;
    FvWideBuilder title;
    ULONGLONG percentage;

    if (g_cache == NULL || g_current_image >= g_files.count) {
        return;
    }
    image = &g_cache[g_current_image].image;

    title.data = NULL;
    title.length = 0;
    title.capacity = 0;
    if (g_zip_mode) {
        fv_builder_append(&title, ZipArchive_Path(&g_zip));
        fv_builder_append(&title, L" -> ");
    }
    fv_builder_append(&title, g_files.items[g_current_image]);
    if (DecodedImage_IsValid(image) && (g_current_activated || !g_loader_active)) {
        /* Размер исходного кадра: декодированный буфер может быть меньше. */
        fv_builder_append(&title, L" [");
        fv_builder_append_u64(&title, image->source_width);
        fv_builder_append(&title, L"x");
        fv_builder_append_u64(&title, image->source_height);
        fv_builder_append(&title, image->has_alpha ? L" RGBA] " : L" RGB] ");
        percentage = (ULONGLONG)(g_scale * 100.0f + 0.5f);
        fv_builder_append_u64(&title, percentage);
        fv_builder_append(&title, L"%");
    } else {
        /* Кадр ещё загружается (фоновый декод): размер и масштаб неизвестны. */
        fv_builder_append(&title, L" [...]");
    }

    if (title.data != NULL) {
        SetWindowTextW(window, title.data);
    }
    fv_builder_dispose(&title);
}

static void fv_release_entry_bitmap(FvCacheEntry *entry) {
    if (entry->bitmap != NULL) {
        FV_UNKNOWN_VT(entry->bitmap)->Release(entry->bitmap);
        entry->bitmap = NULL;
    }
    entry->bitmap_bytes = 0;
}

static void fv_release_shown_bitmap(void) {
    if (g_shown_bitmap != NULL) {
        FV_UNKNOWN_VT(g_shown_bitmap)->Release(g_shown_bitmap);
        g_shown_bitmap = NULL;
    }
}

/* Все битмапы принадлежат render target: при его пересоздании они недействительны. */
static void fv_release_all_bitmaps(void) {
    SIZE_T index;

    fv_release_shown_bitmap();
    if (g_cache == NULL) {
        return;
    }
    for (index = 0; index < g_files.count; ++index) {
        fv_release_entry_bitmap(&g_cache[index]);
    }
}

/*
 * Вытеснение кадров: сначала битмапы сверх своей части бюджета (кадр остаётся
 * в памяти и при возврате лишь заново загружается в GPU), затем кадры целиком
 * сверх общего бюджета и дальше FV_CACHE_RADIUS. Порядок — от самых дальних
 * от текущего (FvPlan_SelectEvictions); текущий кадр не вытесняется.
 */
static void fv_trim_cache(void) {
    ULONGLONG *bytes = NULL;
    SIZE_T *evicted = NULL;
    SIZE_T count = g_files.count;
    SIZE_T evicted_bitmaps;
    SIZE_T evicted_frames;
    SIZE_T array_bytes;
    SIZE_T index;
    uint64_t remaining = 0;

    if (g_cache == NULL || count == 0 || g_current_image >= count) {
        return;
    }
    if (!fv_size_multiply(count, sizeof(*bytes), &array_bytes)) {
        return;
    }
    bytes = (ULONGLONG *)fv_heap_allocate(array_bytes, FALSE);
    if (!fv_size_multiply(count, sizeof(*evicted), &array_bytes)) {
        fv_heap_free(bytes);
        return;
    }
    evicted = (SIZE_T *)fv_heap_allocate(array_bytes, FALSE);
    if (bytes == NULL || evicted == NULL) {
        fv_heap_free(bytes);
        fv_heap_free(evicted);
        return;
    }

    for (index = 0; index < count; ++index) {
        bytes[index] = g_cache[index].bitmap_bytes;
    }
    evicted_bitmaps = FvPlan_SelectEvictions(
        (const uint64_t *)bytes,
        count,
        g_current_image,
        FV_CACHE_RADIUS,
        g_bitmap_budget,
        evicted,
        count,
        &remaining);
    for (index = 0; index < evicted_bitmaps; ++index) {
        fv_release_entry_bitmap(&g_cache[evicted[index]]);
    }

    for (index = 0; index < count; ++index) {
        bytes[index] = (ULONGLONG)g_cache[index].image.pixels_size +
            (ULONGLONG)g_cache[index].bitmap_bytes;
    }
    evicted_frames = FvPlan_SelectEvictions(
        (const uint64_t *)bytes,
        count,
        g_current_image,
        FV_CACHE_RADIUS,
        g_cache_budget,
        evicted,
        count,
        &remaining);
    for (index = 0; index < evicted_frames; ++index) {
        fv_free_cache_entry(&g_cache[evicted[index]]);
    }

    if (FV_TRACE_ENABLED()) {
        ULONGLONG frames = 0;
        ULONGLONG bitmaps = 0;
        ULONGLONG bitmap_bytes = 0;

        for (index = 0; index < count; ++index) {
            if (DecodedImage_IsValid(&g_cache[index].image)) {
                ++frames;
            }
            if (g_cache[index].bitmap != NULL) {
                ++bitmaps;
                bitmap_bytes += g_cache[index].bitmap_bytes;
            }
        }
        FvTrace_Printf(
            L"cache frames=%I64u bytes=%I64u bitmaps=%I64u bitmap_bytes=%I64u "
            L"evicted_frames=%I64u evicted_bitmaps=%I64u budget=%I64u bitmap_budget=%I64u",
            frames,
            (ULONGLONG)remaining,
            bitmaps,
            bitmap_bytes,
            (ULONGLONG)evicted_frames,
            (ULONGLONG)evicted_bitmaps,
            g_cache_budget,
            g_bitmap_budget);
    }

    fv_heap_free(bytes);
    fv_heap_free(evicted);
}

/*
 * Рамка декодирования нового кадра. В развёрнутом окне масштаб «вписать»
 * не больше вписывания в монитор, поэтому кадр декодируется под пиксельный
 * размер монитора окна: ресайз окна в его пределах передекода не требует. В
 * обычном окне вьювер показывает кадр 1:1 — нужна полная рамка (0x0).
 */
static void fv_decode_box(HWND window, UINT *box_width, UINT *box_height) {
    WINDOWPLACEMENT placement;
    MONITORINFO monitor_info;
    HMONITOR monitor;

    *box_width = 0;
    *box_height = 0;
    placement.length = sizeof(placement);
    if (!GetWindowPlacement(window, &placement) || placement.showCmd != SW_MAXIMIZE) {
        return;
    }
    monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    monitor_info.cbSize = sizeof(monitor_info);
    if (monitor == NULL || !GetMonitorInfoW(monitor, &monitor_info)) {
        return;
    }
    if (monitor_info.rcMonitor.right > monitor_info.rcMonitor.left &&
        monitor_info.rcMonitor.bottom > monitor_info.rcMonitor.top) {
        *box_width = (UINT)(monitor_info.rcMonitor.right - monitor_info.rcMonitor.left);
        *box_height = (UINT)(monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top);
    }
}

/* Пиксели экрана под сторону кадра при масштабе g_scale (с округлением вверх). */
static UINT fv_display_extent(UINT source_extent) {
    FLOAT value = (FLOAT)source_extent * g_scale;

    if (value <= 0.0f) {
        return 0;
    }
    if (value >= 4294967040.0f) {
        return 0xFFFFFFFFU;
    }
    return (UINT)value + ((FLOAT)(UINT)value < value ? 1U : 0U);
}

/*
 * Декодирует кадр index под рамку box (0x0 — полное разрешение) с пределом
 * стороны g_max_bitmap_size. Новый кадр заменяет прежний (и его битмап)
 * только при успехе: неудачное передекодирование оставляет то, что было.
 */
static HRESULT fv_decode_image(SIZE_T index, UINT box_width, UINT box_height, const WCHAR *reason) {
    DecodedImage decoded;
    ImageDecodeOptions options;
    ImageDecodeStats stats;
    HRESULT result;
    ULONGLONG trace_started;

    if (g_cache == NULL || index >= g_files.count) {
        return E_FAIL;
    }
    ZeroMemory(&decoded, sizeof(decoded));
    ZeroMemory(&stats, sizeof(stats));
    ZeroMemory(&options, sizeof(options));
    options.box_width = box_width;
    options.box_height = box_height;
    options.max_dimension = g_max_bitmap_size;
    options.interpolation = IMAGE_DECODER_DEFAULT_INTERPOLATION;
    options.dct_slack_percent = IMAGE_DECODER_DEFAULT_DCT_SLACK_PERCENT;

    if (g_zip_mode) {
        BYTE *data = NULL;
        SIZE_T size = 0;
        ZipIoStats io_before;
        BOOL extracted;

        if (g_zip_entries == NULL) {
            return E_FAIL;
        }
        if (FV_TRACE_ENABLED()) {
            ZipArchive_GetIoStats(&g_zip, &io_before);
        }
        trace_started = FV_TRACE_NOW();
        extracted = ZipArchive_ExtractEntry(
            &g_zip,
            g_zip_entries[index],
            FV_MAX_ZIP_IMAGE_BYTES,
            &data,
            &size);
        if (FV_TRACE_ENABLED()) {
            DWORD error = extracted ? ERROR_SUCCESS : GetLastError();
            ULONGLONG elapsed_us = fv_trace_us_since(trace_started);
            ZipIoStats io_after;

            ZipArchive_GetIoStats(&g_zip, &io_after);
            FvTrace_Printf(
                L"entry-read index=%I64u ms=" FV_MS
                L" bytes=%I64u compressed=%I64u method=%u reads=%I64u read_bytes=%I64u error=%lu thread=ui",
                (ULONGLONG)index,
                FV_MS_ARGS(elapsed_us),
                (ULONGLONG)size,
                g_zip_entries[index]->compressed_size,
                (UINT)g_zip_entries[index]->compression_method,
                io_after.read_calls - io_before.read_calls,
                io_after.read_bytes - io_before.read_bytes,
                error);
        }
        if (!extracted) {
            return E_FAIL;
        }
        /* Буфер записи живёт до возврата: WIC читает его без копии. */
        trace_started = FV_TRACE_NOW();
        result = ImageDecoder_DecodeMemoryEx(data, size, &options, &decoded, &stats);
        ZipArchive_FreeBuffer(data);
    } else {
        trace_started = FV_TRACE_NOW();
        result = ImageDecoder_DecodeFileEx(g_files.items[index], &options, &decoded, &stats);
    }

    if (FV_TRACE_ENABLED()) {
        ULONGLONG elapsed_us = fv_trace_us_since(trace_started);
        ULONGLONG open_us = FvTrace_TicksToMicroseconds(stats.open_ticks);
        ULONGLONG pixels_us = FvTrace_TicksToMicroseconds(stats.pixels_ticks);

        FvTrace_Printf(
            L"decode index=%I64u ms=" FV_MS L" hr=0x%08lX reason=%s width=%u height=%u "
            L"src_width=%u src_height=%u box=%ux%u max=%u dct=1/%u dct_size=%ux%u scaled=%d "
            L"bytes=%I64u open_ms=" FV_MS L" pixels_ms=" FV_MS L" thread=ui",
            (ULONGLONG)index,
            FV_MS_ARGS(elapsed_us),
            (DWORD)result,
            reason,
            SUCCEEDED(result) ? decoded.width : 0U,
            SUCCEEDED(result) ? decoded.height : 0U,
            stats.source_width,
            stats.source_height,
            box_width,
            box_height,
            (UINT)g_max_bitmap_size,
            stats.dct_denominator,
            stats.transform_width,
            stats.transform_height,
            stats.scaled ? 1 : 0,
            SUCCEEDED(result) ? (ULONGLONG)decoded.pixels_size : 0ULL,
            FV_MS_ARGS(open_us),
            FV_MS_ARGS(pixels_us));
    }
    if (FAILED(result) || !DecodedImage_IsValid(&decoded)) {
        DecodedImage_Free(&decoded);
        return FAILED(result) ? result : E_FAIL;
    }

    fv_free_cache_entry(&g_cache[index]);
    g_cache[index].image = decoded;
    return S_OK;
}

/*
 * Ленивое передекодирование текущего кадра: если при текущем масштабе
 * (зум, 1:1, обычное окно) кадр на экране больше декодированного — вплоть до
 * полного разрешения; если декодированный кадр больше предела D2D-битмапа
 * (например, после пересоздания render target) — вниз до предела. Соседи в
 * кэше остаются уменьшенными.
 */
/* Нужен ли текущему кадру передекод под масштаб показа; размер — в display_*. */
static BOOL fv_current_needs_upgrade(UINT *display_width, UINT *display_height) {
    const DecodedImage *image;

    *display_width = 0;
    *display_height = 0;
    if (g_cache == NULL || g_current_image >= g_files.count) {
        return FALSE;
    }
    image = &g_cache[g_current_image].image;
    if (!DecodedImage_IsValid(image)) {
        return FALSE;
    }
    *display_width = fv_display_extent(image->source_width);
    *display_height = fv_display_extent(image->source_height);
    if (!FvPlan_NeedsRedecode(
            image->source_width,
            image->source_height,
            image->width,
            image->height,
            *display_width,
            *display_height,
            g_max_bitmap_size)) {
        return FALSE;
    }
    /* Неудачу не повторяем на каждой перерисовке (панорама): до смены кадра. */
    return g_upgrade_failed_image != g_current_image;
}

/* Фоновая загрузка доступна: поток запущен, а для ZIP есть ссылка на архив. */
static BOOL fv_async_available(void) {
    return g_loader_active && (!g_zip_mode || g_loader_archive != NULL);
}

static void fv_ensure_current_resolution(HWND window) {
    UINT display_width;
    UINT display_height;

    if (!g_current_activated || !fv_current_needs_upgrade(&display_width, &display_height)) {
        return;
    }
    if (fv_async_available()) {
        /*
         * Передекод — в рабочем потоке: до его готовности показывается
         * уменьшенный кадр, растянутый D2D; готовый заменит его в кэше.
         */
        fv_schedule_loads(window);
        return;
    }
    /* Показ больше половины кадра даёт полное разрешение (FvPlan_DecodeTarget). */
    if (SUCCEEDED(fv_decode_image(g_current_image, display_width, display_height, L"upgrade"))) {
        fv_trim_cache();
    } else {
        g_upgrade_failed_image = g_current_image;
    }
}

static void fv_cancel_loads(void) {
    if (g_loader_active) {
        FvLoader_Replace(NULL, 0);
    }
}

/* Байт на кадр по оценке: пиксели под рамку плюс такой же D2D-битмап. */
static ULONGLONG fv_prefetch_estimate(const DecodedImage *image, UINT box_width, UINT box_height) {
    uint32_t width = 0;
    uint32_t height = 0;

    FvPlan_DecodeTarget(
        image->source_width,
        image->source_height,
        box_width,
        box_height,
        g_max_bitmap_size,
        &width,
        &height);
    return (ULONGLONG)width * (ULONGLONG)height * 4ULL * 2ULL;
}

/* Текущие условия prefetch (для подавления повторного заказа вытесненных кадров). */
static FvPlanPrefetchContext fv_prefetch_context(UINT box_width, UINT box_height) {
    FvPlanPrefetchContext context;

    ZeroMemory(&context, sizeof(context));
    context.generation = g_generation;
    context.current = g_current_image;
    context.direction = g_nav_direction;
    context.ahead = g_prefetch_ahead;
    context.behind = g_prefetch_behind;
    context.box_width = box_width;
    context.box_height = box_height;
    context.max_dimension = g_max_bitmap_size;
    context.budget = g_cache_budget;
    return context;
}

/* Условия сменились — пометки «вытеснен сразу после prefetch» больше не действуют. */
static void fv_refresh_prefetch_suppression(const FvPlanPrefetchContext *now) {
    SIZE_T index;

    if (!g_prefetch_suppress_any ||
        FvPlan_PrefetchContextEqual(&g_prefetch_suppress_context, now)) {
        return;
    }
    if (g_cache != NULL) {
        for (index = 0; index < g_files.count; ++index) {
            g_cache[index].prefetch_suppressed = FALSE;
        }
    }
    g_prefetch_suppress_any = FALSE;
}

/*
 * Окно prefetch вокруг текущего кадра по направлению листания, обрезанное
 * бюджетом кэша (FvPlan_FitPrefetch): в requests дописываются задания для
 * кадров, которых ещё нет в кэше. Размер соседа — фактический, если он уже
 * декодировался под эту рамку, иначе оценка по текущему кадру; кадр,
 * вытесненный сразу после prefetch при тех же условиях, не заказывается.
 * Возвращает новое число заданий.
 */
static SIZE_T fv_add_prefetch_requests(
    FvLoadRequest *requests,
    SIZE_T request_count,
    UINT box_width,
    UINT box_height) {
    size_t candidates[2 * FV_PLAN_PREFETCH_MAX];
    size_t candidate_count;
    size_t accepted;
    SIZE_T count = g_files.count;
    SIZE_T array_bytes;
    ULONGLONG *bytes = NULL;
    ULONGLONG *known = NULL;
    size_t *scratch = NULL;
    SIZE_T index;
    FvPlanPrefetchContext context = fv_prefetch_context(box_width, box_height);

    fv_refresh_prefetch_suppression(&context);
    candidate_count = FvPlan_PrefetchOrder(
        count,
        g_current_image,
        g_nav_direction,
        g_prefetch_ahead,
        g_prefetch_behind,
        candidates,
        sizeof(candidates) / sizeof(candidates[0]));
    if (candidate_count == 0 ||
        !fv_size_multiply(count, sizeof(*bytes), &array_bytes)) {
        return request_count;
    }
    bytes = (ULONGLONG *)fv_heap_allocate(array_bytes, FALSE);
    known = (ULONGLONG *)fv_heap_allocate(array_bytes, FALSE);
    scratch = (size_t *)fv_heap_allocate(count * sizeof(*scratch), FALSE);
    if (bytes == NULL || known == NULL || scratch == NULL) {
        fv_heap_free(bytes);
        fv_heap_free(known);
        fv_heap_free(scratch);
        return request_count;
    }
    for (index = 0; index < count; ++index) {
        const FvCacheEntry *entry = &g_cache[index];

        bytes[index] = (ULONGLONG)entry->image.pixels_size + (ULONGLONG)entry->bitmap_bytes;
        known[index] = entry->known_box_width == box_width &&
                entry->known_box_height == box_height
            ? entry->known_bytes
            : 0;
    }
    accepted = FvPlan_FitPrefetch(
        (uint64_t *)bytes,
        count,
        g_current_image,
        FV_CACHE_RADIUS,
        g_cache_budget,
        candidates,
        candidate_count,
        fv_prefetch_estimate(&g_cache[g_current_image].image, box_width, box_height),
        (const uint64_t *)known,
        scratch);
    fv_heap_free(bytes);
    fv_heap_free(known);
    fv_heap_free(scratch);

    for (index = 0; index < accepted && request_count < FV_MAX_LOAD_REQUESTS; ++index) {
        SIZE_T candidate = candidates[index];
        FvLoadRequest *request;

        if (DecodedImage_IsValid(&g_cache[candidate].image) || g_cache[candidate].load_failed ||
            FvPlan_PrefetchSuppressed(
                g_cache[candidate].prefetch_suppressed,
                &g_prefetch_suppress_context,
                &context)) {
            continue;
        }
        request = &requests[request_count++];
        ZeroMemory(request, sizeof(*request));
        request->index = candidate;
        request->generation = g_generation;
        request->reason = FV_PLAN_REASON_PREFETCH;
        request->box_width = box_width;
        request->box_height = box_height;
        request->max_dimension = g_max_bitmap_size;
        if (g_zip_mode) {
            request->archive = g_loader_archive;
            request->entry = g_zip_entries[candidate];
        } else {
            request->path = g_files.items[candidate];
        }
    }
    return request_count;
}

/*
 * План фоновой загрузки — заново после каждого изменения состояния
 * (переход, готовый результат, зум): текущий кадр, если его нет; иначе
 * передекод при зуме и окно prefetch. FvLoader_Replace сам сохраняет уже
 * поставленные задания и отменяет ненужные (кадры, с которых ушли).
 */
static void fv_schedule_loads(HWND window) {
    FvLoadRequest requests[FV_MAX_LOAD_REQUESTS];
    SIZE_T request_count = 0;
    FvCacheEntry *entry;
    UINT box_width;
    UINT box_height;

    if (!fv_async_available()) {
        return;
    }
    if (g_cache == NULL || g_current_image >= g_files.count ||
        (g_zip_mode && g_zip_entries == NULL)) {
        FvLoader_Replace(NULL, 0);
        return;
    }

    fv_decode_box(window, &box_width, &box_height);
    entry = &g_cache[g_current_image];
    if (!DecodedImage_IsValid(&entry->image)) {
        if (!entry->load_failed) {
            FvLoadRequest *request = &requests[request_count++];

            ZeroMemory(request, sizeof(*request));
            request->index = g_current_image;
            request->reason = FV_PLAN_REASON_CURRENT;
            request->box_width = box_width;
            request->box_height = box_height;
        }
        /* Пока текущего кадра нет, соседей не грузим: важен только он. */
    } else {
        UINT display_width;
        UINT display_height;

        if (g_current_activated && fv_current_needs_upgrade(&display_width, &display_height)) {
            FvLoadRequest *request = &requests[request_count++];

            ZeroMemory(request, sizeof(*request));
            request->index = g_current_image;
            request->reason = FV_PLAN_REASON_UPGRADE;
            request->box_width = display_width;
            request->box_height = display_height;
        }
        request_count = fv_add_prefetch_requests(requests, request_count, box_width, box_height);
    }
    /* Общие поля текущего кадра (prefetch заполняет свои сам). */
    if (request_count != 0 && requests[0].index == g_current_image &&
        requests[0].reason != FV_PLAN_REASON_PREFETCH) {
        requests[0].generation = g_generation;
        requests[0].max_dimension = g_max_bitmap_size;
        if (g_zip_mode) {
            requests[0].archive = g_loader_archive;
            requests[0].entry = g_zip_entries[g_current_image];
        } else {
            requests[0].path = g_files.items[g_current_image];
        }
    }
    FvLoader_Replace(requests, request_count);
}

/* Битмап кадра из кэша (создаётся при необходимости); NULL — не удалось. */
static ID2D1Bitmap *fv_ensure_entry_bitmap(SIZE_T index, const WCHAR *reason);

static void fv_apply_actual_size_toggle(HWND window, const DecodedImage *image) {
    /* 1:1 — относительно исходного кадра; нужное разрешение догрузит отрисовка. */
    if (fv_float_almost_equal(g_scale, 1.0f)) {
        FLOAT fitted = fv_calculate_scale(window, image->source_width, image->source_height);

        if (!fv_float_almost_equal(fitted, 1.0f)) {
            g_scale = fitted;
        }
    } else {
        g_scale = 1.0f;
    }
}

static void fv_apply_zoom_step(BOOL increase) {
    if (increase) {
        if (g_scale <= 10000.0f) {
            g_scale *= 2.0f;
        }
    } else if (g_scale >= 0.00001f) {
        g_scale *= 0.5f;
    }
}

/*
 * Показ текущего кадра, который уже есть в кэше: масштаб, заголовок и размер
 * окна — по исходному размеру кадра; зум и 1:1, нажатые до готовности кадра,
 * применяются здесь в порядке «1:1, затем зум».
 */
static void fv_activate_current(HWND window, BOOL repaint, const WCHAR *source) {
    DecodedImage *image = &g_cache[g_current_image].image;
    WINDOWPLACEMENT placement;

    g_current_activated = TRUE;
    g_scale = fv_calculate_scale(window, image->source_width, image->source_height);
    if (g_deferred_actual_size) {
        fv_apply_actual_size_toggle(window, image);
    }
    while (g_deferred_zoom_steps > 0) {
        fv_apply_zoom_step(TRUE);
        --g_deferred_zoom_steps;
    }
    while (g_deferred_zoom_steps < 0) {
        fv_apply_zoom_step(FALSE);
        ++g_deferred_zoom_steps;
    }
    g_deferred_actual_size = FALSE;
    fv_update_window_title(window);
    if (FV_TRACE_ENABLED() && g_trace_frame_pending) {
        ULONGLONG ready_us = fv_trace_us_since(g_trace_frame_started);

        FvTrace_Printf(
            L"frame-ready index=%I64u since-request-ms=" FV_MS L" source=%s",
            (ULONGLONG)g_current_image,
            FV_MS_ARGS(ready_us),
            source);
    }

    placement.length = sizeof(placement);
    if (GetWindowPlacement(window, &placement) && placement.showCmd != SW_MAXIMIZE) {
        RECT rectangle;

        rectangle.left = 0;
        rectangle.top = 0;
        rectangle.right = (LONG)image->source_width;
        rectangle.bottom = (LONG)image->source_height;
        AdjustWindowRect(&rectangle, WS_OVERLAPPEDWINDOW, FALSE);
        SetWindowPos(
            window,
            NULL,
            0,
            0,
            rectangle.right - rectangle.left,
            rectangle.bottom - rectangle.top,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOREDRAW);
    }

    if (repaint) {
        InvalidateRect(window, NULL, FALSE);
    }
}

/*
 * Переход к кадру g_current_image. Кадр в кэше показывается сразу. Иначе при
 * фоновой загрузке индекс и заголовок обновляются немедленно, кадр ставится
 * первым в очередь, а до его готовности на экране остаётся прежний кадр (без
 * серого фона и без перерисовки); показ — в fv_handle_loaded. Без рабочего
 * потока (первый кадр при запуске, сбой потока) — синхронный декод.
 */
static void fv_set_current_image(HWND window, BOOL repaint) {
    DecodedImage *image;

    if (g_cache == NULL || g_current_image >= g_files.count) {
        return;
    }

    g_has_image_shift = FALSE;
    g_image_shift.x = 0;
    g_image_shift.y = 0;
    g_upgrade_failed_image = ~(SIZE_T)0;
    g_current_activated = FALSE;
    g_deferred_zoom_steps = 0;
    g_deferred_actual_size = FALSE;
    /* Явный переход к кадру — новая попытка, даже если фоновая не удалась. */
    g_cache[g_current_image].load_failed = FALSE;
    image = &g_cache[g_current_image].image;
    if (FV_TRACE_ENABLED()) {
        g_trace_frame_started = FvTrace_Now();
        g_trace_frame_pending = TRUE;
        FvTrace_Printf(
            L"frame-request index=%I64u cached=%d bitmap=%d",
            (ULONGLONG)g_current_image,
            DecodedImage_IsValid(image) ? 1 : 0,
            g_cache[g_current_image].bitmap != NULL ? 1 : 0);
    }
    if (!DecodedImage_IsValid(image)) {
        UINT box_width;
        UINT box_height;

        if (fv_async_available()) {
            fv_update_window_title(window);
            fv_schedule_loads(window);
            if (FV_TRACE_ENABLED()) {
                FvTrace_Printf(
                    L"frame-pending index=%I64u showing=%s",
                    (ULONGLONG)g_current_image,
                    g_shown_bitmap != NULL ? L"previous" : L"none");
            }
            return;
        }

        fv_decode_box(window, &box_width, &box_height);
        if (FAILED(fv_decode_image(g_current_image, box_width, box_height, L"current")) ||
            !DecodedImage_IsValid(image)) {
            MessageBoxW(window, L"Не удалось загрузить изображение", L"Ошибка", MB_OK | MB_ICONERROR);
            if (repaint) {
                InvalidateRect(window, NULL, FALSE);
            }
            return;
        }
        fv_trim_cache();
        fv_activate_current(window, repaint, L"sync");
    } else {
        fv_trim_cache();
        fv_activate_current(window, repaint, L"cache");
    }
    fv_schedule_loads(window);
}

/* Результат одного задания рабочего потока (UI-поток). */
static void fv_apply_loaded_job(
    FvLoadJob *job,
    BOOL *current_ready,
    BOOL *current_upgraded,
    BOOL *current_failed,
    BOOL *accepted,
    SIZE_T *prefetched,
    SIZE_T *prefetched_count,
    SIZE_T prefetched_capacity) {
    const DecodedImage *cached = NULL;
    BOOL succeeded = SUCCEEDED(job->result) && !job->cancelled &&
        DecodedImage_IsValid(&job->image);
    int disposition;

    if (g_cache != NULL && job->index < g_files.count && job->generation == g_generation) {
        cached = &g_cache[job->index].image;
    }
    disposition = FvPlan_ResultDisposition(
        job->generation,
        g_generation,
        job->index,
        g_cache != NULL ? g_files.count : 0,
        succeeded,
        cached != NULL && DecodedImage_IsValid(cached) ? cached->width : 0U,
        cached != NULL && DecodedImage_IsValid(cached) ? cached->height : 0U,
        succeeded ? job->image.width : 0U,
        succeeded ? job->image.height : 0U,
        g_max_bitmap_size);
    if (FV_TRACE_ENABLED()) {
        static const WCHAR *const actions[] = {L"accept", L"stale", L"redundant", L"failed"};

        FvTrace_Printf(
            L"job-result index=%I64u reason=%s gen=%I64u action=%s cancelled=%d current=%I64u",
            (ULONGLONG)job->index,
            FvLoader_ReasonName(job->reason),
            job->generation,
            disposition >= 0 && disposition <= 3 ? actions[disposition] : L"?",
            job->cancelled ? 1 : 0,
            (ULONGLONG)g_current_image);
    }

    if (disposition == FV_PLAN_RESULT_ACCEPT) {
        FvCacheEntry *entry = &g_cache[job->index];
        BOOL was_valid = DecodedImage_IsValid(&entry->image);

        fv_free_cache_entry(entry);
        /* Пиксели переходят в кэш: задание их больше не освобождает. */
        entry->image = job->image;
        ZeroMemory(&job->image, sizeof(job->image));
        entry->load_failed = FALSE;
        if (job->reason != FV_PLAN_REASON_UPGRADE) {
            /* Пиксели и такой же D2D-битмап — как в бюджете кэша. */
            entry->known_bytes = (ULONGLONG)entry->image.pixels_size * 2ULL;
            entry->known_box_width = job->box_width;
            entry->known_box_height = job->box_height;
        }
        *accepted = TRUE;
        if (job->index == g_current_image) {
            if (!g_current_activated) {
                *current_ready = TRUE;
            } else if (was_valid) {
                *current_upgraded = TRUE;
            }
        } else {
            if (*prefetched_count < prefetched_capacity) {
                prefetched[(*prefetched_count)++] = job->index;
            }
            if (g_render_target != NULL) {
                /* Соседний кадр — сразу в GPU, пока UI свободен: показ без загрузки. */
                fv_ensure_entry_bitmap(job->index, FvLoader_ReasonName(job->reason));
            }
        }
    } else if (disposition == FV_PLAN_RESULT_REDUNDANT &&
        job->reason == FV_PLAN_REASON_UPGRADE &&
        job->index == g_current_image) {
        /*
         * Передекод не дал кадра крупнее: повторять его на каждой
         * перерисовке бессмысленно (как и неудачу) — до смены кадра.
         */
        g_upgrade_failed_image = job->index;
    } else if (disposition == FV_PLAN_RESULT_FAILED && !job->cancelled) {
        if (job->reason == FV_PLAN_REASON_UPGRADE) {
            if (job->index == g_current_image) {
                g_upgrade_failed_image = job->index;
            }
        } else {
            g_cache[job->index].load_failed = TRUE;
            if (job->index == g_current_image && !g_current_activated) {
                *current_failed = TRUE;
            }
        }
    }
}

/* WM_FV_LOADED: забирает готовые задания, показывает текущий кадр, планирует дальше. */
static void fv_handle_loaded(HWND window) {
    FvLoadJob *job = FvLoader_TakeCompleted();
    BOOL current_ready = FALSE;
    BOOL current_upgraded = FALSE;
    BOOL current_failed = FALSE;
    BOOL accepted = FALSE;
    SIZE_T prefetched[FV_LOADER_MAX_QUEUE];
    SIZE_T prefetched_count = 0;
    SIZE_T index;

    while (job != NULL) {
        FvLoadJob *next = job->next;

        fv_apply_loaded_job(
            job,
            &current_ready,
            &current_upgraded,
            &current_failed,
            &accepted,
            prefetched,
            &prefetched_count,
            sizeof(prefetched) / sizeof(prefetched[0]));
        FvLoader_FreeJob(job);
        job = next;
    }
    if (accepted) {
        fv_trim_cache();
    }
    /*
     * Сосед, вытесненный тем же trim сразу после декода, при тех же условиях
     * заказывался бы снова и снова (бесконечный цикл чтения и декода): помечаем его.
     * Основная защита — фактический размер в FvPlan_FitPrefetch; пометка
     * страхует от расхождения оценки и бюджета.
     */
    if (prefetched_count != 0 && g_cache != NULL) {
        UINT box_width;
        UINT box_height;
        FvPlanPrefetchContext context;

        fv_decode_box(window, &box_width, &box_height);
        context = fv_prefetch_context(box_width, box_height);
        fv_refresh_prefetch_suppression(&context);
        for (index = 0; index < prefetched_count; ++index) {
            FvCacheEntry *entry = &g_cache[prefetched[index]];

            if (DecodedImage_IsValid(&entry->image)) {
                continue;
            }
            entry->prefetch_suppressed = TRUE;
            g_prefetch_suppress_context = context;
            g_prefetch_suppress_any = TRUE;
            if (FV_TRACE_ENABLED()) {
                FvTrace_Printf(
                    L"prefetch-suppressed index=%I64u known_bytes=%I64u current=%I64u",
                    (ULONGLONG)prefetched[index],
                    entry->known_bytes,
                    (ULONGLONG)g_current_image);
            }
        }
    }
    if (current_ready &&
        g_cache != NULL &&
        g_current_image < g_files.count &&
        DecodedImage_IsValid(&g_cache[g_current_image].image)) {
        fv_activate_current(window, TRUE, L"loader");
    } else if (current_upgraded) {
        InvalidateRect(window, NULL, FALSE);
    } else if (current_failed) {
        /* Как при синхронном декоде: вместо кадра — фон и сообщение (прежний кадр не выдаём за текущий). */
        fv_release_shown_bitmap();
        InvalidateRect(window, NULL, FALSE);
        MessageBoxW(window, L"Не удалось загрузить изображение", L"Ошибка", MB_OK | MB_ICONERROR);
    }
    fv_schedule_loads(window);
}

static void fv_release_render_target(void) {
    /* D2DERR_RECREATE_TARGET и выход: все битмапы кэша недействительны. */
    fv_release_all_bitmaps();
    if (g_render_target != NULL) {
        FV_TARGET_VT(g_render_target)->Release(g_render_target);
        g_render_target = NULL;
    }
}

static HRESULT fv_create_render_target(HWND window) {
    D2D1_RENDER_TARGET_PROPERTIES properties;
    D2D1_HWND_RENDER_TARGET_PROPERTIES window_properties;
    RECT client;
    HRESULT result;

    if (g_render_target != NULL) {
        return S_OK;
    }
    if (g_d2d_factory == NULL || !GetClientRect(window, &client)) {
        return E_FAIL;
    }

    properties.type = D2D1_RENDER_TARGET_TYPE_HARDWARE;
    properties.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    properties.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    properties.dpiX = 96.0f;
    properties.dpiY = 96.0f;
    properties.usage = D2D1_RENDER_TARGET_USAGE_NONE;
    properties.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;

    window_properties.hwnd = window;
    window_properties.pixelSize.width = (UINT32)(client.right - client.left > 0
        ? client.right - client.left
        : 1);
    window_properties.pixelSize.height = (UINT32)(client.bottom - client.top > 0
        ? client.bottom - client.top
        : 1);
    window_properties.presentOptions = D2D1_PRESENT_OPTIONS_IMMEDIATELY;

    result = FV_FACTORY_VT(g_d2d_factory)->CreateHwndRenderTarget(
        g_d2d_factory,
        &properties,
        &window_properties,
        &g_render_target);
    if (FAILED(result)) {
        properties.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
        result = FV_FACTORY_VT(g_d2d_factory)->CreateHwndRenderTarget(
            g_d2d_factory,
            &properties,
            &window_properties,
            &g_render_target);
        OutputDebugStringW(
            L"GPU render target creation failed, falling back to software rendering.\n");
    }
    if (SUCCEEDED(result) && g_render_target != NULL) {
        UINT32 maximum = FV_TARGET_VT(g_render_target)->GetMaximumBitmapSize(g_render_target);

        /* Кадры больше предела не рисуются: декодируем их под предел. */
        g_max_bitmap_size = maximum != 0 ? maximum : FV_DEFAULT_MAX_BITMAP_SIZE;
        if (FV_TRACE_ENABLED()) {
            FvTrace_Printf(
                L"render-target type=%s max_bitmap=%u",
                properties.type == D2D1_RENDER_TARGET_TYPE_HARDWARE ? L"hardware" : L"default",
                (UINT)g_max_bitmap_size);
        }
    }
    return result;
}

static ID2D1Bitmap *fv_create_d2d_bitmap(const DecodedImage *image) {
    D2D1_BITMAP_PROPERTIES properties;
    D2D1_SIZE_U size;
    ID2D1Bitmap *bitmap = NULL;

    properties.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    properties.pixelFormat.alphaMode = image->has_alpha
        ? D2D1_ALPHA_MODE_PREMULTIPLIED
        : D2D1_ALPHA_MODE_IGNORE;
    properties.dpiX = 96.0f;
    properties.dpiY = 96.0f;
    size.width = image->width;
    size.height = image->height;

    if (FAILED(FV_TARGET_VT(g_render_target)->CreateBitmap(
            g_render_target,
            size,
            image->pixels,
            image->stride,
            &properties,
            &bitmap))) {
        return NULL;
    }
    return bitmap;
}

static ID2D1Bitmap *fv_ensure_entry_bitmap(SIZE_T index, const WCHAR *reason) {
    FvCacheEntry *entry;
    ULONGLONG trace_started;

    if (g_render_target == NULL || g_cache == NULL || index >= g_files.count) {
        return NULL;
    }
    entry = &g_cache[index];
    if (entry->bitmap != NULL || !DecodedImage_IsValid(&entry->image)) {
        return entry->bitmap;
    }
    trace_started = FV_TRACE_NOW();
    entry->bitmap = fv_create_d2d_bitmap(&entry->image);
    entry->bitmap_bytes = entry->bitmap != NULL
        ? (SIZE_T)entry->image.width * (SIZE_T)entry->image.height * 4U
        : 0;
    if (FV_TRACE_ENABLED()) {
        ULONGLONG elapsed_us = fv_trace_us_since(trace_started);

        FvTrace_Printf(
            L"create-bitmap index=%I64u ms=" FV_MS L" ok=%d width=%u height=%u bytes=%I64u reason=%s",
            (ULONGLONG)index,
            FV_MS_ARGS(elapsed_us),
            entry->bitmap != NULL ? 1 : 0,
            entry->image.width,
            entry->image.height,
            (ULONGLONG)entry->bitmap_bytes,
            reason);
    }
    return entry->bitmap;
}

static void fv_draw_image(HWND window) {
    const DecodedImage *image = NULL;
    ID2D1Bitmap *bitmap = NULL;
    D2D1_COLOR_F background;
    HRESULT result;
    BOOL image_drawn = FALSE;
    ULONGLONG trace_started;

    if (FAILED(fv_create_render_target(window))) {
        return;
    }
    if ((FV_TARGET_VT(g_render_target)->CheckWindowState(g_render_target) &
         D2D1_WINDOW_STATE_OCCLUDED) != 0) {
        return;
    }

    fv_ensure_current_resolution(window);
    if (g_cache != NULL && g_current_image < g_files.count &&
        DecodedImage_IsValid(&g_cache[g_current_image].image) &&
        (g_current_activated || !g_loader_active)) {
        FvCacheEntry *entry = &g_cache[g_current_image];

        image = &entry->image;
        if (entry->bitmap == NULL) {
            if (fv_ensure_entry_bitmap(g_current_image, L"draw") != NULL) {
                fv_trim_cache();
            }
        } else if (FV_TRACE_ENABLED() && g_trace_frame_pending) {
            /* Только на первой отрисовке кадра: панорама перерисовывает часто. */
            FvTrace_Printf(
                L"bitmap-cached index=%I64u width=%u height=%u",
                (ULONGLONG)g_current_image,
                image->width,
                image->height);
        }
        bitmap = entry->bitmap;
    }

    background.r = 0.66f;
    background.g = 0.66f;
    background.b = 0.66f;
    background.a = 1.0f;

    FV_TARGET_VT(g_render_target)->BeginDraw(g_render_target);
    FV_TARGET_VT(g_render_target)->Clear(g_render_target, &background);

    if (bitmap != NULL) {
        RECT client;

        if (GetClientRect(window, &client)) {
            FLOAT client_width = (FLOAT)(client.right - client.left);
            FLOAT client_height = (FLOAT)(client.bottom - client.top);
            /*
             * Геометрия — по исходному размеру кадра: декодированный битмап
             * (под экран) растягивается D2D на тот же прямоугольник.
             */
            FLOAT scaled_width = (FLOAT)image->source_width * g_scale;
            FLOAT scaled_height = (FLOAT)image->source_height * g_scale;
            FLOAT x = (client_width - scaled_width) / 2.0f;
            FLOAT y = (client_height - scaled_height) / 2.0f;
            D2D1_RECT_F destination;

            if (x < 0.0f) {
                x = 0.0f;
            }
            if (y < 0.0f) {
                y = 0.0f;
            }
            if (g_has_image_shift) {
                x += (FLOAT)g_image_shift.x;
                y += (FLOAT)g_image_shift.y;
            }

            destination.left = x;
            destination.top = y;
            destination.right = x + scaled_width;
            destination.bottom = y + scaled_height;
            FV_TARGET_VT(g_render_target)->DrawBitmap(
                g_render_target,
                bitmap,
                &destination,
                1.0f,
                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                NULL);
            image_drawn = TRUE;
            /* Снимок показанного кадра: его рисуем, пока грузится следующий. */
            if (g_shown_bitmap != bitmap) {
                fv_release_shown_bitmap();
                FV_UNKNOWN_VT(bitmap)->AddRef(bitmap);
                g_shown_bitmap = bitmap;
            }
            g_shown_rect = destination;
        }
    } else if (g_shown_bitmap != NULL) {
        /*
         * Текущий кадр ещё декодируется: прежний кадр остаётся на месте
         * (перерисовка окна при перекрытии/ресайзе не даёт серого «мигания»).
         * В трассе это не frame-shown — это не текущий кадр.
         */
        FV_TARGET_VT(g_render_target)->DrawBitmap(
            g_render_target,
            g_shown_bitmap,
            &g_shown_rect,
            1.0f,
            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
            NULL);
    }

    trace_started = FV_TRACE_NOW();
    result = FV_TARGET_VT(g_render_target)->EndDraw(g_render_target, NULL, NULL);
    if (FV_TRACE_ENABLED()) {
        ULONGLONG elapsed_us = fv_trace_us_since(trace_started);

        FvTrace_Printf(
            L"end-draw index=%I64u ms=" FV_MS L" hr=0x%08lX drawn=%d",
            (ULONGLONG)g_current_image,
            FV_MS_ARGS(elapsed_us),
            (DWORD)result,
            image_drawn ? 1 : 0);
        if (SUCCEEDED(result) && image_drawn && g_trace_frame_pending) {
            ULONGLONG frame_us = fv_trace_us_since(g_trace_frame_started);

            g_trace_frame_pending = FALSE;
            FvTrace_Printf(
                L"frame-shown index=%I64u since-request-ms=" FV_MS,
                (ULONGLONG)g_current_image,
                FV_MS_ARGS(frame_us));
            if (!g_trace_first_paint_done) {
                g_trace_first_paint_done = TRUE;
                FvTrace_Printf(L"first-paint index=%I64u", (ULONGLONG)g_current_image);
            }
        }
    }
    if (result == D2DERR_RECREATE_TARGET) {
        fv_release_render_target();
    }
}

static void fv_previous_image(HWND window) {
    if (g_block_navigation || g_files.count == 0 || g_current_image == 0) {
        return;
    }
    --g_current_image;
    g_nav_direction = -1;
    fv_set_current_image(window, TRUE);
}

static void fv_next_image(HWND window) {
    if (g_block_navigation || g_current_image + 1 >= g_files.count) {
        return;
    }
    ++g_current_image;
    g_nav_direction = 1;
    fv_set_current_image(window, TRUE);
}

/* Кадр ещё грузится: зум и 1:1 применятся при его показе (fv_activate_current). */
static BOOL fv_current_pending(void) {
    return g_loader_active && !g_current_activated &&
        g_cache != NULL && g_current_image < g_files.count;
}

static void fv_scale_image(HWND window, BOOL increase) {
    if (g_files.count == 0) {
        return;
    }
    if (fv_current_pending()) {
        g_deferred_zoom_steps += increase ? 1 : -1;
        return;
    }
    if (increase) {
        if (g_scale > 10000.0f) {
            return;
        }
        g_scale *= 2.0f;
    } else {
        if (g_scale < 0.00001f) {
            return;
        }
        g_scale *= 0.5f;
    }
    fv_update_window_title(window);
    InvalidateRect(window, NULL, !increase);
}

static void fv_toggle_actual_size(HWND window) {
    const DecodedImage *image;
    FLOAT previous_scale = g_scale;

    if (fv_current_pending()) {
        g_deferred_actual_size = !g_deferred_actual_size;
        return;
    }
    if (g_cache == NULL || g_current_image >= g_files.count ||
        !DecodedImage_IsValid(&g_cache[g_current_image].image)) {
        return;
    }
    image = &g_cache[g_current_image].image;

    /* 1:1 — относительно исходного кадра; нужное разрешение догрузит отрисовка. */
    fv_apply_actual_size_toggle(window, image);
    if (fv_float_almost_equal(previous_scale, g_scale)) {
        return;
    }

    fv_update_window_title(window);
    InvalidateRect(window, NULL, TRUE);
}

static HRESULT fv_get_ui_object_of_file(
    HWND window,
    const WCHAR *path,
    REFIID interface_id,
    void **result) {
    PIDLIST_ABSOLUTE item_id = NULL;
    SFGAOF attributes = 0;
    IShellFolder *parent = NULL;
    PCUITEMID_CHILD child = NULL;
    HRESULT status;

    *result = NULL;
    status = SHParseDisplayName(path, NULL, &item_id, 0, &attributes);
    if (SUCCEEDED(status)) {
        status = SHBindToParent(
            item_id,
            &IID_IShellFolder,
            (void **)&parent,
            &child);
        if (SUCCEEDED(status)) {
            status = IShellFolder_GetUIObjectOf(
                parent,
                window,
                1,
                &child,
                interface_id,
                NULL,
                result);
            IShellFolder_Release(parent);
        }
        CoTaskMemFree((void *)item_id);
    }
    return status;
}

static void fv_copy_current_file(HWND window) {
    IDataObject *data_object = NULL;
    HRESULT status;

    if (g_zip_mode) {
        MessageBeep(MB_ICONWARNING);
        return;
    }
    if (g_current_image >= g_files.count) {
        return;
    }

    status = fv_get_ui_object_of_file(
        window,
        g_files.items[g_current_image],
        &IID_IDataObject,
        (void **)&data_object);
    if (SUCCEEDED(status)) {
        status = OleSetClipboard(data_object);
    }
    if (SUCCEEDED(status)) {
        status = OleFlushClipboard();
    }
    if (data_object != NULL) {
        IDataObject_Release(data_object);
    }
    if (FAILED(status)) {
        MessageBeep(MB_ICONERROR);
    }
}

static void fv_switch_zip_file(HWND window, int direction) {
    FvStringList zip_files;
    WCHAR *archive_path;
    WCHAR *directory;
    WCHAR *next_archive = NULL;
    SIZE_T current_index = 0;
    SIZE_T index;
    BOOL found = FALSE;

    if (g_block_navigation || !g_zip_mode) {
        return;
    }

    fv_string_list_init(&zip_files);
    archive_path = fv_wide_duplicate(ZipArchive_Path(&g_zip));
    if (archive_path == NULL) {
        return;
    }
    directory = fv_parent_path(archive_path);
    if (directory == NULL || !fv_enumerate_matching_files(directory, TRUE, &zip_files)) {
        fv_heap_free(directory);
        fv_heap_free(archive_path);
        fv_string_list_clear(&zip_files);
        return;
    }
    fv_heap_free(directory);
    fv_string_list_sort(&zip_files);

    for (index = 0; index < zip_files.count; ++index) {
        if (lstrcmpiW(zip_files.items[index], archive_path) == 0) {
            current_index = index;
            found = TRUE;
            break;
        }
    }

    if (found) {
        if (direction < 0 && current_index > 0) {
            next_archive = fv_wide_duplicate(zip_files.items[current_index - 1]);
        } else if (direction > 0 && current_index + 1 < zip_files.count) {
            next_archive = fv_wide_duplicate(zip_files.items[current_index + 1]);
        }
    }

    fv_heap_free(archive_path);
    fv_string_list_clear(&zip_files);
    if (next_archive == NULL) {
        return;
    }

    if (fv_build_zip_file_list(next_archive, NULL, TRUE)) {
        /* Новый архив открывается с первого кадра: листать будут вперёд. */
        g_nav_direction = 1;
        fv_set_current_image(window, TRUE);
    }
    fv_heap_free(next_archive);
}

static LONG fv_pan_limit(FLOAT scaled_size, LONG client_size) {
    FLOAT difference = scaled_size - (FLOAT)client_size;

    if (difference <= 0.0f) {
        return 0;
    }
    if (difference >= 2147483647.0f) {
        return 2147483647L;
    }
    return (LONG)difference;
}

static LONG fv_clamp_shift(LONGLONG value, LONG limit) {
    if (value > 0) {
        return 0;
    }
    if (value < -(LONGLONG)limit) {
        return -limit;
    }
    return (LONG)value;
}

static LRESULT CALLBACK FvWindowProc(
    HWND window,
    UINT message,
    WPARAM wparam,
    LPARAM lparam) {
    switch (message) {
    case WM_CREATE:
        return 0;

    case WM_MOUSEWHEEL:
        if (GET_WHEEL_DELTA_WPARAM(wparam) > 0) {
            fv_previous_image(window);
        } else if (GET_WHEEL_DELTA_WPARAM(wparam) < 0) {
            fv_next_image(window);
        }
        return 0;

    case WM_KEYDOWN: {
        UINT key = (UINT)(BYTE)wparam;
        BOOL control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

        switch (key) {
        case VK_LEFT:
        case VK_NUMPAD8:
            fv_previous_image(window);
            return 0;

        case VK_RIGHT:
        case VK_NUMPAD2:
            fv_next_image(window);
            return 0;

        case VK_UP:
            if (control) {
                fv_scale_image(window, TRUE);
            } else if (g_zip_mode) {
                fv_switch_zip_file(window, -1);
            }
            return 0;

        case VK_DOWN:
            if (control) {
                fv_scale_image(window, FALSE);
            } else if (g_zip_mode) {
                fv_switch_zip_file(window, 1);
            }
            return 0;

        case VK_PRIOR:
            if (g_zip_mode) {
                fv_switch_zip_file(window, -1);
            } else {
                fv_previous_image(window);
            }
            return 0;

        case VK_NEXT:
            if (g_zip_mode) {
                fv_switch_zip_file(window, 1);
            } else {
                fv_next_image(window);
            }
            return 0;

        case VK_NUMPAD7:
        case VK_HOME:
            if (!g_block_navigation && g_files.count != 0) {
                g_current_image = 0;
                g_nav_direction = 1;
                fv_set_current_image(window, TRUE);
            }
            return 0;

        case VK_NUMPAD1:
        case VK_END:
            if (!g_block_navigation && g_files.count != 0) {
                g_current_image = g_files.count - 1;
                g_nav_direction = -1;
                fv_set_current_image(window, TRUE);
            }
            return 0;

        case VK_ESCAPE:
            PostQuitMessage(0);
            return 0;

        case VK_ADD:
            fv_scale_image(window, TRUE);
            return 0;

        case VK_SUBTRACT:
            fv_scale_image(window, FALSE);
            return 0;

        case L'0':
        case VK_MULTIPLY:
            fv_toggle_actual_size(window);
            return 0;

        case VK_INSERT:
        case L'C':
            if (control) {
                fv_copy_current_file(window);
                return 0;
            }
            break;
        }
        break;
    }

    case WM_LBUTTONDOWN:
        if (g_files.count == 0) {
            return 0;
        }
        SetCapture(window);
        g_block_navigation = TRUE;
        SetCursor(LoadCursorW(NULL, IDC_HAND));
        g_drag_origin = MAKEPOINTS(lparam);
        return 0;

    case WM_MOUSEMOVE:
        if ((wparam & MK_LBUTTON) != 0 &&
            g_cache != NULL &&
            g_current_image < g_files.count &&
            DecodedImage_IsValid(&g_cache[g_current_image].image)) {
            const DecodedImage *image = &g_cache[g_current_image].image;
            RECT client;

            if (GetClientRect(window, &client)) {
                POINTS current = MAKEPOINTS(lparam);
                LONG limit_x = fv_pan_limit(
                    (FLOAT)image->source_width * g_scale,
                    client.right - client.left);
                LONG limit_y = fv_pan_limit(
                    (FLOAT)image->source_height * g_scale,
                    client.bottom - client.top);
                LONG old_x = g_image_shift.x;
                LONG old_y = g_image_shift.y;
                LONG delta_x = (LONG)current.x - (LONG)g_drag_origin.x;
                LONG delta_y = (LONG)current.y - (LONG)g_drag_origin.y;

                if (!g_has_image_shift) {
                    g_image_shift.x = 0;
                    g_image_shift.y = 0;
                    g_has_image_shift = TRUE;
                }
                g_image_shift.x = fv_clamp_shift(
                    (LONGLONG)g_image_shift.x + delta_x,
                    limit_x);
                g_image_shift.y = fv_clamp_shift(
                    (LONGLONG)g_image_shift.y + delta_y,
                    limit_y);

                ScrollWindowEx(
                    window,
                    g_image_shift.x - old_x,
                    g_image_shift.y - old_y,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    SW_INVALIDATE | SW_ERASE);
                g_drag_origin = current;
            }
        }
        return 0;

    case WM_LBUTTONUP:
        SetCursor(LoadCursorW(NULL, IDC_ARROW));
        g_block_navigation = FALSE;
        ReleaseCapture();
        return 0;

    case WM_CAPTURECHANGED:
        g_block_navigation = FALSE;
        SetCursor(LoadCursorW(NULL, IDC_ARROW));
        return 0;

    case WM_SIZE:
        if (g_render_target != NULL) {
            D2D1_SIZE_U size;
            HRESULT status;

            size.width = LOWORD(lparam);
            size.height = HIWORD(lparam);
            if (size.width == 0) {
                size.width = 1;
            }
            if (size.height == 0) {
                size.height = 1;
            }
            status = FV_TARGET_VT(g_render_target)->Resize(g_render_target, &size);
            if (status == D2DERR_RECREATE_TARGET) {
                fv_release_render_target();
            }
        }
        return 0;

    case WM_FV_LOADED:
        fv_handle_loaded(window);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT paint;

        BeginPaint(window, &paint);
        fv_draw_image(window);
        EndPaint(window, &paint);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(window, message, wparam, lparam);
}

static BOOL fv_choose_root(WCHAR **root) {
    LPWSTR *arguments;
    int argument_count = 0;
    WCHAR *candidate;

    *root = NULL;
    arguments = CommandLineToArgvW(GetCommandLineW(), &argument_count);
    if (arguments != NULL && argument_count == 2) {
        candidate = fv_full_path(arguments[1]);
    } else {
        candidate = fv_current_directory();
    }
    if (arguments != NULL) {
        LocalFree(arguments);
    }

    *root = candidate;
    return candidate != NULL;
}

/*
 * Диагностическая настройка окна prefetch для замеров:
 * FOLDERVIEW_PREFETCH=<вперёд>,<назад> (каждое 0..FV_PLAN_PREFETCH_MAX).
 * Без переменной — FV_PLAN_PREFETCH_AHEAD/BEHIND.
 */
static void fv_read_prefetch_setting(void) {
    WCHAR value[32];
    DWORD length = GetEnvironmentVariableW(
        L"FOLDERVIEW_PREFETCH",
        value,
        (DWORD)(sizeof(value) / sizeof(value[0])));
    SIZE_T numbers[2] = {0, 0};
    SIZE_T parsed = 0;
    BOOL digits = FALSE;
    DWORD index;

    if (length != 0 && length < (DWORD)(sizeof(value) / sizeof(value[0]))) {
        for (index = 0; index <= length && parsed < 2; ++index) {
            WCHAR character = index < length ? value[index] : L',';

            if (character >= L'0' && character <= L'9') {
                numbers[parsed] = numbers[parsed] * 10U + (SIZE_T)(character - L'0');
                if (numbers[parsed] > FV_PLAN_PREFETCH_MAX) {
                    numbers[parsed] = FV_PLAN_PREFETCH_MAX;
                }
                digits = TRUE;
            } else if (character == L',' && digits) {
                ++parsed;
                digits = FALSE;
            } else {
                parsed = 0;
                break;
            }
        }
        if (parsed == 2) {
            g_prefetch_ahead = numbers[0];
            g_prefetch_behind = numbers[1];
        }
    }
    if (FV_TRACE_ENABLED()) {
        FvTrace_Printf(
            L"prefetch ahead=%I64u behind=%I64u",
            (ULONGLONG)g_prefetch_ahead,
            (ULONGLONG)g_prefetch_behind);
    }
}

static int fv_application_main(void) {
    HRESULT status;
    BOOL ole_initialized = FALSE;
    BOOL decoder_initialized = FALSE;
    BOOL zip_initialized = FALSE;
    BOOL loaded = FALSE;
    WCHAR *root = NULL;
    WCHAR *zip_path = NULL;
    WCHAR *inner_entry = NULL;
    HINSTANCE instance = GetModuleHandleW(NULL);
    HBRUSH background = NULL;
    HWND window = NULL;
    WNDCLASSEXW window_class;
    MSG message;
    int exit_code = 0;

    g_heap = GetProcessHeap();
    if (g_heap == NULL) {
        return -1;
    }
    FvTrace_Initialize();
    if (FV_TRACE_ENABLED()) {
        FvTrace_Printf(L"process-start pid=%lu", GetCurrentProcessId());
    }
    {
        MEMORYSTATUSEX memory_status;
        ULONGLONG physical = 0;

        /* Бюджет кэша кадров — от объёма физической памяти (ViewerPlan.h). */
        memory_status.dwLength = sizeof(memory_status);
        if (GlobalMemoryStatusEx(&memory_status)) {
            physical = memory_status.ullTotalPhys;
        }
        g_cache_budget = FvPlan_CacheBudget(physical);
        g_bitmap_budget = FvPlan_BitmapBudget(g_cache_budget);
        fv_read_prefetch_setting();
        if (FV_TRACE_ENABLED()) {
            FvTrace_Printf(
                L"cache-budget bytes=%I64u bitmap_bytes=%I64u physical=%I64u radius=%I64u",
                g_cache_budget,
                g_bitmap_budget,
                physical,
                (ULONGLONG)FV_CACHE_RADIUS);
        }
    }
    fv_string_list_init(&g_files);
    ZipArchive_Init(&g_zip);
    zip_initialized = TRUE;

    status = OleInitialize(NULL);
    if (FAILED(status)) {
        exit_code = -1;
        goto cleanup;
    }
    ole_initialized = TRUE;

    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    status = ImageDecoder_Initialize();
    if (FAILED(status)) {
        MessageBoxW(NULL, L"Не удалось инициализировать WIC", L"FolderView", MB_OK | MB_ICONERROR);
        exit_code = -1;
        goto cleanup;
    }
    decoder_initialized = TRUE;

    status = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        &IID_ID2D1Factory,
        NULL,
        (void **)&g_d2d_factory);
    if (FAILED(status)) {
        MessageBoxW(NULL, L"Не удалось создать фабрику Direct2D", L"FolderView", MB_OK | MB_ICONERROR);
        exit_code = -1;
        goto cleanup;
    }

    if (!fv_choose_root(&root)) {
        exit_code = -1;
        goto cleanup;
    }
    if (FV_TRACE_ENABLED()) {
        FvTrace_Printf(
            L"start root=%s",
            FvTrace_Tail(root, FV_TRACE_PATH_CHARS));
    }
    if (fv_split_zip_virtual_path(root, &zip_path, &inner_entry)) {
        loaded = fv_build_zip_file_list(zip_path, inner_entry, FALSE);
    } else {
        loaded = fv_build_disk_file_list(root);
    }
    if (!loaded || g_files.count == 0) {
        goto cleanup;
    }

    background = CreateSolidBrush(RGB(169, 169, 169));
    if (background == NULL) {
        exit_code = -1;
        goto cleanup;
    }

    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = FvWindowProc;
    window_class.cbClsExtra = 0;
    window_class.cbWndExtra = 0;
    window_class.hInstance = instance;
    window_class.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    window_class.hbrBackground = background;
    window_class.lpszMenuName = NULL;
    window_class.lpszClassName = L"FolderView";
    window_class.hIconSm = NULL;

    if (RegisterClassExW(&window_class) == 0) {
        exit_code = -1;
        goto cleanup;
    }

    window = CreateWindowExW(
        0,
        window_class.lpszClassName,
        L"FolderView",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        NULL,
        NULL,
        instance,
        NULL);
    if (window == NULL) {
        exit_code = -1;
        goto cleanup;
    }

    if (FAILED(fv_create_render_target(window))) {
        MessageBoxW(NULL, L"Не удалось создать Direct2D render target", L"FolderView", MB_OK | MB_ICONERROR);
        exit_code = -1;
        goto cleanup;
    }

    ShowWindow(window, SW_MAXIMIZE);
    /* Первый кадр — синхронно (окно не показывает пустой фон), дальше — фон. */
    fv_set_current_image(window, FALSE);
    g_loader_active = FvLoader_Start(window, WM_FV_LOADED);
    if (g_loader_active) {
        fv_refresh_loader_archive();
        fv_schedule_loads(window);
    } else if (FV_TRACE_ENABLED()) {
        FvTrace_Printf(L"loader-start-failed error=%lu", GetLastError());
    }
    UpdateWindow(window);

    for (;;) {
        BOOL message_result = GetMessageW(&message, NULL, 0, 0);

        if (message_result == 0) {
            exit_code = (int)message.wParam;
            window = NULL;
            break;
        }
        if (message_result == -1) {
            exit_code = -1;
            break;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

cleanup:
    /*
     * Рабочий поток — до окна и кэша: он не трогает их, но его уведомления
     * адресованы окну, а незабранные результаты освобождает FvLoader_Stop.
     */
    if (g_loader_active) {
        FvLoader_Stop();
        g_loader_active = FALSE;
    }
    FvLoaderArchive_Release(g_loader_archive);
    g_loader_archive = NULL;
    if (window != NULL && IsWindow(window)) {
        DestroyWindow(window);
    }
    fv_release_render_target();
    if (g_d2d_factory != NULL) {
        FV_FACTORY_VT(g_d2d_factory)->Release(g_d2d_factory);
        g_d2d_factory = NULL;
    }
    if (background != NULL) {
        DeleteObject(background);
    }
    fv_clear_files_and_cache();
    if (zip_initialized) {
        ZipArchive_Close(&g_zip);
    }
    fv_heap_free(inner_entry);
    fv_heap_free(zip_path);
    fv_heap_free(root);
    if (decoder_initialized) {
        ImageDecoder_Shutdown();
    }
    if (ole_initialized) {
        OleUninitialize();
    }
    if (FV_TRACE_ENABLED()) {
        FvTrace_Printf(L"process-exit code=%d", exit_code);
    }
    FvTrace_Shutdown();
    return exit_code;
}

void WINAPI FolderViewEntry(void) {
    ExitProcess((UINT)fv_application_main());
}
