
#include "main.h"

#define MIN_FILE_SIZE 0x1000

ULONG FindRichSignature(
    _In_ PUCHAR Buffer,
    _In_ ULONG BufSize,
    _In_ PUCHAR StartPtr,
    _Out_ PULONG RichOffset)
{
    *RichOffset = 0;
    ULONG RichSize = 0;

    if ((StartPtr - Buffer) <= 128)
        return NULL;
    for (; StartPtr != Buffer; StartPtr--)
    {
        if (*(ULONG*)StartPtr != 'hciR') // Rich
            continue;
        StartPtr -= 4;
        RichSize = 8;
        for (; *(ULONG*)StartPtr && StartPtr != Buffer; StartPtr--)
        {
            RichSize++;
        }
        if (StartPtr == Buffer)
            return NULL;
        *RichOffset = (StartPtr + 4) - Buffer;
    }
    return RichSize;
}

BOOL GetRichSignatureInfo(
    _In_ PWSTR FileName,
    _Out_ PULONG RichOffset,
    _Out_ PULONG SizeForMoved)
{
    *RichOffset = 0;
    *SizeForMoved = 0;

    BOOL Result = FALSE;

    HANDLE FileHandle = NULL;
    UNICODE_STRING uFileName;
    NTSTATUS status = RtlDosPathNameToNtPathName_U(
        FileName,
        &uFileName,
        nullptr,
        nullptr);
    if (!NT_SUCCESS(status))
    {
        NtSetLastErrorEx(status);
        PrintLastError();
        return FALSE;
    }
    IO_STATUS_BLOCK IoStatusBlock;
    OBJECT_ATTRIBUTES ObjectAttributes;
    InitializeObjectAttributes(&ObjectAttributes, &uFileName, OBJ_CASE_INSENSITIVE, nullptr, nullptr);
    status = NtCreateFile(
        &FileHandle,
        GENERIC_READ | SYNCHRONIZE | FILE_READ_ATTRIBUTES,
        &ObjectAttributes,
        &IoStatusBlock,
        0,
        0,
        0,
        FILE_OPEN,
        FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_FOR_BACKUP_INTENT,
        nullptr,
        0);
    RtlFreeUnicodeString(&uFileName);
    if (!NT_SUCCESS(status))
    {
        NtSetLastErrorEx(status);
        PrintLastError();
        return FALSE;
    }
    LARGE_INTEGER FileSize;
    FileSize.QuadPart = NtGetFileSizeEx(FileHandle);
    if (!FileSize.QuadPart)
    {
        PrintLastError();
        NtClose(FileHandle);
        return FALSE;
    }
    else if (FileSize.QuadPart < MIN_FILE_SIZE)
    {
        StdOutExW(L"\r\nFile Size to Small!\r\n\r\n", FOREGROUND_INTENSE_RED);
        NtClose(FileHandle);
        return FALSE;
    }
    FileSize.QuadPart = MIN_FILE_SIZE;
    HANDLE SectionHandle = NULL;
    status = NtCreateSection(
        &SectionHandle,
        SECTION_ALL_ACCESS,
        nullptr,
        &FileSize,
        PAGE_READONLY,
        SEC_COMMIT,
        FileHandle);
    NtClose(FileHandle);
    if (!NT_SUCCESS(status))
    {
        NtSetLastErrorEx(status);
        PrintLastError();
        return FALSE;
    }
    SIZE_T ViewSize = 0;
    PVOID  BaseAddress = nullptr;
    status = NtMapViewOfSection(
        SectionHandle,
        NtCurrentProcess,
        &BaseAddress,
        0,
        0,
        nullptr,
        &ViewSize,
        ViewUnmap,
        0,
        PAGE_READONLY);
    NtClose(SectionHandle);
    if (!NT_SUCCESS(status))
    {
        NtSetLastErrorEx(status);
        PrintLastError();
        return FALSE;
    }
    PIMAGE_NT_HEADERS NtHeaders = RtlImageNtHeader(BaseAddress);
    if (!NtHeaders)
    {
        NtSetLastError(ERROR_INVALID_EXE_SIGNATURE);
        PrintLastError();
        return FALSE;
    }
    ULONG RichSize = FindRichSignature((PUCHAR)BaseAddress, MIN_FILE_SIZE, (PUCHAR)NtHeaders, RichOffset);
    if (!RichSize || !*RichOffset)
    {
        StdOutExW(L"\r\nRich Signature Not Found!\r\n\r\n", FOREGROUND_INTENSE_RED);
        goto Exit;
    }
    *SizeForMoved = NtHeaders->OptionalHeader.SizeOfHeaders - ((PUCHAR)NtHeaders - (PUCHAR)BaseAddress);
    Result = TRUE;
Exit:
    NtUnmapViewOfSection(NtCurrentProcess, BaseAddress);
    return Result;
}

