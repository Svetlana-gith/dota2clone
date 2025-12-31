#include <Windows.h>
#include <DbgHelp.h>
#include <cstdio>
#include <cstdint>
#include <string>

static std::string WideToUtf8(const wchar_t* w) {
    if (!w) return {};
    char buf[2048]{};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, buf, (int)sizeof(buf), nullptr, nullptr);
    if (n <= 0) return {};
    return std::string(buf);
}

static const MINIDUMP_STRING* DumpString(void* base, RVA rva) {
    if (!base || rva == 0) return nullptr;
    return reinterpret_cast<const MINIDUMP_STRING*>(reinterpret_cast<const uint8_t*>(base) + rva);
}

int main(int argc, char** argv) {
    const char* path = (argc >= 2) ? argv[1] : "runlogs/WorldEditor.dmp";
    uint64_t addr = 0;
    if (argc >= 3) {
        // Accept hex or decimal.
        addr = std::strtoull(argv[2], nullptr, 0);
    }

    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        std::fprintf(stderr, "Failed to open dump: %s (GetLastError=%lu)\n", path, GetLastError());
        return 2;
    }

    HANDLE hMap = CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMap) {
        std::fprintf(stderr, "CreateFileMapping failed (GetLastError=%lu)\n", GetLastError());
        CloseHandle(hFile);
        return 3;
    }

    void* view = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    if (!view) {
        std::fprintf(stderr, "MapViewOfFile failed (GetLastError=%lu)\n", GetLastError());
        CloseHandle(hMap);
        CloseHandle(hFile);
        return 4;
    }

    // Exception stream
    {
        PMINIDUMP_DIRECTORY dir = nullptr;
        PVOID stream = nullptr;
        ULONG streamSize = 0;
        if (MiniDumpReadDumpStream(view, ExceptionStream, &dir, &stream, &streamSize) && stream) {
            auto* exc = reinterpret_cast<MINIDUMP_EXCEPTION_STREAM*>(stream);
            const auto& er = exc->ExceptionRecord;
            std::printf("ExceptionCode=0x%08X\n", er.ExceptionCode);
            std::printf("ExceptionFlags=0x%08X\n", er.ExceptionFlags);
            std::printf("ExceptionAddress=0x%llX\n", static_cast<unsigned long long>(er.ExceptionAddress));
            if (addr == 0) {
                addr = static_cast<uint64_t>(er.ExceptionAddress);
            }
        } else {
            std::printf("No ExceptionStream in dump.\n");
        }
    }

    if (addr != 0) {
        std::printf("LookupAddress=0x%llX\n", static_cast<unsigned long long>(addr));
    }

    // Module list
    {
        PMINIDUMP_DIRECTORY dir = nullptr;
        PVOID stream = nullptr;
        ULONG streamSize = 0;
        if (MiniDumpReadDumpStream(view, ModuleListStream, &dir, &stream, &streamSize) && stream) {
            auto* mods = reinterpret_cast<MINIDUMP_MODULE_LIST*>(stream);
            std::printf("ModuleCount=%lu\n", mods->NumberOfModules);
            const MINIDUMP_MODULE* hit = nullptr;
            std::string hitName;
            for (ULONG i = 0; i < mods->NumberOfModules; ++i) {
                const MINIDUMP_MODULE& m = mods->Modules[i];
                const uint64_t base = m.BaseOfImage;
                const uint64_t end = base + static_cast<uint64_t>(m.SizeOfImage);
                if (addr != 0 && addr >= base && addr < end) {
                    hit = &m;
                    const MINIDUMP_STRING* s = DumpString(view, m.ModuleNameRva);
                    hitName = s ? WideToUtf8(s->Buffer) : std::string("<unknown>");
                    break;
                }
            }

            if (hit) {
                const uint64_t base = hit->BaseOfImage;
                const uint64_t off = (addr != 0) ? (addr - base) : 0;
                std::printf("HitModule=%s\n", hitName.c_str());
                std::printf("HitModuleBase=0x%llX\n", static_cast<unsigned long long>(base));
                std::printf("HitModuleSize=%lu\n", hit->SizeOfImage);
                std::printf("HitModuleOffset=0x%llX\n", static_cast<unsigned long long>(off));
            } else {
                std::printf("HitModule=<not found>\n");
            }
        } else {
            std::printf("No ModuleListStream in dump.\n");
        }
    }

    UnmapViewOfFile(view);
    CloseHandle(hMap);
    CloseHandle(hFile);
    return 0;
}


