// Minimal minidump → exception + thread-context dumper. Avoid needing
// windbg/cdb installs by reading the dump's ExceptionStream + ThreadStream
// directly via DbgHelp.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <DbgHelp.h>
#include <cstdio>
#include <cstdint>

#pragma comment(lib, "DbgHelp.lib")

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: dump_reader <dumpfile.dmp>\n");
        return 2;
    }
    HANDLE hFile = CreateFileA(argv[1], GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "open failed: %lu\n", GetLastError());
        return 3;
    }
    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    void* base = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    if (!base) {
        fprintf(stderr, "MapViewOfFile failed: %lu\n", GetLastError());
        return 4;
    }

    MINIDUMP_DIRECTORY* dir = NULL;
    void* stream = NULL;
    ULONG ssize = 0;

    if (MiniDumpReadDumpStream(base, ExceptionStream, &dir, &stream, &ssize)) {
        MINIDUMP_EXCEPTION_STREAM* es = (MINIDUMP_EXCEPTION_STREAM*)stream;
        printf("=== Exception ===\n");
        printf("  ThreadId:         0x%X (%u)\n", es->ThreadId, es->ThreadId);
        printf("  ExceptionCode:    0x%08X\n", es->ExceptionRecord.ExceptionCode);
        printf("  ExceptionAddress: 0x%llX\n",
            (unsigned long long)es->ExceptionRecord.ExceptionAddress);
        if (es->ExceptionRecord.NumberParameters > 0) {
            printf("  Params:");
            for (DWORD i = 0; i < es->ExceptionRecord.NumberParameters; ++i)
                printf(" 0x%llX",
                    (unsigned long long)es->ExceptionRecord.ExceptionInformation[i]);
            printf("\n");
        }

        // Thread context — registers at crash
        CONTEXT* ctx = (CONTEXT*)((BYTE*)base + es->ThreadContext.Rva);
        printf("\n=== Crash thread CONTEXT (x86) ===\n");
        printf("  Eip = 0x%08X\n", ctx->Eip);
        printf("  Esp = 0x%08X\n", ctx->Esp);
        printf("  Ebp = 0x%08X\n", ctx->Ebp);
        printf("  Eax = 0x%08X  Ebx = 0x%08X\n", ctx->Eax, ctx->Ebx);
        printf("  Ecx = 0x%08X  Edx = 0x%08X\n", ctx->Ecx, ctx->Edx);
        printf("  Esi = 0x%08X  Edi = 0x%08X\n", ctx->Esi, ctx->Edi);
    }

    // Module list — map crash address to a module
    if (MiniDumpReadDumpStream(base, ModuleListStream, &dir, &stream, &ssize)) {
        MINIDUMP_MODULE_LIST* ml = (MINIDUMP_MODULE_LIST*)stream;
        printf("\n=== Modules (%u) ===\n", ml->NumberOfModules);
        for (ULONG32 i = 0; i < ml->NumberOfModules; ++i) {
            MINIDUMP_MODULE* m = &ml->Modules[i];
            MINIDUMP_STRING* name = (MINIDUMP_STRING*)((BYTE*)base + m->ModuleNameRva);
            // narrow-print the wide name
            char buf[260] = {};
            int len = WideCharToMultiByte(CP_UTF8, 0, name->Buffer, name->Length / 2,
                buf, sizeof(buf) - 1, NULL, NULL);
            buf[len] = 0;
            printf("  [%2u] 0x%08llX-0x%08llX  %s\n", i,
                (unsigned long long)m->BaseOfImage,
                (unsigned long long)(m->BaseOfImage + m->SizeOfImage), buf);
        }
    }
    return 0;
}