void CopyFileLoop(
    HANDLE FileHandleSource,
    HANDLE FileHandleDest,
    SIZE_T SourceFileSize)
{
    PUCHAR BufferPtr = nullptr;
    SIZE_T BufferSize = 0x10000;
    NTSTATUS status = NtAllocateVirtualMemory(
        NtCurrentProcess,
        (PVOID*)&BufferPtr,
        0,
        &BufferSize,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_READWRITE);
    if (!NT_SUCCESS(status))
    {
        NtSetLastErrorEx(status);
        PrintLastError();
        return;
    }
    SIZE_T BytesCopied = 0;
    BOOL EndOfFileFound = FALSE;
    IO_STATUS_BLOCK IoStatusBlock;
    while (!EndOfFileFound)
    {
        status = NtReadFile(
            FileHandleSource,
            NULL,
            nullptr,
            nullptr,
            &IoStatusBlock,
            BufferPtr,
            BufferSize,
            nullptr,
            nullptr);
        if (NT_SUCCESS(status) && !IoStatusBlock.Information)
            status = STATUS_END_OF_FILE;
        if (NT_SUCCESS(status))
        {
            status = NtWriteFile(
                FileHandleDest,
                NULL,
                nullptr,
                nullptr,
                &IoStatusBlock,
                BufferPtr,
                IoStatusBlock.Information,
                nullptr,
                nullptr);
            if (NT_SUCCESS(status))
                BytesCopied += IoStatusBlock.Information;
            else
            {
                NtSetLastErrorEx(status);
                PrintLastError();
                goto Exit;
            }
        }
        else if (!NT_SUCCESS(status))
        {
            if (status == STATUS_END_OF_FILE)
            {
                EndOfFileFound = TRUE;
                status = STATUS_SUCCESS;
            }
            else
            {
                NtSetLastErrorEx(status);
                PrintLastError();
                goto Exit;
            }
        }
    }
Exit:
    if (BufferPtr)
        NtFreeVirtualMemory(
            NtCurrentProcess,
            (PVOID*)&BufferPtr,
            &BufferSize,
            MEM_RELEASE);
}

void BackUpFile(
    _In_ PWSTR FileName)
{
    UNICODE_STRING uFileName;
    NTSTATUS status = RtlDosPathNameToNtPathName_U(
        FileName,
        &uFileName,
        nullptr,
        nullptr);
    if (!NT_SUCCESS(status))
    {
        NtSetLastErrorEx(status);
        PrintLastError();
        return;
    }
    IO_STATUS_BLOCK IoStatusBlock;
    OBJECT_ATTRIBUTES ObjectAttributes;
    InitializeObjectAttributes(&ObjectAttributes, &uFileName, OBJ_CASE_INSENSITIVE, nullptr, nullptr);
    HANDLE FileHandleSource = NULL;
    HANDLE FileHandleDest = NULL;
    PWCHAR NewFileName = nullptr;
    status = NtCreateFile(
        &FileHandleSource,
        GENERIC_READ | SYNCHRONIZE | FILE_READ_ATTRIBUTES,
        &ObjectAttributes,
        &IoStatusBlock,
        0,
        0,
        0,
        FILE_OPEN,
        FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_FOR_BACKUP_INTENT,
        nullptr,
        0);
    RtlFreeUnicodeString(&uFileName);
    if (!NT_SUCCESS(status))
    {
        NtSetLastErrorEx(status);
        PrintLastError();
        return;
    }
    SIZE_T SourceFileSize = NtGetFileSizeEx(FileHandleSource);
    if (!SourceFileSize)
    {
        StdOutExW(L"\r\nFile Size to Small!\r\n\r\n", FOREGROUND_INTENSE_RED);
        goto Exit;
    }
    NewFileName = (PWCHAR)AllocateHeap((wcslen(FileName) + 8) * 2);
    if (!NewFileName)
    {
        NtSetLastError(ERROR_INSUFFICIENT_BUFFER);
        PrintLastError();
        goto Exit;
    }
    _wcscpy(NewFileName, FileName);
    _wcscat(NewFileName, L".backup");
    status = RtlDosPathNameToNtPathName_U(
        NewFileName,
        &uFileName,
        nullptr,
        nullptr);
    if (!NT_SUCCESS(status))
    {
        NtSetLastErrorEx(status);
        PrintLastError();
        goto Exit;
    }
    InitializeObjectAttributes(&ObjectAttributes, &uFileName, OBJ_CASE_INSENSITIVE, nullptr, nullptr);
    status = NtCreateFile(
        &FileHandleDest,
        GENERIC_WRITE | SYNCHRONIZE | FILE_READ_ATTRIBUTES,
        &ObjectAttributes,
        &IoStatusBlock,
        0,
        0,
        0,
        FILE_OPEN_IF,
        FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_FOR_BACKUP_INTENT,
        nullptr,
        0);
    RtlFreeUnicodeString(&uFileName);
    if (!NT_SUCCESS(status))
    {
        NtSetLastErrorEx(status);
        PrintLastError();
        goto Exit;
    }
    CopyFileLoop(FileHandleSource, FileHandleDest, SourceFileSize);
Exit:
    if (FileHandleSource)
        NtClose(FileHandleSource);
    if (FileHandleDest)
        NtClose(FileHandleDest);
    if (NewFileName)
        FreeHeap(NewFileName);
}

