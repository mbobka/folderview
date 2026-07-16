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
    FvD2DUnusedMethod GetMaximumBitmapSize;
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
    FvD2DUnusedMethod AddRef;
    ULONG (STDMETHODCALLTYPE *Release)(void *self);
} FvIUnknownVtbl;

struct ID2D1Bitmap {
    const FvIUnknownVtbl *lpVtbl;
};

#define FV_FACTORY_VT(object) ((object)->lpVtbl)
#define FV_TARGET_VT(object) ((object)->lpVtbl)
#define FV_UNKNOWN_VT(object) ((object)->lpVtbl)

#define FV_CACHE_RADIUS ((SIZE_T)50)
#define FV_MAX_ZIP_IMAGE_BYTES ((ULONGLONG)1024 * 1024 * 1024)

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

static HANDLE g_heap;
static ID2D1Factory *g_d2d_factory;
static ID2D1HwndRenderTarget *g_render_target;
static ID2D1Bitmap *g_d2d_bitmap;
static SIZE_T g_d2d_bitmap_image = ~(SIZE_T)0;
static FvStringList g_files;
static FvStringList g_zip_navigation;
static DecodedImage *g_cache;
static ZipArchive g_zip;
static const ZipEntry **g_zip_entries;
static BOOL g_zip_mode;
static SIZE_T g_current_image;
static SIZE_T g_current_zip;
static BOOL g_block_navigation;
static BOOL g_has_image_shift;
static POINT g_image_shift;
static POINTS g_drag_origin;
static FLOAT g_scale = 1.0f;

static LRESULT CALLBACK FvWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
static void fv_set_current_image(HWND window, BOOL repaint);
static void fv_previous_image(HWND window);
static void fv_next_image(HWND window);
static void fv_draw_image(HWND window);
static void fv_update_window_title(HWND window);
static void fv_release_d2d_bitmap(void);

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

static void fv_free_cache(void) {
    SIZE_T index;

    if (g_cache == NULL) {
        return;
    }
    for (index = 0; index < g_files.count; ++index) {
        DecodedImage_Free(&g_cache[index]);
    }
    fv_heap_free(g_cache);
    g_cache = NULL;
}

static void fv_clear_files_and_cache(void) {
    fv_release_d2d_bitmap();
    fv_free_cache();
    fv_string_list_clear(&g_files);
    fv_heap_free(g_zip_entries);
    g_zip_entries = NULL;
    g_current_image = 0;
    g_has_image_shift = FALSE;
    g_image_shift.x = 0;
    g_image_shift.y = 0;
}

static BOOL fv_allocate_cache_for_count(SIZE_T count, DecodedImage **cache) {
    SIZE_T bytes;

    *cache = NULL;
    if (count == 0) {
        return TRUE;
    }
    if (!fv_size_multiply(count, sizeof(DecodedImage), &bytes)) {
        return FALSE;
    }

    *cache = (DecodedImage *)fv_heap_allocate(bytes, TRUE);
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

static BOOL fv_enumerate_zip_files_recursive(
    const WCHAR *directory,
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
        return find_error == ERROR_FILE_NOT_FOUND ||
            find_error == ERROR_PATH_NOT_FOUND ||
            find_error == ERROR_ACCESS_DENIED;
    }

    for (;;) {
        WCHAR *full_path = NULL;

        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0 &&
                lstrcmpW(find_data.cFileName, L".") != 0 &&
                lstrcmpW(find_data.cFileName, L"..") != 0) {
                full_path = fv_join_path(directory, find_data.cFileName);
                if (full_path == NULL ||
                    !fv_enumerate_zip_files_recursive(full_path, destination)) {
                    success = FALSE;
                }
            }
        } else if (fv_is_zip_file(find_data.cFileName)) {
            full_path = fv_join_path(directory, find_data.cFileName);
            if (full_path == NULL) {
                success = FALSE;
            } else if (!fv_string_list_add_owned(destination, full_path)) {
                full_path = NULL;
                success = FALSE;
            } else {
                full_path = NULL;
            }
        }

        fv_heap_free(full_path);
        if (!success || !FindNextFileW(find_handle, &find_data)) {
            break;
        }
    }

    if (success && GetLastError() != ERROR_NO_MORE_FILES) {
        success = FALSE;
    }
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

