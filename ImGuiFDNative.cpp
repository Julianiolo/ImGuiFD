#include "ImGuiFD.h"
#include "ImGuiFD_internal.h"

#include <imgui.h>
#include <stdint.h>
#include <sys/stat.h>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>

    #include <direct.h> // for _getcwd _fullpath

    #define GETCWD _getcwd
#else
    #include <dirent.h>
    #include <stdlib.h>
    #include <limits.h>
    #include <errno.h>

    #define GETCWD getcwd
#endif

#ifdef _WIN32

ds::string getErrorMsg(DWORD errorCode) {
    ds::string errMsg;
    
    // https://stackoverflow.com/questions/1387064/how-to-get-the-error-message-from-the-error-code-returned-by-getlasterror
    LPSTR messageBuffer = nullptr;

    //Ask Win32 to give us the string version of that message ID.
    //The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

    //Copy the error message into a std::string.
    errMsg = ds::string(messageBuffer);

    //Free the Win32's string's buffer.
    LocalFree(messageBuffer);
    return ds::move(errMsg);
}

static ds::vector<wchar_t> wstr_buf;

static ds::ErrResult<const wchar_t*> toWinPath(const char* str) {
    const int size = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    if (size == 0) {
        return ds::Err(ds::format("utf8ToWStr failed: %s", getErrorMsg(GetLastError()).c_str()));
    }

    if(wstr_buf.size() < size)
        wstr_buf.resize(size);

    const int size2 = MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr_buf.data(), wstr_buf.size());
    if(size2 == 0 || size2 > wstr_buf.size()) {
        return ds::Err(ds::format("utf8ToWStr failed: %s", getErrorMsg(GetLastError()).c_str()));
    }

    IM_ASSERT(wstr_buf.size() > 0 && wstr_buf[wstr_buf.size()-1] == 0);

    return ds::Ok(wstr_buf.data());
}
static ds::ErrResult<ds::string> fromWinPath(const wchar_t* str) {
    const int size = WideCharToMultiByte(CP_UTF8, WC_COMPOSITECHECK, str, -1, NULL, 0, NULL, NULL);
    if (size == 0) {
        return ds::Err(ds::format("utf8ToWStr failed: %s", getErrorMsg(GetLastError()).c_str()));
    }

    ds::string result;
    result.resize(size-1);
    const int size2 = WideCharToMultiByte(CP_UTF8, WC_COMPOSITECHECK, str, -1, result.data(), result.size()+1, NULL, NULL);
    if(size2 == 0 || size2 > result.size()+1) {
        return ds::Err(ds::format("utf8ToWStr failed: %s", getErrorMsg(GetLastError()).c_str()));
    }

    IM_ASSERT(result.data()[result.size()] == 0);

    return ds::Ok(ds::move(result));
}
static ds::vector<char> str_buf;
static ds::ErrResult<const char*> fromWinPath_Buf(const wchar_t* str) {
    const int size = WideCharToMultiByte(CP_UTF8, WC_COMPOSITECHECK, str, -1, NULL, 0, NULL, NULL);
    if (size == 0) {
        return ds::Err(ds::format("utf8ToWStr failed: %s", getErrorMsg(GetLastError()).c_str()));
    }

    if(str_buf.size() < size)
        str_buf.resize(size);
    const int size2 = WideCharToMultiByte(CP_UTF8, WC_COMPOSITECHECK, str, -1, str_buf.data(), str_buf.size(), NULL, NULL);
    if(size2 == 0 || size2 > size) {
        return ds::Err(ds::format("utf8ToWStr failed: %s", getErrorMsg(GetLastError()).c_str()));
    }

    IM_ASSERT(str_buf[size-1] == 0);

    return ds::Ok(str_buf.data());
}
static const char* backupWStrToUtf8_Buf(const wchar_t* str) {
    size_t len = wcslen(str)+1;
    if(str_buf.size() < len+1)
        str_buf.resize(len+1);
    for(size_t i = 0; i<len; i++) {
        wchar_t c = str[i];
        if(c < 0 || c > 127)
            c = '?';
        str_buf[i] = (char)c;
    }
    return str_buf.data();
}

static ds::ErrResult<HANDLE> FindFirstFileUtf8(const char* path, _Out_ LPWIN32_FIND_DATAW lpFindFileData) {
    auto res = toWinPath(path);
    if(res.has_err())
        return res.error_prop();
    const wchar_t* path_w = res.value();
    HANDLE findH = FindFirstFileW(path_w, lpFindFileData);
    if (findH == INVALID_HANDLE_VALUE) {  // error
        return ds::Err(ds::format("FindFirstFileW failed: %s", getErrorMsg(GetLastError()).c_str()));
    }
    return ds::Ok(findH);
}

