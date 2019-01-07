#pragma once

#ifdef _WIN32
#include <windows.h>
#else
#include <stdint.h>
#include <stddef.h>
typedef int INT;
typedef unsigned int UINT;
typedef long LONG;
typedef unsigned long ULONG;
typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef uintptr_t DWORD_PTR;
typedef bool BOOL;
typedef void *HMODULE;
typedef void *LPVOID;
typedef const void *LPCVOID;
typedef char *LPSTR;
typedef const char *LPCSTR;
typedef intptr_t (*FARPROC)();
#define WINAPI
#define CALLBACK
#endif
