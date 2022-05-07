#include "ImGuiFD.h"
#include "ImGuiFD_internal.h"

#include <stdint.h>
#include <dirent.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <direct.h> // for _getcwd _fullpath

#define GETCWD _getcwd
#define GETABS _fullpath
#else
#define GETCWD getcwd
#define GETABS realpath
#endif

#if defined(_WIN32) || defined(__unix__)
#define DT_HAS_STAT
#endif


ds::string ImGuiFD::Native::getAbsolutePath(const char* path) {
	if (strlen(path) == 1 && path[0] == '/')
		return "/";

#ifdef _WIN32
	if (*path == '/')
		path++;
#endif

	ds::string out;
	out.data.resize(MAX_PATH_LEN + 1);
	GETABS(out.data.Data, path, MAX_PATH_LEN);
	return out;
}

void setupDirEnt(ImGuiFD::DirEntry* entry, size_t id, const dirent* de, const char* dir_) {
	entry->id = id;
	entry->name = ImStrdup(de->d_name);
	entry->dir = ImStrdup(dir_);
	entry->path = ImStrdup((ds::string(entry->dir) + "/" + entry->name).c_str());
	entry->isFolder = de->d_type == DT_DIR;


#ifdef DT_HAS_STAT
#ifdef _MSC_VER
	struct _stat64 st;
	__stat64(entry->path, &st);
#else
	struct stat st;
	stat(entry->path, &st);
#endif

	entry->size = entry->isFolder? -1 : st.st_size;
	entry->lastModified = st.st_mtime;
	entry->creationDate = st.st_ctime;

#else
#ifdef _WIN32
	WIN32_FILE_ATTRIBUTE_DATA fInfo;
	GetFileAttributesEx(entry->path.c_str(), GetFileExInfoStandard,&fInfo);
	entry->size = ((uint64_t)fInfo.nFileSizeHigh << 32) | fInfo.nFileSizeLow;
	entry->creationDate = ((uint64_t)fInfo.ftCreationTime.dwHighDateTime << 32) | fInfo.ftCreationTime.dwLowDateTime;
#endif
#endif
}

int alphaSortEx(const void* a, const void* b) {
	const dirent** a_ = (const dirent**)a;
	const dirent** b_ = (const dirent**)b;
	if (ImGuiFD::settings.showDirFirst) {
		if (a_[0]->d_type == DT_DIR && b_[0]->d_type != DT_DIR)
			return -1;
		if (a_[0]->d_type != DT_DIR && b_[0]->d_type == DT_DIR)
			return 1;
	}
	return ::alphasort(a_, b_);
}

ds::vector<ImGuiFD::DirEntry> ImGuiFD::Native::loadDirEnts(const char* path, bool* success, int (*compare)(const void* a, const void* b)) {
	if (compare == 0)
		compare = alphaSortEx;

	ds::vector<DirEntry> entrys;
	entrys.clear();

#ifdef _WIN32
	if (strlen(path) == 1 && path[0] == '/') {
		char buf[512];
		int byteLen = GetLogicalDriveStrings(sizeof(buf), buf);
		if (byteLen > 0) {
			*success = true;
			size_t off = 0;
			size_t id = 0;
			while (buf[off] != 0) {
				entrys.push_back(DirEntry());
				auto& entry = entrys.back();

				ds::string name = buf + off;
				while (name.c_str() > 0 && name[-1] == '\\')
					name = name.substr(0, -1);
				
				entry.name = ImStrdup(name.c_str());
				entry.dir = ImStrdup("/");
				entry.path = ImStrdup((ds::string("/") + entry.name).c_str());

				entry.isFolder = true;
				entry.id = id;
				
				off += strlen(buf+off)+1;
				id++;
			}
		}
		else {
			*success = false;
		}

		return entrys;
	}

	if (*path == '/')
		path++;
#endif



	dirent** namelist = 0; // pointer for array of dirent*
	int numRead = scandir(path, &namelist, 0, (int (*)(const dirent**,const dirent**))compare);
	//IM_ASSERT(namelist != 0); // check if scandir() put something into namelist

	if (namelist != NULL && numRead >= 0) {
		*success = true;
		for (size_t i = 0; i<numRead; i++) {
			IM_ASSERT(namelist[i] != NULL);
			if (namelist[i]->d_name[0] != '.' || (strcmp(namelist[i]->d_name,".") != 0 && strcmp(namelist[i]->d_name,"..") != 0)) {
				entrys.push_back(DirEntry());
				setupDirEnt(&entrys.back(), i+ImHashStr(path)<<12, namelist[i], path);
			}
			free(namelist[i]);
		}
		free(namelist);
	}
	else {
		// couldnt read directory
		*success = false;
	}
	
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