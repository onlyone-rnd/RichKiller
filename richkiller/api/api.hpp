#pragma once

#include "../ntdll/ntdll.h"

PVOID AllocateHeap(
    SIZE_T size);

SIZE_T SizeHeap(
    PVOID memory);

BOOLEAN FreeHeap(
    PVOID memory);

PVOID ReAllocateHeap(
    PVOID baseAddress,
    SIZE_T size);

FORCEINLINE void NtSetLastErrorEx(
    _In_ NTSTATUS status)
{
    NtCurrentTeb()->LastErrorValue = RtlNtStatusToDosError(status);
}

SIZE_T NtGetFileSizeEx(
    _In_ HANDLE FileHandle);

FORCEINLINE
void a2w(
    PSTR _In_ astr,
    PWSTR _Out_ wstr)
{
    for (; (*wstr = *astr); ++wstr, ++astr);
}

FORCEINLINE
void w2a(
    PWSTR _In_ wstr,
    PSTR _Out_ astr)
{
    for (; (*astr = *wstr); ++astr, ++wstr);
}

#ifdef _M_IX86
#pragma function(wcscpy)
#endif

FORCEINLINE
PWCHAR _wcscpy(
    PWCHAR to,
    PCWCHAR from)
{
    PWCHAR save = to;
    for (; (*to = *from); ++from, ++to);
    return save;
}

FORCEINLINE
PWCHAR _wcscat(
    PWCHAR s,
    PCWCHAR append)
{
    PWCHAR save = s;
    for (; *s; ++s);
    while ((*s++ = *append++));
    return save;
}

FORCEINLINE
SIZE_T _wcslen(
    PCWCHAR str)
{
    PCWCHAR s;

    if (str == 0)
        return 0;
    for (s = str; *s; ++s);
    return s - str;
}

_Use_decl_annotations_
FORCEINLINE
int _wcsicmp(
    PCWCHAR first,
    PCWCHAR last)
{
    int f;
    int l;
    do {
        if ((f = *first++) >= 'A' && f <= 'Z')
            f += 'a' - 'A';

        if ((l = *last++) >= 'A' && l <= 'Z')
            l += 'a' - 'A';
    } while (f && f == l);
    return(f - l);
}

template<typename _tchar>
FORCEINLINE
int _isspace(
    _tchar ch)
{
    return (ch == L' ' || ch == L'\t');
}

PWSTR* NtCommandLineToArgvW(
    _In_  PCWSTR cmdLine,
    _Out_ PINT argcOut);

FORCEINLINE
HANDLE NtGetStdHandle(
    _In_ ULONG nStdHandle)
{
    HANDLE Handle = INVALID_HANDLE_VALUE;
    PRTL_USER_PROCESS_PARAMETERS Ppb = NtCurrentPeb()->ProcessParameters;
    switch (nStdHandle)
    {
    case STD_INPUT_HANDLE:
        Handle = Ppb->StandardInput;
        break;

    case STD_OUTPUT_HANDLE:
        Handle = Ppb->StandardOutput;
        break;

    case STD_ERROR_HANDLE:
        Handle = Ppb->StandardError;
        break;
    }
    if (Handle == INVALID_HANDLE_VALUE)
        NtSetLastError(ERROR_INVALID_HANDLE);
    return Handle;
}

HMODULE LoadDll(
    _In_ PCWSTR LibName);

PVOID GetProcedureByName(
    _In_ HMODULE hModule,
    _In_ PCSTR pProcName);

PVOID GetProcedure(
    PCWSTR moduleName,
    PCSTR  procName);

#define API_WRAPPER(Define, DllName, FuncName)                      \
    DECLSPEC_SELECTANY PVOID __api_##Define = nullptr;              \
    inline decltype(&Define) _##Define()                            \
    {                                                               \
        if (!__api_##Define)                                        \
        {                                                           \
            PVOID p = GetProcedure(DllName, FuncName);              \
            if (!p)                                                 \
            {                                                       \
                NtTerminateProcess(                                 \
                    NtCurrentProcess,                               \
                    STATUS_ENTRYPOINT_NOT_FOUND);                   \
                for (;;);                                           \
            }                                                       \
            __api_##Define = p;                                     \
        }                                                           \
        return reinterpret_cast<decltype(&Define)>(__api_##Define); \
    }

API_WRAPPER(GetConsoleScreenBufferInfo, L"kernelbase.dll", "GetConsoleScreenBufferInfo");
API_WRAPPER(SetConsoleTextAttribute, L"kernelbase.dll", "SetConsoleTextAttribute");
API_WRAPPER(WriteConsoleW, L"kernelbase.dll", "WriteConsoleW");
API_WRAPPER(FormatMessageW, L"kernelbase.dll", "FormatMessageW");

FORCEINLINE
USHORT CalcCheckSum(
    ULONG StartValue,
    PVOID BaseAddress,
    ULONG WordCount)
{
    ULONG Sum = StartValue;
    PUSHORT Ptr = (PUSHORT)BaseAddress;
    for (ULONG i = 0; i < WordCount; i++)
    {
        Sum += *Ptr;
        if (HIWORD(Sum) != 0)
        {
            Sum = LOWORD(Sum) + HIWORD(Sum);
        }
        Ptr++;
    }
    return (USHORT)(LOWORD(Sum) + HIWORD(Sum));
}

PIMAGE_NT_HEADERS NtCheckSumMappedFile(
    _In_ PVOID BaseAddress,
    _In_ ULONG FileLength,
    _Out_ PULONG HeaderSum,
    _Out_ PULONG CheckSum);