static double winFileTimeToUnix(const FILETIME& ft) {
    ULARGE_INTEGER li = { 0 };
    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    if (li.QuadPart == 0) return NAN;
    return (double)li.QuadPart / 1e7 - 11644473600LL;
}
#endif

bool ImGuiFD::Native::isAbsolutePath(const char* path) {
    IM_ASSERT(path != NULL);
#ifdef _WIN32
    return true;  // TODO
#else
    return path[0] == '/';
#endif
}

ds::ErrResult<ds::string> ImGuiFD::Native::getAbsolutePath(const char* path_) {
    if (path_[0] == '/' && path_[0] == 0)  // path_ == "/"
        return ds::Ok<const char*>("/");

    
#ifdef _WIN32
    auto res = toWinPath(path_);
    if(res.has_err())
        return res.error_prop();
    const wchar_t* path_w = res.value();

    static ds::vector<wchar_t> path_full(256);
    DWORD ret = GetFullPathNameW(path_w, (DWORD)path_full.size(), path_full.data(), NULL);
    if (ret == 0)
        return ds::Err(ds::format("getAbsolutePath(%s) failed: %s", path_, getErrorMsg(GetLastError()).c_str()));
    if (ret > path_full.size()) {
        path_full.resize((size_t)ret);
        ret = GetFullPathNameW(path_w, (DWORD)path_full.size(), path_full.data(), NULL);
        if (ret == 0 || ret > path_full.size())
            return ds::Err(ds::format("getAbsolutePath(%s) failed: %s", path_, getErrorMsg(GetLastError()).c_str()));
    }

    auto out_ = fromWinPath(path_full.data());
    if(out_.has_err())
        out_.error_prop();
    ds::string out = move(out_.value());
#else
    char* out_ = realpath(path_, NULL);
    if (out_ == NULL) return ds::Err(ds::format("realpath(%s) failed: %s", path_, strerror(errno)));
    ds::string out(out_);
    free(out_);
#endif

    return ds::Ok(ds::move(out));
}

bool ImGuiFD::Native::fileExists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && (st.st_mode & S_IFREG);
}

bool ImGuiFD::Native::rename(const char* name, const char* newName) {
    return ::rename(name, newName) == 0;
}

static void statDirEnt(ImGuiFD::DirEntry* entry) {
#ifdef __unix__
    struct stat st;
    int ret = lstat(entry->path, &st);

    if (ret != 0) {
        if(entry->error == NULL) {
            entry->error = ds::format_("lstat: %s", strerror(errno));
        }
        return;
    }

    entry->size = entry->isFolder ? (uint64_t)-1 : st.st_size;
    entry->lastAccessed = st.st_atime;
    entry->lastModified = st.st_mtime;
#elif defined(_WIN32)
    auto res = toWinPath(entry->path);
    if(res.has_err())
        return;
    const wchar_t* path_w = res.value();

    WIN32_FILE_ATTRIBUTE_DATA fInfo;
    if(GetFileAttributesExW(path_w, GetFileExInfoStandard,&fInfo) == 0) {
        if(entry->error == NULL) {
            entry->error = ds::format_("GetFileAttributesExW: %s", getErrorMsg(GetLastError()));
        }
        return;
    }
    entry->size = ((uint64_t)fInfo.nFileSizeHigh << 32) | fInfo.nFileSizeLow;
    entry->lastAccessed = winFileTimeToUnix(fInfo.ftLastAccessTime);
    entry->lastModified = winFileTimeToUnix(fInfo.ftLastWriteTime);
#else
#error not implemented
#endif
}

static char* combinePath(const char* dir, const char* fname, bool isFolder) {
    const size_t dir_len = strlen(dir);
    const size_t fname_len = strlen(fname);
    char* out = (char*)IMFD_ALLOC(dir_len+1+fname_len+1+1);
    IM_ASSERT(out);
    strcpy(out, dir);
    if (dir_len > 0) {
        IM_ASSERT(dir[dir_len - 1] == '/');
    }
    strcat(out, fname);
    if(isFolder)
        strcat(out, "/");
    return out;
}

static int cmpEntrys(const void* a_, const void* b_) {
    const ImGuiFD::DirEntry* a = (const ImGuiFD::DirEntry*)a_;
    const ImGuiFD::DirEntry* b = (const ImGuiFD::DirEntry*)b_;
    if (ImGuiFD::settings.showDirFirst) {
        if (a->isFolder && !b->isFolder)
            return -1;
        if (!a->isFolder && b->isFolder)
            return 1;
    }
    return strcmp(a->name, b->name);
}


