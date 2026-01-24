#ifndef __IMGUIFD_H__
#define __IMGUIFD_H__

#define IMGUIFD_VERSION "0.1.0 alpha"
#define IMGUIFD_VERSION_NUM 000100

// uncomment this for using stl internally
//#define IMGUIFD_ENABLE_STL 1

#ifndef IMFD_USE_MOVE
#define IMFD_USE_MOVE (__cplusplus >= 201103L)
#endif

#ifdef _MSC_VER
    #ifndef _CRT_SECURE_NO_WARNINGS
        #define _CRT_SECURE_NO_WARNINGS 1
    #endif
#endif


#include "imgui.h"
#include <stdint.h>
#include <math.h>

/*

Example Filters:
    NULL
    "*"
    "*.txt"
    "."
    "{*},{*.txt,*.text}"
    "{*},{Text Files:*.txt,*.text}"
*/

enum ImGuiFDMode_ {
    ImGuiFDMode_LoadFile = 0,
    ImGuiFDMode_SaveFile,
    ImGuiFDMode_OpenDir,
};
typedef uint8_t ImGuiFDMode;

enum ImGuiFDDialogFlags_ {
    ImGuiFDDialogFlags_Modal = 1<<0
};
typedef int ImGuiFDDialogFlags;

namespace ImGuiFD {
    struct DirEntry {
        ImGuiID id;

        const char* error; // may be NULL

        // all of these may be NULL
        const char* dir;
        const char* path;
        const char* name;  // this points inside of the path string
        
        bool isFolder;
        uint64_t size;        // (uint64_t)-1 means not available
        double lastAccessed;  // NAN means not available
        double lastModified;  // NAN means not available

        inline DirEntry() : id((ImGuiID)-1), error(NULL), dir(NULL), path(NULL), name(NULL), isFolder(false), size((uint64_t)-1), lastAccessed(NAN), lastModified(NAN) {}
        DirEntry(const DirEntry& src);
        DirEntry& operator=(const DirEntry& src);
#if IMFD_USE_MOVE
        DirEntry(DirEntry&& src) noexcept;
        DirEntry& operator=(DirEntry&& src) noexcept;
#endif
        ~DirEntry();
    };

    struct GlobalSettings {
        bool showDirFirst = true;
        bool adjustIconWidth = true;
        ImVec4 iconTextCol = ImVec4(.08f, .08f, .78f, 1.0f);
        const float iconModeSizeDef = 100;
        float iconModeSize = iconModeSizeDef;

        enum DisplayMode_ {
            DisplayMode_List  = 0,
            DisplayMode_Icons = 1
        };
        uint8_t displayMode = DisplayMode_Icons;


        ImVec4 descTextCol = ImVec4(.7f, .7f, .7f, 1.0f);
        ImVec4 errorTextCol = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);

        bool asciiArtIcons = true;
    };

    static GlobalSettings settings;

    struct FileData {
        struct Image {
            ImTextureID texID;
            int width;
            int height;
            int origWidth;
            int origHeight;

            void* userData;

            bool dimDone;
            bool loadDone;
            Image() : texID(0), width(0), height(0), origWidth(0), origHeight(0), userData(NULL), dimDone(false), loadDone(false) {}
        }* thumbnail;

        bool loadingFinished;
        FileData() : thumbnail(NULL), loadingFinished(false) {}
    };
    typedef FileData* (*RequestFileDataCallback)(const DirEntry& entry, int maxDimSize);
    typedef void (*FreeFileDataCallback)(ImGuiID id);

    void SetFileDataCallback(RequestFileDataCallback loadCallB, FreeFileDataCallback unloadCallB);

    void OpenDialog(const char* str_id, ImGuiFDMode mode, const char* path, const char* filter = NULL, ImGuiFDDialogFlags flags = 0, size_t maxSelections = 1);
    void CloseDialog(const char* str_id);
    void CloseCurrentDialog();

    bool BeginDialog(const char* str_id);
    bool BeginDialog(ImGuiID id);
    void EndDialog();

    bool ActionDone(); // was Open/Cancel (=> anything) pressed?
    bool SelectionMade(); // was Open (and not Cancel) pressed?
    const char* GetResultStringRaw();
    size_t GetSelectionStringsAmt();
    const char* GetSelectionNameString(size_t ind);
    const char* GetSelectionPathString(size_t ind);

    // draw a debug window for the given dialog
    void DrawDebugWin(const char* dialog_str_id);

    void Shutdown();
}


#endif
