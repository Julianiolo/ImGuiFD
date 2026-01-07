#include "ImGuiFD.h"
#include "ImGuiFD_internal.h"

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
}

static ds::vector<wchar_t> wstr_buf;

static ds::ErrResult<wchar_t*> utf8ToWStrBuf(const char* str) {
    const size_t str_len = strlen(str);
    if (str_len > INT_MAX)
        return ds::Err(ds::format("str too long: %zu", strlen));

    const int size = MultiByteToWideChar(CP_UTF8, 0, str, (int)str_len, wstr_buf.data(), wstr_buf.size());
    if (size == 0) {
        return ds::Err(ds::format("utf8ToWStr failed: %s", getErrorMsg(GetLastError()).c_str()));
    }
    if(size < wstr_buf.size()) {
        return ds::Ok(wstr_buf.data());
    }

    wstr_buf.resize(size);
    const int size2 = MultiByteToWideChar(CP_UTF8, 0, str, (int)str_len, wstr_buf.data(), wstr_buf.size());
    if(size2 == 0 || size2 >= wstr_buf.size()) {
        return ds::Err(ds::format("utf8ToWStr failed: %s", getErrorMsg(GetLastError()).c_str()));
    }

    return ds::Ok(wstr_buf.data());
}
static ds::ErrResult<ds::string> wStrToUtf8(const wchar_t* str) {
    const size_t str_len = wcslen(str);

    if (str_len > INT_MAX)
        return ds::Err(ds::format("str too long: %zu", strlen));

    const int size = WideCharToMultiByte(CP_UTF8, WC_COMPOSITECHECK, str, -1, NULL, 0, NULL, NULL);
    if (size == 0) {
        return ds::Err(ds::format("utf8ToWStr failed: %s", getErrorMsg(GetLastError()).c_str()));
    }

    ds::string result(size);
    const int size2 = WideCharToMultiByte(CP_UTF8, WC_COMPOSITECHECK, str, -1, result.data.data(), result.size(), NULL, NULL);
    if(size2 == 0 || size2 > result.size()) {
        return ds::Err(ds::format("utf8ToWStr failed: %s", getErrorMsg(GetLastError()).c_str()));
    }

    return ds::Ok(ds::move(result));
}
static ds::ErrResult<HANDLE> FindFirstFileUtf8(const char* path, _Out_ LPWIN32_FIND_DATAW lpFindFileData) {
    auto res = utf8ToWStrBuf(path);
    if(res.has_err())
        return res.err_prop();
    wchar_t* path_w = res.value();
    HANDLE findH = FindFirstFileW(path_w, lpFindFileData);
    if (findH == INVALID_HANDLE_VALUE) {  // error
        return ds::Err(ds::format("FindFirstFileW failed: %s", getErrorMsg(GetLastError()).c_str()));
    }
    return ds::Ok(findH);
}
#endif

ds::ErrResult<ds::string> ImGuiFD::Native::getAbsolutePath(const char* path_) {
	if (path_[0] == '/' && path_[0] == 0)  // path_ == "/"
		return ds::Ok("/");

	ds::string path = makePathStrOSComply(path_);

	if (path.size() == 0)
		path = ".";

	
#ifdef _WIN32
    auto res = utf8ToWStrBuf(path.c_str());
    if(res.has_err())
        return res.err_prop();
    wchar_t* path_w = res.value();

	ds::vector<wchar_t> path_full(1024);
	DWORD ret = GetFullPathNameW(path_w, (DWORD)path_full.size(), path_full.data(), NULL);
	if (ret == 0)
        return ds::Err(ds::format("getAbsolutePath(%s) failed: %s", path_, getErrorMsg(GetLastError()).c_str()));
	if (ret > path_full.size()) {
        path_full.resize((size_t)ret);
		ret = GetFullPathNameW(path_w, (DWORD)path_full.size(), path_full.data(), NULL);
		if (ret == 0 || ret > path_full.size())
            return ds::Err(ds::format("getAbsolutePath(%s) failed: %s", path_, getErrorMsg(GetLastError()).c_str()));
	}

    auto out_ = wStrToUtf8(path_full.data());
    if(out_.has_err());
        out_.err_prop();
    ds::string out = move(out_.value());
#else
	char* out_ = realpath(path.c_str(), NULL);
	if (out_ == NULL) return "?";
	ds::string out(out_);
	free(out_);
#endif

	return ds::Ok(out);
}

bool ImGuiFD::Native::fileExists(const char* path) {
	ds::string path_ = makePathStrOSComply(path);
	struct stat st;
	return stat(path_.c_str(), &st) == 0 && (st.st_mode & S_IFREG);
}

bool ImGuiFD::Native::rename(const char* name, const char* newName) {
	return ::rename(name, newName) == 0;
}

