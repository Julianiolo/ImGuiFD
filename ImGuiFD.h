#ifndef __IMGUIFD_H__
#define __IMGUIFD_H__

#define IMGUIFD_VERSION "0.1 alpha"
#define IMGUIFD_VERSION_NUM 0

// uncomment this for stl support
//#define IMGUIFD_ENABLE_STL 1


#ifdef _MSC_VER
    #ifndef _CRT_SECURE_NO_WARNINGS
        #define _CRT_SECURE_NO_WARNINGS 1
    #endif
#endif


#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "imgui.h"
#include <stdint.h>
#include <ctime>

/*

Example Filters:
    NULL
    "*"
    "*.txt"
    "."
    "{*},{*.txt,*.text}"
    "{*},{Text Files:*.txt,*.text}"
*/

enum {
    ImGuiFDMode_LoadFile = 0,
    ImGuiFDMode_SaveFile,
    ImGuiFDMode_OpenDir,
};
typedef uint8_t ImGuiFDMode;

enum {
    ImGuiFDDialogFlags_Modal = 1<<0
};
typedef int ImGuiFDDialogFlags;

namespace ImGuiFD {
    struct DirEntry {
    public:
        DirEntry();
        DirEntry(const DirEntry& src);
        DirEntry& operator=(const DirEntry& src);
        ~DirEntry();

        ImGuiID id = (ImGuiID)-1;
        const char* name = 0;
        const char* dir = 0;
        const char* path = 0;
        bool isFolder = false;

        uint64_t size = (uint64_t)-1;
        time_t lastModified = -1;
        time_t creationDate = -1;
    };

    struct GlobalSettings {
        bool showDirFirst = true;
        bool adjustIconWidth = true;
        ImVec4 iconTextCol = { .08f, .08f, .78f, 1 };
        const float iconModeSizeDef = 100;
        float iconModeSize = iconModeSizeDef;

        enum {
            DisplayMode_List  = 0,
            DisplayMode_Icons = 1
        };
        uint8_t displayMode = DisplayMode_Icons;


        ImVec4 descTextCol = { .7f, .7f, .7f, 1 };

        bool asciiArtIcons = true;
    };

    static GlobalSettings settings;

    struct FileData {
        struct Image {
            ImTextureID texID;
            int width;
            int height;
            int origWidth = -1;
            int origHeight = -1;

            uint64_t memSize;
            void* userData = 0;

            bool dimDone = false;
            bool loadDone = false;
        }* thumbnail;


        bool loadingFinished = false;

        uint64_t getSize() const;
    };
    typedef FileData* (*RequestFileDataCallback)(const DirEntry& entry, int maxDimSize);
    typedef void (*FreeFileDataCallback)(ImGuiID id);

    void SetFileDataCallback(RequestFileDataCallback loadCallB, FreeFileDataCallback unloadCallB);

    void GetFileDialog(const char* str_id, const char* filter, const char* path, ImGuiFDDialogFlags flags = 0, size_t maxSelections = 1);

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

    void DrawDebugWin(const char* str_id);

    void Shutdown();
}


#endif