static BOOL fv_build_zip_file_list(
    const WCHAR *zip_path,
    const WCHAR *start_entry,
    BOOL require_image,
    BOOL report_open_error) {
    ZipArchive temporary_zip;
    FvStringList temporary_files;
    const ZipEntry **temporary_zip_entries = NULL;
    DecodedImage *temporary_cache = NULL;
    SIZE_T temporary_current_image = 0;
    SIZE_T entry_count;
    SIZE_T byte_count;
    SIZE_T index;
    BOOL success = FALSE;

    ZipArchive_Init(&temporary_zip);
    fv_string_list_init(&temporary_files);
    if (!ZipArchive_Open(&temporary_zip, zip_path)) {
        if (report_open_error) {
            MessageBoxW(
                NULL,
                L"Не удалось открыть ZIP-архив",
                L"FolderView",
                MB_OK | MB_ICONERROR);
        }
        goto cleanup;
    }

    entry_count = ZipArchive_EntryCount(&temporary_zip);
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
        const ZipEntry *entry = ZipArchive_EntryAt(&temporary_zip, index);
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
        SetLastError(ERROR_FILE_NOT_FOUND);
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

    /* Старое состояние уничтожается только после полной подготовки нового. */
    fv_clear_files_and_cache();
    ZipArchive_Close(&g_zip);
    g_zip = temporary_zip;
    ZipArchive_Init(&temporary_zip);
    g_files = temporary_files;
    fv_string_list_init(&temporary_files);
    g_zip_entries = temporary_zip_entries;
    temporary_zip_entries = NULL;
    g_cache = temporary_cache;
    temporary_cache = NULL;
    g_current_image = temporary_current_image;
    g_zip_mode = TRUE;
    success = TRUE;

cleanup:
    fv_heap_free(temporary_cache);
    fv_heap_free(temporary_zip_entries);
    fv_string_list_clear(&temporary_files);
    ZipArchive_Close(&temporary_zip);
    return success;
}

static BOOL fv_open_first_zip_from_tree(const WCHAR *root) {
    FvStringList archives;
    SIZE_T index;
    BOOL success = FALSE;

    fv_string_list_init(&archives);
    if (!fv_enumerate_zip_files_recursive(root, &archives)) {
        goto cleanup;
    }
    fv_string_list_sort(&archives);

    for (index = 0; index < archives.count; ++index) {
        if (fv_build_zip_file_list(
                archives.items[index],
                NULL,
                TRUE,
                FALSE)) {
            g_zip_navigation = archives;
            fv_string_list_init(&archives);
            g_current_zip = index;
            success = TRUE;
            break;
        }
        if (GetLastError() == ERROR_OUTOFMEMORY) {
            break;
        }
    }

cleanup:
    fv_string_list_clear(&archives);
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
    image = &g_cache[g_current_image];
    if (!DecodedImage_IsValid(image)) {
        return;
    }

    title.data = NULL;
    title.length = 0;
    title.capacity = 0;
    if (g_zip_mode) {
        fv_builder_append(&title, ZipArchive_Path(&g_zip));
        fv_builder_append(&title, L" -> ");
    }
    fv_builder_append(&title, g_files.items[g_current_image]);
    fv_builder_append(&title, L" [");
    fv_builder_append_u64(&title, image->width);
    fv_builder_append(&title, L"x");
    fv_builder_append_u64(&title, image->height);
    fv_builder_append(&title, image->has_alpha ? L" RGBA] " : L" RGB] ");
    percentage = (ULONGLONG)(g_scale * 100.0f + 0.5f);
    fv_builder_append_u64(&title, percentage);
    fv_builder_append(&title, L"%");

    if (title.data != NULL) {
        SetWindowTextW(window, title.data);
    }
    fv_builder_dispose(&title);
}

static void fv_trim_cache(void) {
    SIZE_T index;

    for (index = 0; index < g_files.count; ++index) {
        SIZE_T distance = index < g_current_image
            ? g_current_image - index
            : index - g_current_image;
        if (distance > FV_CACHE_RADIUS) {
            DecodedImage_Free(&g_cache[index]);
        }
    }
}

static HRESULT fv_decode_current_image(void) {
    HRESULT result;

    if (g_zip_mode) {
        BYTE *data = NULL;
        SIZE_T size = 0;

        if (g_zip_entries == NULL ||
            g_current_image >= g_files.count ||
            !ZipArchive_ExtractEntry(
                &g_zip,
                g_zip_entries[g_current_image],
                FV_MAX_ZIP_IMAGE_BYTES,
                &data,
                &size)) {
            return E_FAIL;
        }
        result = ImageDecoder_DecodeMemory(data, size, &g_cache[g_current_image]);
        ZipArchive_FreeBuffer(data);
        return result;
    }

    return ImageDecoder_DecodeFile(
        g_files.items[g_current_image],
        &g_cache[g_current_image]);
}