static void statDirEnt(ImGuiFD::DirEntry* entry) {
	ds::string path = ImGuiFD::Native::makePathStrOSComply(entry->path);

#ifdef __unix__
	struct stat st;
	int ret = lstat(path.c_str(), &st);

	if (ret != 0)
		return;

	entry->size = entry->isFolder? -1 : st.st_size;
	entry->lastModified = st.st_mtime;
	entry->creationDate = st.st_ctime;
#elif defined(_WIN32)
    auto res = utf8ToWStrBuf(path.c_str());
    if(res.has_err())
        return;
    wchar_t* path_w = res.value();

	WIN32_FILE_ATTRIBUTE_DATA fInfo;
	if(GetFileAttributesExW(path_w, GetFileExInfoStandard,&fInfo) == 0)
        return;
	entry->size = ((uint64_t)fInfo.nFileSizeHigh << 32) | fInfo.nFileSizeLow;
	entry->creationDate = ((uint64_t)fInfo.ftCreationTime.dwHighDateTime << 32) | fInfo.ftCreationTime.dwLowDateTime;
#else
#error not implemented
#endif
}

static char* combinePath(const char* dir, const char* fname, bool isFolder) {
	const size_t dir_len = strlen(dir);
	const size_t fname_len = strlen(fname);
	char* out = (char*)IM_ALLOC(dir_len+1+fname_len+1+1);
	IM_ASSERT(out);
	strcpy(out, dir);
	if (dir_len > 0 && dir[dir_len - 1] != '/')
		strcat(out, "/");
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


ds::ErrResult<ds::vector<ImGuiFD::DirEntry>> ImGuiFD::Native::loadDirEnts(const char* path_) {
	ds::string path = makePathStrOSComply(path_);

	ds::vector<DirEntry> entrys;

	auto hash = ImHashStr(path_);

#ifdef _WIN32
	if (strcmp(path_, "/") == 0) {
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

			entry.name = ImStrdup(name.c_str());
			entry.dir = ImStrdup("/");
			entry.path = ImStrdup((ds::string("/") + entry.name + "/").c_str());

			entry.isFolder = true;
			entry.id = id;

			statDirEnt(&entry);

			off += strlen(buf+off)+1;
			id++;
		}
	}
	else {
		WIN32_FIND_DATAW fdata;
		auto ret = FindFirstFileUtf8((path+"/*").c_str(), &fdata);
        if(ret.has_err()) return ret.err_prop();
		HANDLE findH = ret.value();

		size_t i = 0;
		do {
            // skip . and ..
			if (fdata.cFileName[0] == '.' && (fdata.cFileName[1] == 0 || (fdata.cFileName[1] == '.' && fdata.cFileName[2] == 0))) continue;

			entrys.push_back(DirEntry());
			DirEntry* entry = &entrys.back();
			entry->id = (ImGuiID)((hash<<16)+i);
			entry->name = ImStrdup(fdata.cFileName);
			entry->dir = ImStrdup(path.c_str());
			entry->isFolder = !!(fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
			entry->path = combinePath(entry->dir, entry->name, entry->isFolder);
			statDirEnt(entry);

			i++;
		} while (FindNextFileW(findH, &fdata) != 0);

        auto err = GetLastError();
		if (err != ERROR_NO_MORE_FILES) {
			FindClose(findH);
			return ds::Err(ds::format("FindNextFileW failed: %s", getErrorMsg(err).c_str()));
		}

		FindClose(findH);
	}
#else

	dirent** namelist = 0; // pointer for array of dirent*
	const int numRead = scandir(path.c_str(), &namelist, 0, ::alphasort);

	if (namelist == NULL || numRead < 0) // couldn't read directory
		return entrys;

	
	for (int i = 0; i<numRead; i++) {
		dirent* de = namelist[i];
		IM_ASSERT(de != NULL);
		if (de->d_name[0] != '.' || (strcmp(de->d_name,".") != 0 && strcmp(de->d_name,"..") != 0)) {
			entrys.push_back(DirEntry());
			DirEntry* entry = &entrys.back();
			entry->id = (hash<<16)+i;
			entry->name = ImStrdup(de->d_name);
			entry->dir = ImStrdup(path.c_str());
			entry->isFolder = de->d_type == DT_DIR;
			entry->path = combinePath(entry->dir, entry->name, entry->isFolder);

			statDirEnt(entry);
		}
		free(namelist[i]);
	}
	free(namelist);
#endif

	if(entrys.size() > 1)
		qsort(&entrys[0], entrys.size(), sizeof(entrys[0]), cmpEntrys);
	
	return ds::Ok(move(entrys));
}

bool ImGuiFD::Native::isValidDir(const char* dir) {
	struct stat info;
	int ret = stat(makePathStrOSComply(dir).c_str(), &info);
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

ds::string ImGuiFD::Native::makePathStrOSComply(const char* path) {
#ifdef _WIN32
    // remove root '/'
    IM_ASSERT(path[0] == '/');
    path++;
    IM_ASSERT(path[0] != '/');

	size_t len = strlen(path);
	if (len == 2 && path[len-1] == ':') {
		return ds::string(path) + "/";
	}
#endif
	return path;
}
