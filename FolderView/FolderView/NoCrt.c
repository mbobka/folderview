// Минимальные compiler helpers для freestanding-сборки Clang.
// Это не CRT: функции покрывают только операции, которые LLVM
// может сгенерировать при копировании структур и работе с float.

typedef __SIZE_TYPE__ size_t;
typedef __UINTPTR_TYPE__ uintptr_t;

int _fltused = 0;

#pragma clang optimize off

void* __cdecl memcpy(void* destination, const void* source, size_t count) {
    unsigned char* out = (unsigned char*)destination;
    const unsigned char* in = (const unsigned char*)source;

    while (count != 0) {
        *out++ = *in++;
        --count;
    }

    return destination;
}

void* __cdecl memmove(void* destination, const void* source, size_t count) {
    unsigned char* out = (unsigned char*)destination;
    const unsigned char* in = (const unsigned char*)source;

    if ((uintptr_t)out <= (uintptr_t)in ||
        (uintptr_t)out - (uintptr_t)in >= count) {
        while (count != 0) {
            *out++ = *in++;
            --count;
        }
    }
    else {
        out += count;
        in += count;
        while (count != 0) {
            *--out = *--in;
            --count;
        }
    }

    return destination;
}

void* __cdecl memset(void* destination, int value, size_t count) {
    unsigned char* out = (unsigned char*)destination;
    const unsigned char byte = (unsigned char)value;

    while (count != 0) {
        *out++ = byte;
        --count;
    }

    return destination;
}

int __cdecl memcmp(const void* left, const void* right, size_t count) {
    const unsigned char* a = (const unsigned char*)left;
    const unsigned char* b = (const unsigned char*)right;

    while (count != 0) {
        if (*a != *b) {
            return *a < *b ? -1 : 1;
        }
        ++a;
        ++b;
        --count;
    }

    return 0;
}

#pragma clang optimize on
