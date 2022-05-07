#ifndef __IMGUIFD_H__
#define __IMGUIFD_H__

#ifdef _MSC_VER
    #define _CRT_SECURE_NO_WARNINGS 1
#endif

#include "imgui.h"
#include <stdint.h>

namespace ImGuiFD {
    struct DirEntry {
    public:
        DirEntry();
        DirEntry(const DirEntry& src);
        DirEntry& operator=(const DirEntry& src);
        ~DirEntry();

        size_t id;
        const char* name = 0;
        const char* dir = 0;
        const char* path = 0;
        bool isFolder;

        uint64_t size = -1;
        time_t lastModified = -1;
        time_t creationDate = -1;
    };

    struct GlobalSettings {
        bool showDirFirst = true;
        bool adjustIconWidth = true;
        float iconModeSize = 100;

        enum {
            DisplayMode_List  = 0,
            DisplayMode_Icons = 1
        };
        uint8_t displayMode = DisplayMode_Icons;


        ImVec4 descTextCol = { .7f, .7f, .7f, 1 };
    };

    static GlobalSettings settings;

    enum {
        ImGuiFDDialogFlags_Modal = 1<<0
    };
    typedef int ImGuiFDDialogFlags;

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

    void OpenFileDialog(const char* str_id, const char* filter, const char* path, ImGuiFDDialogFlags flags = 0, size_t maxSelections = 1);
    void OpenDirDialog(const char* str_id, const char* path, ImGuiFDDialogFlags flags = 0);
    void CloseDialog(const char* str_id);
    void CloseCurrentDialog();

    bool BeginDialog(const char* str_id);
    void EndDialog();

    bool ActionDone(); // was Open/Cancel (=> anything) pressed?
    bool SelectionMade(); // was Open and not Cancel pressed?
    const char* GetSelectionStringRaw();
    size_t GetSelectionStringsAmt();
    const char* GetSelectionNameString(size_t ind);
    const char* GetSelectionPathString(size_t ind);

    void DrawDebugWin(const char* str_id);

    void Shutdown();
}


#endif