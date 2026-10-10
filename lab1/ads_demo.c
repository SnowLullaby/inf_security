#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <io.h>
#include <fcntl.h>

#define PATH_BUF   32768
#define CHUNK_SIZE 65536

static BOOL BuildStreamPath(wchar_t* out, size_t outSize,
                            LPCWSTR filePath, LPCWSTR streamName)
{
    return swprintf(out, outSize, L"%ls:%ls", filePath, streamName) >= 0;
}

static HANDLE OpenSource(LPCWSTR path, BOOL* isStd)
{
    if (wcscmp(path, L"-") == 0) {
        *isStd = TRUE;
        _setmode(_fileno(stdin), _O_BINARY);
        return GetStdHandle(STD_INPUT_HANDLE);
    }
    *isStd = FALSE;
    return CreateFileW(path, GENERIC_READ, FILE_SHARE_READ,
                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
}

static HANDLE OpenDest(LPCWSTR path, BOOL* isStd)
{
    if (wcscmp(path, L"-") == 0) {
        *isStd = TRUE;
        _setmode(_fileno(stdout), _O_BINARY);
        return GetStdHandle(STD_OUTPUT_HANDLE);
    }
    *isStd = FALSE;
    return CreateFileW(path, GENERIC_WRITE, 0,
                       NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

// write <file> <stream> <source|->
static int CmdWrite(LPCWSTR filePath, LPCWSTR streamName, LPCWSTR sourcePath)
{
    wchar_t fullPath[PATH_BUF];
    if (!BuildStreamPath(fullPath, PATH_BUF, filePath, streamName)) {
        fwprintf(stderr, L"Path too long\n");
        return 2;
    }

    BOOL srcIsStd = FALSE;
    HANDLE hSrc = OpenSource(sourcePath, &srcIsStd);
    if (hSrc == INVALID_HANDLE_VALUE || hSrc == NULL) {
        fwprintf(stderr, L"Cannot open source: %lu\n", GetLastError());
        return 3;
    }

    HANDLE hDst = CreateFileW(fullPath, GENERIC_WRITE, 0,
                              NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hDst == INVALID_HANDLE_VALUE) {
        fwprintf(stderr, L"Cannot open stream: %lu\n", GetLastError());
        if (!srcIsStd) CloseHandle(hSrc);
        return 4;
    }

    BYTE* buf = (BYTE*)malloc(CHUNK_SIZE);
    if (!buf) {
        if (!srcIsStd) CloseHandle(hSrc);
        CloseHandle(hDst);
        return 5;
    }

    DWORD read = 0, written = 0;
    ULONGLONG total = 0;
    BOOL ok = TRUE;

    while (ok) {
        if (!ReadFile(hSrc, buf, CHUNK_SIZE, &read, NULL)) {
            DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE) {
                break;
            }
            fwprintf(stderr, L"Read error: %lu\n", err);
            ok = FALSE; break;
        }
        if (read == 0) break;

        if (!WriteFile(hDst, buf, read, &written, NULL) || written != read) {
            fwprintf(stderr, L"Write error: %lu\n", GetLastError());
            ok = FALSE; break;
        }
        total += written;
    }

    free(buf);
    if (!srcIsStd) CloseHandle(hSrc);
    CloseHandle(hDst);

    if (ok) fwprintf(stderr, L"Written to %ls: %llu bytes\n", fullPath, total);
    return ok ? 0 : 6;
}

// read <file> <stream> <output_file|->
static int CmdRead(LPCWSTR filePath, LPCWSTR streamName, LPCWSTR outputPath)
{
    wchar_t fullPath[PATH_BUF];
    if (!BuildStreamPath(fullPath, PATH_BUF, filePath, streamName)) {
        fwprintf(stderr, L"Path too long\n");
        return 2;
    }

    HANDLE hSrc = CreateFileW(fullPath, GENERIC_READ, FILE_SHARE_READ,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSrc == INVALID_HANDLE_VALUE) {
        fwprintf(stderr, L"Cannot open stream: %lu\n", GetLastError());
        return 3;
    }

    BOOL dstIsStd = FALSE;
    HANDLE hDst = OpenDest(outputPath, &dstIsStd);
    if (hDst == INVALID_HANDLE_VALUE || hDst == NULL) {
        fwprintf(stderr, L"Cannot create file: %lu\n", GetLastError());
        CloseHandle(hSrc);
        return 4;
    }

    BYTE* buf = (BYTE*)malloc(CHUNK_SIZE);
    if (!buf) {
        CloseHandle(hSrc);
        if (!dstIsStd) CloseHandle(hDst);
        return 5;
    }

    DWORD read = 0, written = 0;
    ULONGLONG total = 0;
    BOOL ok = TRUE;

    while (ok) {
        if (!ReadFile(hSrc, buf, CHUNK_SIZE, &read, NULL)) {
            fwprintf(stderr, L"Read error: %lu\n", GetLastError());
            ok = FALSE; break;
        }
        if (read == 0) break;

        if (!WriteFile(hDst, buf, read, &written, NULL) || written != read) {
            DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE) {
                break;
            }
            fwprintf(stderr, L"Write error: %lu\n", err);
            ok = FALSE; break;
        }
        total += written;
    }

    free(buf);
    CloseHandle(hSrc);
    if (!dstIsStd) CloseHandle(hDst);

    if (ok) fwprintf(stderr, L"Extracted from %ls: %llu bytes\n", fullPath, total);
    return ok ? 0 : 6;
}

// list <file>
static int CmdList(LPCWSTR filePath)
{
    WIN32_FIND_STREAM_DATA fsd;
    HANDLE hFind = FindFirstStreamW(filePath, FindStreamInfoStandard, &fsd, 0);
    if (hFind == INVALID_HANDLE_VALUE) {
        fwprintf(stderr, L"No streams or error: %lu\n", GetLastError());
        return 3;
    }

    wprintf(L"Streams of file %ls:\n", filePath);
    do {
        wprintf(L"  %-20ls %llu bytes\n",
                fsd.cStreamName,
                (unsigned long long)fsd.StreamSize.QuadPart);
    } while (FindNextStreamW(hFind, &fsd));

    FindClose(hFind);
    return 0;
}

// del <file> <stream>
static int CmdDelete(LPCWSTR filePath, LPCWSTR streamName)
{
    wchar_t fullPath[PATH_BUF];
    if (!BuildStreamPath(fullPath, PATH_BUF, filePath, streamName)) {
        fwprintf(stderr, L"Path too long\n");
        return 2;
    }

    if (!DeleteFileW(fullPath)) {
        fwprintf(stderr, L"Cannot delete stream: %lu\n", GetLastError());
        return 3;
    }

    wprintf(L"Stream deleted: %ls\n", fullPath);
    return 0;
}

static void PrintUsage(LPCWSTR prog)
{
    wprintf(L"Usage:\n");
    wprintf(L"  %ls write <file> <stream> <source|->\n", prog);
    wprintf(L"  %ls read  <file> <stream> <output_file|->\n", prog);
    wprintf(L"  %ls list  <file>\n", prog);
    wprintf(L"  %ls del   <file> <stream>\n", prog);
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
