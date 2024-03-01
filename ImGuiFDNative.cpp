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
	#define GETABS realpath
#endif

#if defined(_WIN32) || defined(__unix__)
#define DT_HAS_STAT
#endif


ds::string ImGuiFD::Native::getAbsolutePath(const char* path_) {
	if (strlen(path_) == 1 && path_[0] == '/')
		return "/";

	ds::string path = makePathStrOSComply(path_);

	if (path.size() == 0)
		path = ".";

	
#ifdef _WIN32
	ds::string out;
	out.resize(1024);
	DWORD ret = GetFullPathNameA(path.c_str(), (DWORD)out.size(), &out[0], NULL);
	if (ret == 0) return "?";
	if (ret > out.size()) {
		out.resize(ret);
		ret = GetFullPathNameA(path.c_str(), (DWORD)out.size(), &out[0], NULL);
		if (ret == 0 || ret > out.size()) return "?";
	}
#else
	char* out_ = realpath(path.c_str(), NULL);
	if (out_ == NULL) return "?";
	ds::string out(out_);
	free(out_);
#endif

	return out;
}

bool ImGuiFD::Native::fileExists(const char* path) {
	ds::string path_ = makePathStrOSComply(path);
#ifdef _MSC_VER
	struct _stat64 st;
	return __stat64(path_.c_str(), &st) == 0;
#else
	struct stat st;
	return stat(path_.c_str(), &st) == 0;
#endif
}

bool ImGuiFD::Native::rename(const char* name, const char* newName) {
	return ::rename(name, newName) == 0;
}

static void statDirEnt(ImGuiFD::DirEntry* entry) {
	ds::string path = ImGuiFD::Native::makePathStrOSComply(entry->path);

#ifdef DT_HAS_STAT
#ifdef _MSC_VER
	struct _stat64 st;
	int ret = __stat64(path.c_str(), &st);
#else
	struct stat st;
	int ret = lstat(path.c_str(), &st);
#endif

	if (ret != 0)
		return;

	entry->size = entry->isFolder? -1 : st.st_size;
	entry->lastModified = st.st_mtime;
	entry->creationDate = st.st_ctime;

#else
#ifdef _WIN32
	WIN32_FILE_ATTRIBUTE_DATA fInfo;
	GetFileAttributesEx(path.c_str(), GetFileExInfoStandard,&fInfo);
	entry->size = ((uint64_t)fInfo.nFileSizeHigh << 32) | fInfo.nFileSizeLow;
	entry->creationDate = ((uint64_t)fInfo.ftCreationTime.dwHighDateTime << 32) | fInfo.ftCreationTime.dwLowDateTime;
#endif
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


ds::vector<ImGuiFD::DirEntry> ImGuiFD::Native::loadDirEnts(const char* path_, bool* success) {
	*success = false;
	
	ds::string path = makePathStrOSComply(path_);

	ds::vector<DirEntry> entrys;

	auto hash = ImHashStr(path_);

#ifdef _WIN32
	if (strcmp(path_, "/") == 0) {
		char buf[1024];
		int byteLen = GetLogicalDriveStringsA(sizeof(buf), buf);
		if (byteLen <= 0) {
			*success = false;
			return entrys;
		}

		*success = true;
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
		WIN32_FIND_DATAA fdata;
		HANDLE findH = FindFirstFileA((path+"/*").c_str(), &fdata);
		if (findH == INVALID_HANDLE_VALUE) {  // error
			return {};
		}

		size_t i = 0;
		do {
			if (strcmp(fdata.cFileName, ".") == 0 || strcmp(fdata.cFileName, "..") == 0) continue;

			entrys.push_back(DirEntry());
			DirEntry* entry = &entrys.back();
			entry->id = (ImGuiID)((hash<<16)+i);
			entry->name = ImStrdup(fdata.cFileName);
			entry->dir = ImStrdup(path.c_str());
			entry->isFolder = !!(fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
			entry->path = combinePath(entry->dir, entry->name, entry->isFolder);
			statDirEnt(entry);

			i++;
		} while (FindNextFileA(findH, &fdata) != 0);

		if (GetLastError() != ERROR_NO_MORE_FILES) {
			FindClose(findH);
			return {};
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
	
	*success = true;
	return entrys;
}

bool ImGuiFD::Native::isValidDir(const char* dir) {
#ifdef DT_HAS_STAT
	struct stat info;
	int ret = stat(dir, &info);
	return ret == 0 && (info.st_mode & S_IFDIR);
#else
	return false;
#endif
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
	while (*path == '/')
		path++;

	size_t len = strlen(path);
	if (len == 2 && path[len-1] == ':') {
		return ds::string(path) + "/";
	}
#endif
	return path;
}