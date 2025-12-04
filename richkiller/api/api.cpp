
#include "api.hpp"

PVOID AllocateHeap(
    SIZE_T size)
{
    return RtlAllocateHeap(
        RtlProcessHeap(),
        HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS,
        size);
}

SIZE_T SizeHeap(
    PVOID memory)
{
    if (!memory)
        return NULL;
    return RtlSizeHeap(
        RtlProcessHeap(),
        NULL,
        memory);
}

BOOLEAN FreeHeap(
    PVOID memory)
{
    if (!memory)
        return FALSE;
    SIZE_T size = SizeHeap(memory);
    if (size != (SIZE_T)-1)
        __stosb((PBYTE)memory, 0, size);

    return RtlFreeHeap(
        RtlProcessHeap(),
        NULL,
        memory);
}

PVOID ReAllocateHeap(
    PVOID baseAddress,
    SIZE_T size)
{
    if (!RtlProcessHeap())
        return NULL;
    return RtlReAllocateHeap(
        RtlProcessHeap(),
        HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS,
        baseAddress,
        size);
}

SIZE_T NtGetFileSizeEx(
    _In_ HANDLE FileHandle)
{
    IO_STATUS_BLOCK IoStatusBlock;
    FILE_STANDARD_INFORMATION FileInformation;

    NTSTATUS status = NtQueryInformationFile(
        FileHandle,
        &IoStatusBlock,
        &FileInformation,
        sizeof(FileInformation),
        FileStandardInformation);
    if (NT_SUCCESS(status))
        return FileInformation.EndOfFile.QuadPart;
    NtSetLastErrorEx(status);
    return NULL;
}

PWSTR* NtCommandLineToArgvW(
    _In_  PCWSTR cmdLine,
    _Out_ PINT argcOut)
{
    *argcOut = 0;

    if (!cmdLine || !argcOut)
        return nullptr;
    SIZE_T cmdLen = wcslen(cmdLine);
    if (!cmdLen)
        return nullptr;
    SIZE_T ptrsSize = (cmdLen / 2 + 2) * sizeof(PWSTR);
    SIZE_T strBufSize = (cmdLen + 1) * sizeof(WCHAR);
    SIZE_T totalSize = ptrsSize + strBufSize;
    PUCHAR block = (PUCHAR)AllocateHeap(totalSize);
    if (!block)
        return nullptr;
    PWSTR* argv = (PWSTR*)block;
    PWSTR  dst = (PWSTR)(block + ptrsSize);
    int argc = 0;
    PCWCHAR s = cmdLine;
    while (*s)
    {
        while (*s && _isspace(*s))
            ++s;
        if (!*s)
            break;
        argv[argc++] = dst;
        int inQuotes = 0;
        for (;;)
        {
            WCHAR ch = *s;
            if (!ch)
                break;
            if (!inQuotes && _isspace(ch))
                break;
            if (ch == L'\"')
            {
                inQuotes = ~inQuotes;
                ++s;
                continue;
            }
            if (ch == L'\\')
            {
                int backslashes = 0;
                PCWCHAR p = s;
                while (*p == L'\\')
                {
                    ++backslashes;
                    ++p;
                }
                if (*p == L'\"')
                {
                    for (int i = 0; i < backslashes / 2; ++i)
                        *dst++ = L'\\';
                    if ((backslashes & 1) == 0)
                    {
                        ++p;
                        inQuotes = ~inQuotes;
                    }
                    else
                    {
                        *dst++ = L'\"';
                        ++p;
                    }
                    s = p;
                    continue;
                }
                else
                {
                    for (int i = 0; i < backslashes; ++i)
                        *dst++ = L'\\';
                    s = p;
                    continue;
                }
            }
            *dst++ = ch;
            ++s;
        }
        *dst++ = L'\0';
    }
    argv[argc] = nullptr;
    *argcOut = argc;
    return argv;
}

HMODULE LoadDll(
    _In_ PCWSTR LibName)
{
    HMODULE Dll = NULL;
    UNICODE_STRING unicodeString;

    RtlInitUnicodeString(&unicodeString, (PWSTR)LibName);
    LdrLoadDll(nullptr, nullptr, &unicodeString, (PVOID*)&Dll);
    return Dll;
}

PVOID GetProcedureByName(
    _In_ HMODULE hModule,
    _In_ PCSTR pProcName)
{
    PVOID Address = nullptr;
    ANSI_STRING ansiString;

    if ((INT_PTR)pProcName > 65535)
    {
        RtlInitAnsiString(&ansiString, (PSTR)pProcName);
        LdrGetProcedureAddress(hModule, &ansiString, NULL, &Address);
    }
    else
        LdrGetProcedureAddress(hModule, NULL, (ULONG)pProcName, &Address);
    return Address;
}

PVOID GetProcedure(
    PCWSTR moduleName,
    PCSTR  procName)
{
    HMODULE hModule = LoadDll(moduleName);
    if (hModule)
        return GetProcedureByName(hModule, procName);
    return nullptr;
}

PIMAGE_NT_HEADERS NtCheckSumMappedFile(
    _In_ PVOID BaseAddress,
    _In_ ULONG FileLength,
    _Out_ PULONG HeaderSum,
    _Out_ PULONG CheckSum)
{
    *HeaderSum = 0;
    *CheckSum = 0;

    PIMAGE_NT_HEADERS NtHeader = nullptr;
    ULONG HdrSum = 0;
    ULONG CalcSum = CalcCheckSum(0, BaseAddress, (FileLength + 1) / sizeof(USHORT));
    NtHeader = RtlImageNtHeader(BaseAddress);
    if (!NtHeader)
        return nullptr;
    *HeaderSum = HdrSum = NtHeader->OptionalHeader.CheckSum;
    if (LOWORD(CalcSum) >= LOWORD(HdrSum))
        CalcSum -= LOWORD(HdrSum);
    else
        CalcSum = ((LOWORD(CalcSum) - LOWORD(HdrSum)) & 0xFFFF) - 1;
    if (LOWORD(CalcSum) >= HIWORD(HdrSum))
        CalcSum -= HIWORD(HdrSum);
    else
        CalcSum = ((LOWORD(CalcSum) - HIWORD(HdrSum)) & 0xFFFF) - 1;
    CalcSum += FileLength;
    *CheckSum = CalcSum;
    return NtHeader;
}