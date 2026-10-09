#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define PATH_BUF   32768
#define CHUNK_SIZE 65536

static BOOL BuildStreamPath(wchar_t* out, size_t outSize,
                            LPCWSTR filePath, LPCWSTR streamName)
{
    return swprintf(out, outSize, L"%ls:%ls", filePath, streamName) >= 0;
}

// write <файл> <поток> <источник>
static int CmdWrite(LPCWSTR filePath, LPCWSTR streamName, LPCWSTR sourcePath)
{
    wchar_t fullPath[PATH_BUF];
    if (!BuildStreamPath(fullPath, PATH_BUF, filePath, streamName)) {
        fwprintf(stderr, L"Путь слишком длинный\n");
        return 2;
    }

    HANDLE hSrc = CreateFileW(sourcePath, GENERIC_READ, FILE_SHARE_READ,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSrc == INVALID_HANDLE_VALUE) {
        fwprintf(stderr, L"Не открыть источник: %lu\n", GetLastError());
        return 3;
    }

    HANDLE hDst = CreateFileW(fullPath, GENERIC_WRITE, 0,
                              NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hDst == INVALID_HANDLE_VALUE) {
        fwprintf(stderr, L"Не открыть поток: %lu\n", GetLastError());
        CloseHandle(hSrc);
        return 4;
    }

    BYTE* buf = (BYTE*)malloc(CHUNK_SIZE);
    if (!buf) { CloseHandle(hSrc); CloseHandle(hDst); return 5; }

    DWORD read = 0, written = 0;
    ULONGLONG total = 0;
    BOOL ok = TRUE;

    while (ok) {
        if (!ReadFile(hSrc, buf, CHUNK_SIZE, &read, NULL)) {
            fwprintf(stderr, L"Ошибка чтения: %lu\n", GetLastError());
            ok = FALSE; break;
        }
        if (read == 0) break;

        if (!WriteFile(hDst, buf, read, &written, NULL) || written != read) {
            fwprintf(stderr, L"Ошибка записи: %lu\n", GetLastError());
            ok = FALSE; break;
        }
        total += written;
    }

    free(buf);
    CloseHandle(hSrc);
    CloseHandle(hDst);

    if (ok) wprintf(L"Записано в %ls: %llu байт\n", fullPath, total);
    return ok ? 0 : 6;
}

// read <файл> <поток> <выходной_файл>
static int CmdRead(LPCWSTR filePath, LPCWSTR streamName, LPCWSTR outputPath)
{
    wchar_t fullPath[PATH_BUF];
    if (!BuildStreamPath(fullPath, PATH_BUF, filePath, streamName)) {
        fwprintf(stderr, L"Путь слишком длинный\n");
        return 2;
    }

    HANDLE hSrc = CreateFileW(fullPath, GENERIC_READ, FILE_SHARE_READ,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSrc == INVALID_HANDLE_VALUE) {
        fwprintf(stderr, L"Не открыть поток: %lu\n", GetLastError());
        return 3;
    }

    HANDLE hDst = CreateFileW(outputPath, GENERIC_WRITE, 0,
                              NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hDst == INVALID_HANDLE_VALUE) {
        fwprintf(stderr, L"Не создать файл: %lu\n", GetLastError());
        CloseHandle(hSrc);
        return 4;
    }

    BYTE* buf = (BYTE*)malloc(CHUNK_SIZE);
    if (!buf) { CloseHandle(hSrc); CloseHandle(hDst); return 5; }

    DWORD read = 0, written = 0;
    ULONGLONG total = 0;
    BOOL ok = TRUE;

    while (ok) {
        if (!ReadFile(hSrc, buf, CHUNK_SIZE, &read, NULL)) {
            fwprintf(stderr, L"Ошибка чтения: %lu\n", GetLastError());
            ok = FALSE; break;
        }
        if (read == 0) break;

        if (!WriteFile(hDst, buf, read, &written, NULL) || written != read) {
            fwprintf(stderr, L"Ошибка записи: %lu\n", GetLastError());
            ok = FALSE; break;
        }
        total += written;
    }

    free(buf);
    CloseHandle(hSrc);
    CloseHandle(hDst);

    if (ok) wprintf(L"Извлечено из %ls: %llu байт\n", fullPath, total);
    return ok ? 0 : 6;
}

// list <файл>
static int CmdList(LPCWSTR filePath)
{
    WIN32_FIND_STREAM_DATA fsd;
    HANDLE hFind = FindFirstStreamW(filePath, FindStreamInfoStandard, &fsd, 0);
    if (hFind == INVALID_HANDLE_VALUE) {
        fwprintf(stderr, L"Нет потоков или ошибка: %lu\n", GetLastError());
        return 3;
    }

    wprintf(L"Потоки файла %ls:\n", filePath);
    do {
        wprintf(L"  %-20ls %llu байт\n",
                fsd.cStreamName,
                (unsigned long long)fsd.StreamSize.QuadPart);
    } while (FindNextStreamW(hFind, &fsd));

    FindClose(hFind);
    return 0;
}

// del <файл> <поток>
static int CmdDelete(LPCWSTR filePath, LPCWSTR streamName)
{
    wchar_t fullPath[PATH_BUF];
    if (!BuildStreamPath(fullPath, PATH_BUF, filePath, streamName)) {
        fwprintf(stderr, L"Путь слишком длинный\n");
        return 2;
    }

    if (!DeleteFileW(fullPath)) {
        fwprintf(stderr, L"Не удалить поток: %lu\n", GetLastError());
        return 3;
    }

    wprintf(L"Удалён поток %ls\n", fullPath);
    return 0;
}

static void PrintUsage(LPCWSTR prog)
{
    wprintf(L"Использование:\n");
    wprintf(L"  %ls write <файл> <поток> <источник>\n", prog);
    wprintf(L"  %ls read  <файл> <поток> <выходной_файл>\n", prog);
    wprintf(L"  %ls list  <файл>\n", prog);
    wprintf(L"  %ls del   <файл> <поток>\n", prog);
}

int wmain(int argc, wchar_t* argv[])
{
    if (argc < 2) { 
        PrintUsage(argv[0]); return 1; 
    }
    if (wcscmp(argv[1], L"write") == 0) {
        if (argc != 5) { PrintUsage(argv[0]); return 1; }
        return CmdWrite(argv[2], argv[3], argv[4]);
    }
    if (wcscmp(argv[1], L"read") == 0) {
        if (argc != 5) { PrintUsage(argv[0]); return 1; }
        return CmdRead(argv[2], argv[3], argv[4]);
    }
    if (wcscmp(argv[1], L"list") == 0) {
        if (argc != 3) { PrintUsage(argv[0]); return 1; }
        return CmdList(argv[2]);
    }
    if (wcscmp(argv[1], L"del") == 0) {
        if (argc != 4) { PrintUsage(argv[0]); return 1; }
        return CmdDelete(argv[2], argv[3]);
    }
    PrintUsage(argv[0]);
    return 1;
}