void RemoveRichSignature(
    _In_ PWSTR FileName,
    _In_ ULONG RichOffset,
    _In_ ULONG SizeForMoved,
    _In_ BOOL bCheckSum)
{
    UNICODE_STRING uFileName;

    NTSTATUS status = RtlDosPathNameToNtPathName_U(
        FileName,
        &uFileName,
        nullptr,
        nullptr);
    if (!NT_SUCCESS(status))
    {
        NtSetLastErrorEx(status);
        PrintLastError();
        return;
    }
    IO_STATUS_BLOCK IoStatusBlock;
    OBJECT_ATTRIBUTES ObjectAttributes;
    InitializeObjectAttributes(&ObjectAttributes, &uFileName, OBJ_CASE_INSENSITIVE, nullptr, nullptr);
    HANDLE FileHandle = NULL;
    status = NtCreateFile(
        &FileHandle,
        GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE | FILE_READ_ATTRIBUTES,
        &ObjectAttributes,
        &IoStatusBlock,
        0,
        0,
        0,
        FILE_OPEN,
        FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_FOR_BACKUP_INTENT,
        nullptr,
        0);
    RtlFreeUnicodeString(&uFileName);
    if (!NT_SUCCESS(status))
    {
        NtSetLastErrorEx(status);
        PrintLastError();
        return;
    }
    LARGE_INTEGER FileSize;
    FileSize.HighPart = 0;
    FileSize.LowPart = MIN_FILE_SIZE;
    if (bCheckSum)
    {
        FileSize.QuadPart = NtGetFileSizeEx(FileHandle);
    }
    HANDLE SectionHandle = NULL;
    status = NtCreateSection(
        &SectionHandle,
        SECTION_ALL_ACCESS,
        nullptr,
        &FileSize,
        PAGE_READWRITE,
        SEC_COMMIT,
        FileHandle);
    NtClose(FileHandle);
    if (!NT_SUCCESS(status))
    {
        NtSetLastErrorEx(status);
        PrintLastError();
        return;
    }
    SIZE_T ViewSize = 0;
    PVOID  BaseAddress = nullptr;
    status = NtMapViewOfSection(
        SectionHandle,
        NtCurrentProcess,
        &BaseAddress,
        0,
        0,
        nullptr,
        &ViewSize,
        ViewUnmap,
        0,
        PAGE_READWRITE);
    NtClose(SectionHandle);
    if (!NT_SUCCESS(status))
    {
        NtSetLastErrorEx(status);
        PrintLastError();
        return;
    }
    PIMAGE_NT_HEADERS NtHeaders = RtlImageNtHeader(BaseAddress);
    __movsb((PBYTE)(RichOffset + (PUCHAR)BaseAddress), (PBYTE)NtHeaders, SizeForMoved);
    ((PIMAGE_DOS_HEADER)BaseAddress)->e_lfanew = (LONG)RichOffset;
    if (bCheckSum)
    {
        ULONG HeaderSum = 0;
        ULONG CheckSum = 0;
        NtHeaders = NtCheckSumMappedFile(
            BaseAddress,
            (ULONG)FileSize.QuadPart,
            &HeaderSum,
            &CheckSum);
        if (NtHeaders)
        {
            if (HeaderSum != CheckSum)
            {
                NtHeaders->OptionalHeader.CheckSum = CheckSum;
            }
        }
    }
    NtUnmapViewOfSection(NtCurrentProcess, BaseAddress);
}

BOOL KillRichSignature(
    _In_ PWSTR FileName,
    _In_ BOOL BackUp,
    _In_ BOOL bCheckSum)
{
    BOOL Result = FALSE;
    ULONG RichOffset = 0;
    ULONG SizeForMoved = 0;

    if (!GetRichSignatureInfo(FileName, &RichOffset, &SizeForMoved))
        return FALSE;
    if (BackUp)
        BackUpFile(FileName);
    RemoveRichSignature(FileName, RichOffset, SizeForMoved, bCheckSum);
    return TRUE;
}
