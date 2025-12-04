
#include "main.h"
#include "res/resource.h"

ULONG StdOutExW(
    _In_ PCWSTR pszText,
    _In_ USHORT wColor)
{
    ULONG cWritten = 0;
    CONSOLE_SCREEN_BUFFER_INFO ConsoleScreenInfo;

    __stosb((PBYTE)&ConsoleScreenInfo, 0, sizeof(ConsoleScreenInfo));
    HANDLE StdHandle = NtGetStdHandle(STD_OUTPUT_HANDLE);
    if (StdHandle)
    {
        if (wColor)
        {
            _GetConsoleScreenBufferInfo()(StdHandle, &ConsoleScreenInfo);
            _SetConsoleTextAttribute()(StdHandle, wColor);
        }
        _WriteConsoleW()(StdHandle, pszText, (ULONG)wcslen(pszText), &cWritten, 0);
        if (ConsoleScreenInfo.wAttributes)
            _SetConsoleTextAttribute()(StdHandle, ConsoleScreenInfo.wAttributes);
    }
    return cWritten;
}

void PrintLastError()
{
    PWSTR Message = nullptr;
    _FormatMessageW()(
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_ALLOCATE_BUFFER,
        nullptr,
        NtGetLastError(),
        MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
        (PWSTR)&Message,
        0,
        nullptr);
    if (Message)
    {
        StdOutExW(Message, FOREGROUND_INTENSE_RED);
        FreeHeap(Message);
    }
}

int main()
{
    BOOL Result = FALSE;

    int NumArgs = 0;
    BOOL BackUp = FALSE;
    BOOL bCheckSum = FALSE;
    PWSTR In = nullptr;
    PWSTR* Args = NtCommandLineToArgvW(RtlProcessCommandLine(), &NumArgs);
    if (NumArgs > 1)
    {
        NumArgs--;
        for (ULONG i = 1; NumArgs; i++, NumArgs--)
        {
            if (!_wcsicmp(Args[i], L"-i"))
            {
                In = Args[i + 1];
                continue;
            }
            if (!_wcsicmp(Args[i], L"-b"))
            {
                BackUp = TRUE;
                continue;
            }
            if (!_wcsicmp(Args[i], L"-r"))
            {
                bCheckSum = TRUE;
                continue;
            }
        }
        if (In)
            Result = KillRichSignature(In, BackUp, bCheckSum);
    }
    if (Args)
        FreeHeap(Args);
    if (!Result)
    {
        StdOutExW(L"\t\t\tMicrosoft Rich Signature Remover\r\n\t\t   © 2001-2025 OnLyOnE, aLL rIGHTS rEVERSED\r\n\r\n", FOREGROUND_GREEN);
        StdOutExW(L"Usage: richkiller [-i file] -b -r\r\n\r\n", FOREGROUND_INTENSE_WHITE);
        StdOutExW(L"Options:\r\n", FOREGROUND_INTENSE_YELLOW);
        StdOutExW(L"  -i file  executables to remove rich signature\r\n", FOREGROUND_INTENSE_WHITE);
        StdOutExW(L"  -b       backup copy\r\n", FOREGROUND_INTENSE_WHITE);
        StdOutExW(L"  -r       add checksum to header\r\n\r\n", FOREGROUND_INTENSE_WHITE);
    }
    return 0;
}