ds::ErrResult<ds::vector<ImGuiFD::DirEntry>> ImGuiFD::Native::loadDirEntrys(const char* dir) {
    ds::vector<DirEntry> entrys;

    auto hash = ImHashStr(dir);

#ifdef _WIN32
    if (strcmp(path, "/") == 0) {
        // handle drive letters

        char buf[1024];
        int byteLen = GetLogicalDriveStringsA(sizeof(buf), buf);
        if (byteLen <= 0) {
            return ds::Err(ds::format("GetLogicalDriveStringsA failed: %s", getErrorMsg(GetLastError()).c_str()));
        }

        size_t off = 0;
        ImGuiID id = 0;
        while (buf[off] != 0) {
            entrys.push_back(DirEntry());
            auto& entry = entrys.back();

            ds::string name = ds::string(buf + off);
            while (name.size() > 0 && name[name.size()-1] == '\\')
                name = name.substr(0, name.size()-1);

            entry.dir = dir;
            entry.path = ImStrdup((ds::string("/") + name + "/").c_str());
            entry.name = entry.path+1;

            entry.isFolder = true;
            entry.id = id;

            statDirEnt(&entry);

            off += strlen(buf+off)+1;
            id++;
        }
    }
    else {
        // handle normal folder
        WIN32_FIND_DATAW fdata;
        auto ret = FindFirstFileUtf8((ds::string(path)+"/*").c_str(), &fdata);
        if(ret.has_err()) return ret.error_prop();
        HANDLE findH = ret.value();

        size_t i = 0;
        do {
            // skip . and ..
            if (fdata.cFileName[0] == '.' && (fdata.cFileName[1] == 0 || (fdata.cFileName[1] == '.' && fdata.cFileName[2] == 0))) continue;

            entrys.push_back(DirEntry());
            DirEntry* entry = &entrys.back();
            entry->id = (ImGuiID)((hash<<16)+i);
 
            auto ret = fromWinPath_Buf(fdata.cFileName);
            const char* name;
            if(ret.has_value()) {
                name = ret.value();
            } else {
                if(entry->error == NULL)
                    entry->error = ImStrdup(ret.error().c_str());
                name = backupWStrToUtf8_Buf(fdata.cFileName);
            }
            entry->isFolder = !!(fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
            entry->dir = dir;
            entry->path = combinePath(entry->dir, name, entry->isFolder);
            entry->name = entry->path + strlen(entry->dir);
            statDirEnt(entry);

            i++;
        } while (FindNextFileW(findH, &fdata) != 0);
        
        auto err = GetLastError();

        FindClose(findH);

        if (err != ERROR_NO_MORE_FILES) {
            return ds::Err(ds::format("FindNextFileW failed: %s", getErrorMsg(err).c_str()));
        }
    }
#else

    dirent** namelist = 0; // pointer for array of dirent*
    const int numRead = scandir(dir, &namelist, NULL, ::alphasort);

    if (namelist == NULL || numRead < 0) // couldn't read directory
        return ds::Err(ds::format("scandir failed: %s", strerror(errno)));;

    entrys.reserve(numRead);
    for (int i = 0; i<numRead; i++) {
        dirent* de = namelist[i];
        IM_ASSERT(de != NULL);
        if (de->d_name[0] != '.' || (strcmp(de->d_name,".") != 0 && strcmp(de->d_name,"..") != 0)) {
            entrys.push_back(DirEntry());
            DirEntry* entry = &entrys.back();
            entry->id = (hash<<16)+i;
            entry->isFolder = de->d_type == DT_DIR;
            entry->dir = dir;
            entry->path = combinePath(entry->dir, de->d_name, entry->isFolder);
            entry->name = entry->path + strlen(entry->dir);

            statDirEnt(entry);
        }
        free(namelist[i]);
    }
    free(namelist);
#endif

    if(entrys.size() > 1)
        qsort(&entrys[0], entrys.size(), sizeof(entrys[0]), cmpEntrys);
    
    return ds::Ok(ds::move(entrys));
}

bool ImGuiFD::Native::isValidDir(const char* dir) {
    struct stat info;
    int ret = stat(dir, &info);
    return ret == 0 && (info.st_mode & S_IFDIR);
}

bool ImGuiFD::Native::makeFolder(const char* path) {
    int status;
#if defined(_MSC_VER) || defined(__MINGW32__)
    status = _mkdir(makePathStrOSComply(path).c_str());
#else
    status = mkdir(path, S_IRWXU);
#endif

    return status == 0;
}

// ds::string ImGuiFD::Native::makePathStrOSComply(const char* path) {
// #ifdef _WIN32
//     // remove root '/'
//     if(path[0] == '/')
//         path++;
//     IM_ASSERT(path[0] != '/');

//     size_t len = strlen(path);
//     if (len == 2 && path[len-1] == ':') {
//         return ds::string(path) + "/";
//     }
// #endif
//     return path;
// }