static void fv_set_current_image(HWND window, BOOL repaint) {
    DecodedImage *image;
    WINDOWPLACEMENT placement;

    if (g_cache == NULL || g_current_image >= g_files.count) {
        return;
    }

    g_has_image_shift = FALSE;
    g_image_shift.x = 0;
    g_image_shift.y = 0;
    fv_release_d2d_bitmap();
    image = &g_cache[g_current_image];
    if (!DecodedImage_IsValid(image)) {
        if (FAILED(fv_decode_current_image()) || !DecodedImage_IsValid(image)) {
            MessageBoxW(window, L"Не удалось загрузить изображение", L"Ошибка", MB_OK | MB_ICONERROR);
            if (repaint) {
                InvalidateRect(window, NULL, FALSE);
            }
            return;
        }
        fv_trim_cache();
    }

    g_scale = fv_calculate_scale(window, image->width, image->height);
    fv_update_window_title(window);

    placement.length = sizeof(placement);
    if (GetWindowPlacement(window, &placement) && placement.showCmd != SW_MAXIMIZE) {
        RECT rectangle;

        rectangle.left = 0;
        rectangle.top = 0;
        rectangle.right = (LONG)image->width;
        rectangle.bottom = (LONG)image->height;
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

static void fv_release_d2d_bitmap(void) {
    if (g_d2d_bitmap != NULL) {
        FV_UNKNOWN_VT(g_d2d_bitmap)->Release(g_d2d_bitmap);
        g_d2d_bitmap = NULL;
    }
    g_d2d_bitmap_image = ~(SIZE_T)0;
}

static void fv_release_render_target(void) {
    fv_release_d2d_bitmap();
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

static void fv_draw_image(HWND window) {
    const DecodedImage *image = NULL;
    ID2D1Bitmap *bitmap = NULL;
    D2D1_COLOR_F background;
    HRESULT result;

    if (FAILED(fv_create_render_target(window))) {
        return;
    }
    if ((FV_TARGET_VT(g_render_target)->CheckWindowState(g_render_target) &
         D2D1_WINDOW_STATE_OCCLUDED) != 0) {
        return;
    }

    if (g_cache != NULL && g_current_image < g_files.count &&
        DecodedImage_IsValid(&g_cache[g_current_image])) {
        image = &g_cache[g_current_image];
        if (g_d2d_bitmap == NULL || g_d2d_bitmap_image != g_current_image) {
            fv_release_d2d_bitmap();
            g_d2d_bitmap = fv_create_d2d_bitmap(image);
            if (g_d2d_bitmap != NULL) {
                g_d2d_bitmap_image = g_current_image;
            }
        }
        bitmap = g_d2d_bitmap;
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
            FLOAT scaled_width = (FLOAT)image->width * g_scale;
            FLOAT scaled_height = (FLOAT)image->height * g_scale;
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
        }
    }

    result = FV_TARGET_VT(g_render_target)->EndDraw(g_render_target, NULL, NULL);
    if (result == D2DERR_RECREATE_TARGET) {
        fv_release_render_target();
    }
}

static void fv_previous_image(HWND window) {
    if (g_block_navigation || g_files.count == 0 || g_current_image == 0) {
        return;
    }
    --g_current_image;
    fv_set_current_image(window, TRUE);
}

static void fv_next_image(HWND window) {
    if (g_block_navigation || g_current_image + 1 >= g_files.count) {
        return;
    }
    ++g_current_image;
    fv_set_current_image(window, TRUE);
}

static void fv_scale_image(HWND window, BOOL increase) {
    if (g_files.count == 0) {
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

    if (g_cache == NULL || g_current_image >= g_files.count ||
        !DecodedImage_IsValid(&g_cache[g_current_image])) {
        return;
    }
    image = &g_cache[g_current_image];

    if (fv_float_almost_equal(g_scale, 1.0f)) {
        FLOAT fitted = fv_calculate_scale(window, image->width, image->height);
        if (fv_float_almost_equal(fitted, 1.0f)) {
            return;
        }
        g_scale = fitted;
    } else {
        g_scale = 1.0f;
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

    if (g_zip_navigation.count != 0) {
        SIZE_T next_index = g_current_zip;

        for (;;) {
            if (direction < 0 && next_index > 0) {
                --next_index;
            } else if (direction > 0 && next_index + 1 < g_zip_navigation.count) {
                ++next_index;
            } else {
                return;
            }

            if (fv_build_zip_file_list(
                    g_zip_navigation.items[next_index],
                    NULL,
                    TRUE,
                    FALSE)) {
                g_current_zip = next_index;
                fv_set_current_image(window, TRUE);
                return;
            }
        }
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

    if (fv_build_zip_file_list(next_archive, NULL, TRUE, TRUE)) {
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
                fv_set_current_image(window, TRUE);
            }
            return 0;

        case VK_NUMPAD1:
        case VK_END:
            if (!g_block_navigation && g_files.count != 0) {
                g_current_image = g_files.count - 1;
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
            DecodedImage_IsValid(&g_cache[g_current_image])) {
            const DecodedImage *image = &g_cache[g_current_image];
            RECT client;

            if (GetClientRect(window, &client)) {
                POINTS current = MAKEPOINTS(lparam);
                LONG limit_x = fv_pan_limit(
                    (FLOAT)image->width * g_scale,
                    client.right - client.left);
                LONG limit_y = fv_pan_limit(
                    (FLOAT)image->height * g_scale,
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
    fv_string_list_init(&g_files);
    fv_string_list_init(&g_zip_navigation);
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
    if (fv_split_zip_virtual_path(root, &zip_path, &inner_entry)) {
        loaded = fv_build_zip_file_list(zip_path, inner_entry, FALSE, TRUE);
    } else {
        loaded = fv_build_disk_file_list(root);
        if (loaded && g_files.count == 0) {
            loaded = fv_open_first_zip_from_tree(root);
        }
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
    fv_set_current_image(window, FALSE);
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
    fv_string_list_clear(&g_zip_navigation);
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
    return exit_code;
}

void WINAPI FolderViewEntry(void) {
    ExitProcess((UINT)fv_application_main());
}
