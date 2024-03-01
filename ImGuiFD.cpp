#include "ImGuiFD.h"
#include "ImGuiFD_internal.h"

#include <string.h>

#include <stdint.h>
#include <time.h> // used for localtime() and strftime()

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS 1
#endif
#include <inttypes.h> // used for u64 in format string

namespace ImGuiFD {
	namespace utils {
		const char* findCharInStrFromBack(char c, const char* str, const char* strEnd = nullptr) {
			if (strEnd == nullptr)
				strEnd = str + strlen(str);
			for (const char* ptr = strEnd-1; ptr >= str; ptr--) {
				if (*ptr == c)
					return ptr;
			}
			return nullptr;
		}
		const char* getFileName(const char* path, const char* path_end = 0) {
			if (path_end == 0)
				path_end = path + strlen(path);

			//while(path+1 <= path_end && (*(path_end-1) == '/' || *(path_end-1) == '\\'))
			//	path_end--;

			const char* lastSlash = findCharInStrFromBack('/', path, path_end);
			const char* lastBSlash = findCharInStrFromBack('\\', path, path_end);

			if (lastSlash == NULL && lastBSlash == NULL)
				return path;

			const char* lastDiv = ImMax(lastSlash != nullptr ? lastSlash : 0, lastBSlash != nullptr ? lastBSlash : 0);

			return lastDiv + 1;
		}
		ds::string fixDirStr(const char* path) {
			ds::string out;

			size_t len = 0;
			{
				size_t ind = 0;
				bool lastWasSlash = false;
				while (path[ind]) {
					char c = path[ind];
					switch (c) {
					case '/':
					case '\\':
					{
						if (!lastWasSlash) {
							lastWasSlash = true;
							len++;
						}
						break;
					}

					default:
						lastWasSlash = false;
						len++;
						break;
					}

					ind++;
				}

				out.resize(len);
			}

			size_t ind = 0;
			size_t indOut = 0;
			bool lastWasSlash = false;
			while (path[ind]) {
				char c = path[ind];
				switch (c) {
				case '\\':
					c = '/'; // fall through
				case '/':
				{
					if (!lastWasSlash) {
						lastWasSlash = true;
						out[indOut++] = c;
					}
					break;
				}
				default:
					lastWasSlash = false;
					out[indOut++] = c;
					break;
				}
				ind++;
			}
			out[indOut] = 0;

			if (out[out.size() - 1] != '/') {
				out += "/";
			}

			if (out[0] != '/') {
				out = ds::string("/") + out;
			}

			return out;
		}
		ds::vector<ds::string> splitPath(const char* path) {
			if (strlen(path) == 1 && path[0] == '/') {
				ds::vector<ds::string> out;
				out.push_back("/");
				return out;
			}
				

			size_t len = strlen(path);
			ds::vector<ds::string> out;
			size_t last = 0;

			if (path[0] == '/') {
				out.push_back("/");
				last = 1;
			}

			for (size_t i = last; i < len; i++) {
				if (path[i] == '/') {
					out.push_back(ds::string(path + last, path + i));
					last = i + 1;
				}
			}
			return out;
		}
		ds::vector<ds::pair<ds::string, ds::string>> splitInput(const char* str, const char* dir) {
			size_t len = strlen(str);
			ds::string dirStr = dir;

			ds::vector<ds::pair<ds::string, ds::string>> out;
			bool insideQuote = false;

			size_t i = 0;
			size_t last = 0;
			while (i < len) {
				char c = str[i];
				if (c == '"') {
					insideQuote = !insideQuote;

					if (insideQuote) {
						last = i+1;
					}
					else {
						ds::string path(str+last,str+i);
						if (path[0] != '/') { // check if path is relative
							path = dirStr + path;
						}
						path = Native::makePathStrOSComply(path.c_str());
						const char* filename = getFileName(path.c_str());
						out.push_back({ filename,path});
						last = i+1;
					}
				}
				i++;
			}
			if (last != i) {
				ds::string path(str+last,str+i);
				if (path[0] != '/') { // check if path is relative
					path = dirStr + path;
				}
				path = Native::makePathStrOSComply(path.c_str());
				out.push_back({ getFileName(path.c_str()),path});
			}

			return out;
		}

		int TextCallBack(ImGuiInputTextCallbackData* data) {
			ds::string* str = (ds::string*)data->UserData;

			if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
				str->resize(data->BufTextLen);
				data->Buf = (char*)str->c_str();
			}
			return 0;
		}
		bool InputTextString(const char* label, const char* hint, ds::string* str, ImGuiInputTextFlags flags = 0, const ImVec2& size = { 0,0 }) {
			return ImGui::InputTextEx(
				label, hint, (char*)str->c_str(), (int)str->capacity(), 
				size, flags | ImGuiInputTextFlags_CallbackResize, TextCallBack, str
			);
		}
	}

	ImGuiTableSortSpecs* globalSortSpecs = 0;
	ds::vector<DirEntry>* globalSortData = 0;

	enum {
		DEIG_NAME = 0,
		DEIG_SIZE = 1,
		DEIG_LASTMOD_DATE,
		DEIG_CREATION_DATE,
	};

	static int customStrCmp(const char* strA, const char* strB) {
		while (true) {
			char a = *strA++;
			char b = *strB++;

			if (a == 0 || b == 0) {
				if (a == 0 && b == 0)
					return 0;
				if (a == 0)
					return -1;
				else
					return 1;
			}

			if (a >= 'A' && a <= 'Z')
				a += 'a' - 'A';

			if (b >= 'A' && b <= 'Z')
				b += 'a' - 'A';

			if (a == b)
				continue;

			return a - b;
		}
	}

	static int compareSortSpecs(const void* lhs, const void* rhs){
		auto& a = (*globalSortData)[*(size_t*)lhs];
		auto& b = (*globalSortData)[*(size_t*)rhs];

		if (settings.showDirFirst) {
			if (a.isFolder && !b.isFolder) return -1;
			if (!a.isFolder && b.isFolder) return 1;
		}

		for (int i = 0; i < globalSortSpecs->SpecsCount; i++) {
			auto& specs = globalSortSpecs->Specs[i];
			int delta = 0;
			switch (specs.ColumnUserID) {
				case DEIG_NAME:
					delta = customStrCmp(a.name, b.name);
					break;
				case DEIG_SIZE: {
					int64_t diff = ((int64_t)a.size - (int64_t)b.size); // int64 to prevent overflows
					if (diff < 0) delta = -1;
					if (diff > 0) delta = 1;
					break;
				}
				case DEIG_CREATION_DATE: {
					int64_t diff = ((int64_t)a.creationDate - (int64_t)b.creationDate); // int64 to prevent overflows
					if (diff < 0) delta = -1;
					if (diff > 0) delta = 1;
					break;
				}
				case DEIG_LASTMOD_DATE: {
					int64_t diff = ((int64_t)a.lastModified - (int64_t)b.lastModified); // int64 to prevent overflows
					if (diff < 0) delta = -1;
					if (diff > 0) delta = 1;
					break;
				}
			}

			if (delta > 0)
				return (specs.SortDirection == ImGuiSortDirection_Ascending) ? +1 : -1;
			if (delta < 0)
				return (specs.SortDirection == ImGuiSortDirection_Ascending) ? -1 : +1;
		}

		return (int)a.id-(int)b.id;
	}

	class EditablePath {
	public:
		ds::vector<ds::string> parts;

		EditablePath(const char* path) : parts(utils::splitPath(path)) {
			
		}

		void setToStr(const char* rawPath) {
			ds::string rawPathFix = utils::fixDirStr(rawPath).c_str(); // fixes weird behaviour on win when e.g. setting to "D:"
			parts = utils::splitPath(
				utils::fixDirStr(
					Native::getAbsolutePath(rawPathFix.c_str()).c_str()
				).c_str()
			);
		}

		bool setBackToInd(size_t ind) {
			if (ind + 1 != parts.size()) {
				parts.resize(ind+1);
				return true;
			}
			return false;
		}

		bool goUp() {
			if (parts.size() > 1) {
				parts.resize(parts.size() - 1);
				return true;
			}
			return false;
		}

		void moveDownTo(const char* folder) {
			parts.push_back(folder);
		}

		ds::string toString() {
			size_t len = 0;
			for (size_t i = 0; i < parts.size(); i++) {
				if (i > 0)
					len++; // +1 len for '/'
				len += parts[i].size();
			}
			ds::string out;
			out.reserve(len);
			if(parts.size() == 1 && parts[0] == "/") {
				out = "/";
			}
			else {
				for (size_t i = 0; i < parts.size(); i++) {
					out += parts[i];
					out += "/";
				}
			}
			
			return utils::fixDirStr(out.c_str());
		}
	};

	class FileNameFilter {
	private:
		class Filter {
			class SubFilter {
				ds::string name;

				ds::string include;
				ds::string exclude;

				ds::string fileName;
				ds::string fileExt;

				ds::string exact;

			public:

				SubFilter(ds::string cmd) {
					{
						const char* colPos = strchr(cmd.c_str(), ':');
						if (colPos != nullptr) {
							name = cmd.substr(0, colPos - cmd.c_str());
							cmd = cmd.substr(colPos - cmd.c_str() + 1, cmd.size());
						}
					}

					cmd = ds::replace(cmd.c_str(), " ", "");

					if (cmd[0] == '=') {
						exact = cmd.substr(1, cmd.size());
					}
					else if (cmd[0] == '!') {
						exclude = cmd.substr(1, cmd.size());
					}
					else {
						const char* dotPos = strrchr(cmd.c_str(), '.');
						if (dotPos != NULL) {
							fileName = cmd.substr(0, dotPos - cmd.c_str());
							fileExt = cmd.substr(dotPos - cmd.c_str() + 1, cmd.size());

							if (fileName == "*")
								fileName = "";
							if (fileExt == "*")
								fileExt = "";
						}
						else {
							include = cmd;
						}
					}
				}

				bool passes(const char* str) {
					if (exact.size() > 0 && exact != str)
						return false;

					if (include.size() > 0 && strstr(str, include.c_str()) == NULL)
						return false;

					if (exclude.size() > 0 && strstr(str, exclude.c_str()) != NULL)
						return false;

					{
						const char* dotPos = strrchr(str, '.');
						if (dotPos == NULL) {
							if (fileExt.size() > 0)
								return false;
							if (fileName.size() > 0 && fileName != str)
								return false;
						}
						else {
							if (fileExt.size() > 0 && fileExt != (dotPos+1))
								return false;

							if (fileName.size() > 0 && strncmp(fileName.c_str(), str, fileName.size() < (size_t)(dotPos - str) ? fileName.size() : (dotPos - str)) != 0)
								return false;
						}

					}

					return true;
				}
			};

			ds::vector<SubFilter> filters;
		public:
			ds::string rawStr;
			
			Filter(const ds::string& cmd) : rawStr(cmd) {
				size_t last = 0;
				for (size_t i = 0; i < cmd.size(); i++) {
					if (cmd[i] == ',' && i>last) {
						filters.push_back(SubFilter(cmd.substr(last, i)));
						last = i + 1;
					}
				}
				if (last != cmd.size()) {
					filters.push_back(SubFilter(cmd.substr(last, cmd.size())));
				}
			}

			bool passes(const char* name) {
				if (filters.size() == 0)
					return true;

				for (size_t i = 0; i < filters.size(); i++) {
					if (filters[i].passes(name))
						return true;
				}
				return false;
			}
		};

	public:
		ds::string searchText;
		size_t filterSel = 0; // currently selected filter
		ds::vector<Filter> filters;

		FileNameFilter() {

		}
		FileNameFilter(const char* filter) {
			size_t len = filter ? strlen(filter) : 0;

			if (len == 0)
				return;

			if (filter[0] != '{') {
				filters.push_back(Filter(filter));
			}
			else {
				size_t bracketCntr = 0;

				size_t last = 0;
				for (size_t i = 0; i < len; i++) {
					if (filter[i] == '{') bracketCntr++;
					if (filter[i] == '{') bracketCntr--;
					if (filter[i] == ',' && bracketCntr == 0 && i>last) {
						filters.push_back(ds::string(filter+last,filter+i));
						last = i + 1;
					}
				}
				if (len-last > 0) {
					filters.push_back(ds::string(filter+last,filter+len));
				}
			}

			if (filters.size() > 0)
				filters.push_back(Filter("*.*"));
		}

		bool passes(const char* name, bool isFolder) {
			if (searchText.size() > 0) {
				Filter searchFilter(searchText);
				if (!searchFilter.passes(name))
					return false;
			}

			if (isFolder)
				return true;
			
			if (filters.size() == 0)
				return true;

			if (!filters[filterSel].passes(name))
				return false;

			return true;
		}

		bool draw(float width = -1) {
			ImGui::PushItemWidth(width);
			bool ret = utils::InputTextString("##Search", "Search", &searchText);
			ImGui::PopItemWidth();
			return ret;
		}
	};

	class FileDataCache {
	private:
		ds::set<ImGuiID> loaded;
	public:
		static RequestFileDataCallback requestFileDataCallB;
		static FreeFileDataCallback freeFileDataCallB;

		~FileDataCache() {
			clear();
		}

		FileData* get(const DirEntry& entry) {
			if(!requestFileDataCallB)
				return 0;

			if(!loaded.contains(entry.id))
				loaded.add(entry.id);

			return requestFileDataCallB(entry, 300);
		}

		void clear() {
			if(!freeFileDataCallB)
				return;

			for (auto& l : loaded) {
				freeFileDataCallB(l);
			}
			loaded.clear();
		}

		uint64_t maxSize() const {
			return 0;
		}
		uint64_t size() const {
			return 0;
		}
	};
	
	class FileDialog {
	public:
		ds::string str_id;
		ImGuiID id;
		
		ds::string path;
		EditablePath currentPath;
		ds::string oldPath;
		ds::string couldntLoadPath;

		bool forceDisplayAllDirs = false;

		bool isEditingPath = false;
		ds::string editOnPathStr;
		ds::OverrideStack<ds::string> undoStack;
		ds::OverrideStack<ds::string> redoStack;
		
		class EntryManager {
			ds::vector<DirEntry> data;
			ds::vector<size_t> dataModed;

			bool loadedSucessfully = false;
		public:
			bool sorted = false;
			FileNameFilter filter;

			EntryManager(const char* filter) : filter(filter) {
				
			}

			// return true if it was able to load the directory
			bool update(const char* dir) {
				auto res = Native::loadDirEnts(dir, &loadedSucessfully);
				if(loadedSucessfully)
					setEntrysTo(res);
				return loadedSucessfully;
			}

			void setEntrysTo(const ds::vector<DirEntry>& src) {
				data = src;
				updateFiltering();
			}

			DirEntry& getRaw(size_t i) {
				return data[i];
			}

			DirEntry& get(size_t i) {
				return data[dataModed[i]];
			}
			size_t getInd(size_t i) {
				return dataModed[i];
			}

			size_t size() {
				return dataModed.size();
			}

			size_t getActualIndex(size_t i) {
				return dataModed[i];
			}

			void updateFiltering() {
				dataModed.clear();
				for (size_t i = 0; i < data.size(); i++) {
					if (filter.passes(data[i].name, data[i].isFolder)) {
						dataModed.push_back(i);
					}
				}
				sorted = false;
			}

			void sort(ImGuiTableSortSpecs* sorts_specs) {
				globalSortSpecs = sorts_specs;
				globalSortData = &data;

				if(dataModed.size() > 1)
					qsort(&dataModed[0], dataModed.size(), sizeof(dataModed[0]), compareSortSpecs);
				
				globalSortSpecs = 0;
				globalSortData = 0;

				sorted = true;
			}

			void drawSeachBar(float width = -1) {
				if (filter.draw(width))
					updateFiltering();
			}

			bool wasLoadedSuccesfully() const {
				return loadedSucessfully;
			}
		};
		EntryManager entrys;
		FileDataCache fileDataCache;

		size_t lastSelected = (size_t)-1;
		ds::set<size_t> selected;

		ImGuiFDMode mode;
		bool isModal = false;
		bool hasFilter;
		
		size_t maxSelections;
		
		ds::string inputText = "";
		ds::string newFolderNameStr = "";
		ds::string renameStr = "";
		size_t renameId = (size_t)-1;

		bool needsEntrysUpdate = false;

		bool actionDone = false;
		bool selectionMade = false;
		bool toDelete = false;
		bool showLoadErrorMsg = false;

		ds::vector<ds::pair<ds::string,ds::string>> inputStrs;

		FileDialog(ImGuiID id, const char* str_id, const char* filter, const char* path, ImGuiFDMode mode, ImGuiFDDialogFlags flags = 0, size_t maxSelections = 1) : 
			str_id(str_id), id(id), path(utils::fixDirStr(Native::getAbsolutePath(path).c_str())), 
			currentPath(this->path.c_str()), oldPath(this->path),
			undoStack(32), redoStack(32),
			entrys(filter),
			mode(mode), isModal((flags&ImGuiFDDialogFlags_Modal)!=0), hasFilter(filter != NULL), maxSelections(maxSelections)
		{
			updateEntrys();
			setInputTextToSelected();
		};

		void dirSetTo(const char* str) {
			undoStack.push(currentPath.toString());
			redoStack.clear();

			currentPath.setToStr(str);

			needsEntrysUpdate = true;
		}
		void dirGoUp() {
			undoStack.push(currentPath.toString());
			redoStack.clear();

			if (currentPath.goUp())
				needsEntrysUpdate = true;
		}
		void dirShrinkTo(size_t ind) {
			undoStack.push(currentPath.toString());
			redoStack.clear();

			if (currentPath.setBackToInd(ind))
				needsEntrysUpdate = true;
		}
		void dirMoveDownInto(const char* folder) {
			undoStack.push(currentPath.toString());
			redoStack.clear();

			currentPath.moveDownTo(folder);
			needsEntrysUpdate = true;
		}

		void setInputTextToSelected() {
			if (selected.size() == 0) {
				if (mode == ImGuiFDMode_OpenDir) {
					inputText = ds::string("\"") + currentPath.toString() + "\""; // if selecting a directory, always have the folder we are in as default if nothing is selected
				}
				else {
					inputText = "";
				}
			}
			else {
				inputText = "\"";
				for (size_t i = 0; i < selected.size(); i++) {
					if(i>0)
						inputText += "\", \"";
					inputText += entrys.getRaw(selected[i]).name;
				}
				inputText += "\"";
			}
		}
		void beginEditOnStr() {
			isEditingPath = true;
			editOnPathStr = currentPath.toString();
		}
		void endEditOnStr() {
			isEditingPath = false;

		}
		
		void update() {
			if (needsEntrysUpdate) {
				needsEntrysUpdate = false;
				updateEntrys();
				setInputTextToSelected();
			}
		}
		void updateEntrys() {
			ds::string curDirStr = currentPath.toString();
			if (entrys.update(curDirStr.c_str())) {
				inputText = "";

				fileDataCache.clear();

				lastSelected = (size_t)-1;
				selected.clear();
				oldPath = curDirStr;
			}
			else {
				showLoadErrorMsg = true;
				couldntLoadPath = curDirStr;
				currentPath.setToStr(oldPath.c_str());
			}
		}
		void updateFiltering() {
			entrys.updateFiltering();
		}

		bool canUndo() const {
			return !undoStack.isEmpty();
		}
		void undo() {
			ds::string s;
			if (undoStack.pop(&s)) {
				redoStack.push(currentPath.toString());
				currentPath.setToStr(s.c_str());

				needsEntrysUpdate = true;
			}
		}
		bool canRedo() const {
			return !redoStack.isEmpty();
		}
		void redo() {
			ds::string s;
			if (redoStack.pop(&s)) {
				undoStack.push(currentPath.toString());
				currentPath.setToStr(s.c_str());

				needsEntrysUpdate = true;
			}
		}

		bool isFileMode() const {
			return mode == ImGuiFDMode_LoadFile || mode == ImGuiFDMode_SaveFile;
		}
		bool isDirMode() const {
			return mode == ImGuiFDMode_OpenDir;
		}

		DirEntry& getSelectedInd(size_t ind) {
			IM_ASSERT(ind <= selected.size());
			return entrys.getRaw(*(selected.begin() + ind));
		}
		void resetRename() {
			renameId = (size_t)-1;
			renameStr = "";
		}
	};

	FileDialog* fd = 0;
	ds::map<FileDialog> openDialogs;

	static ImRect TextWrappedCentered(const char* text, float maxWidth, int maxLines = -1) {
		ImGuiContext& g = *GImGui;
		ImVec2 cursorStart = ImGui::GetCursorPos();

		size_t len = strlen(text);

		const char* s = text;
		int lineInd = 0;

		ImVec2 minPos(10000,10000), maxPos(-10000,-10000);

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0,0 });
		while (s < text + len && lineInd < maxLines) {
			const char* wrap = g.Font->CalcWordWrapPositionA(g.FontSize/g.Font->FontSize, s, text+len, maxWidth);
			
			ImVec2 lineSize;
			if (wrap == s || lineInd+1 == maxLines) {
				lineSize = g.Font->CalcTextSizeA(g.FontSize, maxWidth, 0, s, text + len, &wrap);
			}
			else {
				lineSize = ImGui::CalcTextSize(s, wrap);
			}

			// Wrapping skips upcoming blanks
			while (s < text+len)
			{
				const char c = *s;
				if (ImCharIsBlankA(c)) { s++; } else if (c == '\n') { s++; break; } else { break; }
			}

			{
				const ImVec2 cursorPos = cursorStart + ImVec2{ maxWidth / 2 - lineSize.x / 2, ImGui::GetTextLineHeightWithSpacing() * lineInd };
				if (cursorPos.x < minPos.x) minPos.x = cursorPos.x;
				if (cursorPos.y < minPos.y) minPos.y = cursorPos.y;

				if ((cursorPos+lineSize).x > maxPos.x) maxPos.x = (cursorPos+lineSize).x;
				if ((cursorPos+lineSize).y > maxPos.y) maxPos.y = (cursorPos+lineSize).y;


				ImGui::SetCursorPos(cursorPos);
				if (lineInd + 1 >= maxLines && wrap < text+len) {
					ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(), ImGui::GetCursorScreenPos(), ImGui::GetCursorScreenPos() + lineSize + ImVec2{0,g.Style.FramePadding.y}, ImGui::GetCursorScreenPos().x + maxWidth, ImGui::GetCursorScreenPos().x + maxWidth, s, text + len, 0);
				}
				else {
					ImGui::TextUnformatted(s, wrap);
				}
			}
			

			s = wrap;
			lineInd++;
		}
		ImGui::PopStyleVar();

		// convert to screen space
		minPos += ImGui::GetWindowPos() - ImVec2{ImGui::GetScrollX(), ImGui::GetScrollY()};
		maxPos += ImGui::GetWindowPos() - ImVec2{ImGui::GetScrollX(), ImGui::GetScrollY()};

		return ImRect(minPos, maxPos);
	}
	static bool ComboVertical(const char* str_id, size_t* v, const char** labels, size_t labelCnt, const ImVec2& size_arg = { 0,0 }) {
		ImGui::PushID(str_id);

		ImVec2 avail = ImGui::GetContentRegionAvail();
		ImVec2 size = ImVec2(size_arg.x != 0 ? size_arg.x : avail.x, size_arg.y != 0 ? size_arg.y : ImGui::GetFrameHeight());

		ImVec2 borderPad = { 1,1 };
		ImRect rec(ImGui::GetCursorScreenPos() - borderPad, ImGui::GetCursorScreenPos() + size + borderPad);
		ImGui::GetWindowDrawList()->AddRectFilled(rec.Min, rec.Max, ImColor(ImGui::GetStyleColorVec4(ImGuiCol_ChildBg)));

		//ImGui::A();
		ImGui::GetCurrentWindow()->DC.CurrLineTextBaseOffset = 0;

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0,0 });
		ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, {0.5,0.5});
		bool changed = false;
		for (size_t i = 0; i < labelCnt; i++) {
			if (i > 0)
				ImGui::SameLine();
			if (ImGui::Selectable(labels[i], *v == i, ImGuiSelectableFlags_NoPadWithHalfSpacing, ImVec2(size.x / labelCnt, size.y))) {
				*v = i;
				changed = true;
			}
		}
		ImGui::PopStyleVar();
		ImGui::PopStyleVar();

		ImGui::GetWindowDrawList()->AddRect(rec.Min, rec.Max, ImColor(ImGui::GetStyleColorVec4(ImGuiCol_Border)));

		ImGui::PopID();
		return changed;
	}

	static void formatTime(char* buf, size_t bufSize, time_t unixTime) {
		tm* tm = localtime(&unixTime);
		strftime(buf, bufSize, "%d.%m.%y %H:%M", tm);
	}
	static void formatSize(char* buf, size_t bufSize, uint64_t size) {
		snprintf(buf, bufSize, "%" PRIu64 " Bytes", size);
	}

	static void OpenNow() {
		fd->inputStrs = utils::splitInput(fd->inputText.c_str(), fd->currentPath.toString().c_str());

		fd->actionDone = true;
		fd->selectionMade = true;
	}

	static void ClickedOnEntrySelect(size_t id, bool isSel, bool isFolder) {
		if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			return;
		if (fd->mode == ImGuiFDMode_OpenDir && !isFolder)
			return;

		if (ImGui::GetIO().KeyShift == ImGui::GetIO().KeyCtrl || ImGui::IsMouseClicked(ImGuiMouseButton_Right)) { // both on or both off => standard select
			if (isSel) {
				if (fd->selected.size() != 1) {
					fd->selected.clear();
					fd->selected.add(id);
				}
			}
			else {
				if (fd->selected.size() > 0)
					fd->selected.clear();
				fd->selected.add(id);
			}
		}
		else if (ImGui::GetIO().KeyShift) {
			size_t a = fd->lastSelected;
			if (a == (size_t)-1)
				a = id;
			size_t b = id;
			size_t from, to;
			if (a<b) {
				from = a;
				to = b;
			}
			else {
				from = a;
				to = b;
			}

			for (size_t i = from; i <= to; i++)
				fd->selected.add(i);
		}
		else if (ImGui::GetIO().KeyCtrl) {
			if (!isSel) {
				fd->selected.add(id);

				// if too many selected: delete the earlier selected ones
				if (fd->selected.size() > fd->maxSelections)
					fd->selected.erase(fd->selected.begin());
			}
			else {
				fd->selected.eraseItem(id);
			}
		}

		fd->setInputTextToSelected();
		fd->resetRename();
	}
	static void CheckDoubleClick(const DirEntry& entry) {
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
			if (entry.isFolder) {
				fd->dirMoveDownInto(entry.name);
			}
			else {
				if (fd->mode == ImGuiFDMode_LoadFile) {
					OpenNow();
				}
			}
			fd->resetRename();
		}
	}
	
	static void DrawSettings() {
		ImGui::Checkbox("Show dir first", &settings.showDirFirst);
		ImGui::Checkbox("Adjust icon width", &settings.adjustIconWidth);

		ImGui::Separator();

		ImGui::DragFloat("Icon scale", &settings.iconModeSize, 1, 10, 300);
		if (settings.iconModeSize != settings.iconModeSizeDef) {
			ImGui::SameLine();
			if (ImGui::Button("Reset")) {
				settings.iconModeSize = settings.iconModeSizeDef;
			}
		}


		ImGui::ColorEdit4("Icon text color", (float*)&settings.iconTextCol);
		ImGui::Checkbox("Ascii art icons", &settings.asciiArtIcons);
	}


	static void DrawContorls() {
		bool canUndo = fd->canUndo();
		if(!canUndo) ImGui::BeginDisabled();
		if (ImGui::Button("<")) {
			fd->undo();
		}
		if(!canUndo) ImGui::EndDisabled();

		
		ImGui::SameLine();

		bool canRedo = fd->canRedo();
		if(!canRedo) ImGui::BeginDisabled();
		if (ImGui::Button(">")) {
			fd->redo();
		}
		if(!canRedo) ImGui::EndDisabled();


		ImGui::SameLine();
		if (ImGui::Button("^")) {
			fd->dirGoUp();
		}
	}
	static void DrawDirBar() {
		const float width = ImGui::GetContentRegionAvail().x;
		if (width <= 0) {
			ImGui::Dummy(ImVec2(0, 0)); // dummy to reset SameLine();
			return;
		}

		const ImVec2 borderPad(2, 2);
		const ImRect rec(ImGui::GetCursorScreenPos(), ImGui::GetCursorScreenPos() + ImVec2(width, ImGui::GetFrameHeight()));
		const ImRect recOuter(rec.Min - borderPad, rec.Max + borderPad);
		ImGuiStyle& style = ImGui::GetStyle();

		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 1);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0,0 });

		ImGui::BeginChild("DirBar", ImVec2(width, ImGui::GetFrameHeight()));

		ImVec2 cursorStartPos = ImGui::GetCursorPos();

		if (!fd->isEditingPath) {
			bool enterEditMode = false;

			ImGui::BeginGroup();

			// draw Background
			ImGui::GetWindowDrawList()->AddRectFilled(recOuter.Min, recOuter.Max, ImColor(ImGui::GetStyleColorVec4(ImGuiCol_TableHeaderBg)));

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 2,0 });

			// calculate total width of all dir buttons
			float totalWidth = 0;
			size_t lastToFit = (size_t)-1; // including the ... button
			
			if (!fd->forceDisplayAllDirs) {
				for (ptrdiff_t i = fd->currentPath.parts.size()-1; i >= 0; i--) {
					const float label_size = ImGui::CalcTextSize(fd->currentPath.parts[i].c_str(), NULL, true).x;
					float nextWidth = totalWidth + (label_size + style.FramePadding.x * 2.0f) + style.ItemSpacing.x;

					if (totalWidth <= width && nextWidth > width) {
						lastToFit = i;
						totalWidth = nextWidth;
						break;
					}

					totalWidth = nextWidth;
				}
			}

			//ImGui::GetWindowDrawList()->AddRect(ImGui::GetCursorScreenPos(), ImGui::GetCursorScreenPos() + ImVec2(totalWidth, ImGui::GetTextLineHeight()), IM_COL32(255, 0, 0, 255));

			bool doesntFit = lastToFit != (size_t)-1;

			
			size_t startOn = 0;
			const float ellipseBtnWidth = ImGui::CalcTextSize("...").x + style.FramePadding.x * 2 + style.ItemSpacing.x;
			if (doesntFit) {
				startOn = lastToFit;

				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + width - totalWidth);
			}

			// draw buttons
			for (size_t i = startOn; i < fd->currentPath.parts.size(); i++) {
				ImGui::PushID((ImGuiID)i);
				if (i > startOn)
					ImGui::SameLine();
				if (ImGui::Button(fd->currentPath.parts[i].c_str())) {
					fd->dirShrinkTo(i); // navigate to clicked dir
				}
				ImGui::PopID();
			}
			ImGui::PopStyleVar();

			
			if (doesntFit) {
				// draw ellipse last so it overlapps
				ImGui::SameLine();
				ImGui::SetCursorPos(cursorStartPos);
				ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetCursorScreenPos(), ImGui::GetCursorScreenPos() + ImVec2(ellipseBtnWidth, ImGui::GetFrameHeight()), ImColor(ImGui::GetStyleColorVec4(ImGuiCol_TableHeaderBg)));
				if (ImGui::Button("...")) { // ellipse Button
					fd->forceDisplayAllDirs = true;
				}
			}


			ImGui::EndGroup();

			if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
				enterEditMode = true;

			// add dummy to register mousepress on background
			ImGui::SameLine();
			ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight()));
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right))
				enterEditMode = true;

			if (enterEditMode) {
				fd->beginEditOnStr();
				ImGui::SetActiveID(ImGui::GetID("##editPath"), ImGui::GetCurrentWindow());
			}
		}
		else {
			ImGui::PushItemWidth(-FLT_MIN);

			bool endEdit = false;
			fd->forceDisplayAllDirs = false;

			if (utils::InputTextString("##editPath", "Enter a Directory", &fd->editOnPathStr, ImGuiInputTextFlags_EnterReturnsTrue)) {
				endEdit = true;
				if (Native::isValidDir(utils::fixDirStr(fd->editOnPathStr.c_str()).c_str())) {
					fd->dirSetTo(fd->editOnPathStr.c_str());
				}
				else {
					printf("Not a valid dir: %s\n", fd->editOnPathStr.c_str());
				}
			}

			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsItemHovered())
				endEdit = true;

			if (endEdit)
				fd->endEditOnStr();

			ImGui::PopItemWidth();
		}

		

		ImGui::EndChild();

		ImGui::PopStyleVar();
		ImGui::PopStyleVar();

		// draw border
		ImGui::GetWindowDrawList()->AddRect(recOuter.Min, recOuter.Max, ImColor(ImGui::GetStyleColorVec4(ImGuiCol_Border)));
	}
	static void DrawNavigation() {
		ImGuiStyle& style = ImGui::GetStyle();

		DrawContorls();
		ImGui::SameLine();
		DrawDirBar();
		

		float displayModeBtnWidth = 50;
		float settingsBtnWidth = ImGui::CalcTextSize("S").x + style.FramePadding.x * 2;
		fd->entrys.drawSeachBar(ImGui::GetContentRegionAvail().x - (displayModeBtnWidth+style.ItemSpacing.x) - (settingsBtnWidth+style.ItemSpacing.x));
		ImGui::SameLine();
		const char* labels[] = { "T","I" };
		size_t mode = settings.displayMode;
		if (ComboVertical("DisplayModeBtn", &mode, labels, 2, { displayModeBtnWidth, 0 })) {
			settings.displayMode = (uint8_t)mode;
		}

		ImGui::SameLine();

		if (ImGui::Button("S")) {
			ImGui::OpenPopup((fd->str_id + "SettingsPopup").c_str());
		}


		if (ImGui::BeginPopup((fd->str_id + "SettingsPopup").c_str())) {
			DrawSettings();
			ImGui::EndPopup();
		}
	}

	static void DrawDirFiles_TableRow(size_t row) {
		auto& entry = fd->entrys.get(row);
		size_t ind = fd->entrys.getActualIndex(row);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		bool isSel = fd->selected.contains(ind);
		ImGuiSelectableFlags flags = ImGuiSelectableFlags_AllowItemOverlap | ImGuiSelectableFlags_SpanAllColumns;

		if (ImGui::Selectable(entry.isFolder?"[DIR]":"[FILE]", isSel, flags)) {
			ClickedOnEntrySelect(ind, isSel, entry.isFolder);
		}

		CheckDoubleClick(entry);

		ImGui::TableNextColumn();
		ImGui::TextUnformatted(entry.name);

		ImGui::TableNextColumn();
		if (entry.size != (uint64_t)-1) {
			char buf[128];
			formatSize(buf, sizeof(buf), entry.size);
			ImGui::TextUnformatted(buf);
		}
		else
			ImGui::Dummy({ 0,0 });

		ImGui::TableNextColumn();
		if (entry.lastModified != (time_t)-1) {
			char buf[128];
			formatTime(buf, sizeof(buf), entry.lastModified);
			ImGui::TextUnformatted(buf);
		}
		else
			ImGui::Dummy({ 0,0 });

		ImGui::TableNextColumn();
		if (entry.creationDate != (time_t)-1) {
			char buf[128];
			formatTime(buf, sizeof(buf), entry.creationDate);
			ImGui::TextUnformatted(buf);
		}
		else
			ImGui::Dummy({ 0,0 });
	}
	static void DrawDirFiles_Table(float height) {
		if (height <= 0)
			return;

		ImGuiTableFlags flags =
			//	sizeing:
			ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_SizingFixedFit |
			ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter |
			// style:
			ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_RowBg |
			// layout:
			ImGuiTableFlags_Reorderable | ImGuiTableFlags_Resizable |
			// sorting:
			ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti;

		//const ImVec2 tableScreenPos = ImGui::GetCursorScreenPos();

		if (ImGui::BeginTable("Dir", 5, flags, {0,height})) {
			ImGui::TableSetupScrollFreeze(0, 1); // make Header always visible
			ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_NoSort);
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort, 0, DEIG_NAME);
			ImGui::TableSetupColumn("Size", 0, 0, DEIG_SIZE);
			ImGui::TableSetupColumn("Creation Date", 0, 0, DEIG_CREATION_DATE);
			ImGui::TableSetupColumn("Last Modified", 0, 0, DEIG_LASTMOD_DATE);
			ImGui::TableHeadersRow();

			if (ImGuiTableSortSpecs* sorts_specs = ImGui::TableGetSortSpecs())
				if (sorts_specs->SpecsDirty || !fd->entrys.sorted){
					fd->entrys.sort(sorts_specs);
					sorts_specs->SpecsDirty = false;
				}

			ImGuiListClipper clipper;
			clipper.Begin((int)fd->entrys.size());
			while (clipper.Step()) {
				for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
					ImGui::PushID(row);

					DrawDirFiles_TableRow(row);

					ImGui::PopID();
				}
			}
			ImGui::EndTable();
		}

		/*ImGui::GetWindowDrawList()->AddRect(
			tableScreenPos, { tableScreenPos.x + ImGui::GetContentRegionAvail().x, tableScreenPos.y + height }, 
			ImColor(ImGui::GetStyleColorVec4(ImGuiCol_Border))
		);*/
	}

	static void DrawDirFiles_IconsItemDesc(const DirEntry& entry) {
		ImGui::PushStyleColor(ImGuiCol_Text, settings.descTextCol);

		if (ImGui::BeginTable("ToolTipDescTable", 2)) {
			
			if (entry.size != (uint64_t)-1) {
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Size:");

				ImGui::TableNextColumn();
				char buf[128];
				formatSize(buf, sizeof(buf), entry.size);
				ImGui::TextUnformatted(buf);
			}

			if (entry.creationDate != (time_t)-1) {
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Created:");

				ImGui::TableNextColumn();
				char buf[128];
				formatTime(buf, sizeof(buf), entry.creationDate);
				ImGui::TextUnformatted(buf);
			}

			if (entry.lastModified != (time_t)-1) {
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Last Modified:");

				ImGui::TableNextColumn();
				char buf[128];
				formatTime(buf, sizeof(buf), entry.lastModified);
				ImGui::TextUnformatted(buf);
			}

			ImGui::EndTable();
		}

		

		ImGui::PopStyleColor();
	}
	static void DrawDirFiles_Icons(float height) {
		if (height <= 0)
			return;

		// calculating item sizes
		const size_t numOfItems = fd->entrys.size();

		const float itemWidthRaw = settings.iconModeSize;
		const float itemHeight = itemWidthRaw * (3.0f / 4.0f);
		const ImVec2 padding(5,5);

		const float width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ScrollbarSize;

		size_t itemsPerLineRaw = (size_t)(width / (itemWidthRaw+padding.x*2));
		if (itemsPerLineRaw == 0) itemsPerLineRaw = 1;

		size_t itemsPerLine = itemsPerLineRaw;
		if (itemsPerLine > numOfItems && numOfItems > 0) itemsPerLine = numOfItems;

		
		float itemWidth = itemWidthRaw;
		if (settings.adjustIconWidth) {
			float widthLeft = width - ((itemWidthRaw+padding.x*2) * itemsPerLine);
			float itemWidthExtra = ImMin(itemWidthRaw * 0.5f, widthLeft / itemsPerLine);
			itemWidth += itemWidthExtra;
		}
		
		size_t numOfLines = numOfItems / itemsPerLine;
		if (numOfItems % itemsPerLineRaw != 0 && numOfLines > 0)
			numOfLines++;

		if (ImGui::BeginChild("IconTable", { 0,height }, true)) {
			ImGui::SetCursorPos(ImGui::GetCursorPos() + padding);
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, padding);
			ImGuiStyle& style = ImGui::GetStyle();
			
			if (numOfItems == 0) {
				const char* msg = "Directory is Empty!";
				ImVec2 msgSize = ImGui::CalcTextSize(msg);
				ImVec2 crsr = ImGui::GetCursorPos();
				ImGui::SetCursorPos(
					ImVec2 { ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x / 2 - msgSize.x / 2,
					ImGui::GetCursorPosY() + height / 10 - msgSize.y / 2 }
				);
				ImGui::TextUnformatted(msg);
				ImGui::SetCursorPos(crsr);
			}

			const ImVec2 totalCursorStart = ImGui::GetCursorPos();

			ImGuiListClipper clipper;
			clipper.Begin((int)numOfLines, itemHeight + style.ItemSpacing.y*2);
			while (clipper.Step()) {
				for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
					size_t itemsInThisLine = itemsPerLine;
					if ((size_t)row == numOfLines - 1)
						itemsInThisLine = numOfItems - itemsPerLine*(numOfLines-1);

					for (size_t col = 0; col < itemsInThisLine; col++) {
						const size_t ind = row * itemsPerLineRaw + col;

						const size_t id = fd->entrys.getInd(ind);

						ImGui::PushID((ImGuiID)ind);

						auto& entry = fd->entrys.getRaw(id);
						const bool isSel = fd->selected.contains(id);

						ImVec2 cursorStart = totalCursorStart + ImVec2{(itemWidth+style.ItemSpacing.x*2)*col, (itemHeight+style.ItemSpacing.y*2)*row};
						ImVec2 cursorEnd = cursorStart + ImVec2{itemWidth, itemHeight};
						ImGui::SetCursorPos(cursorStart);
						ImGui::Selectable("", isSel, 0, { itemWidth, itemHeight });
						if (ImGui::IsItemClicked() || ImGui::IsItemClicked(ImGuiMouseButton_Right)) { // directly using return value of Selectable doesnt work when going into folder (instantly selects hovered item) => ImGui bug?
							ClickedOnEntrySelect(id, isSel, entry.isFolder);
						}

						CheckDoubleClick(entry);

						int maxTextLines = 2;
						float textY = cursorEnd.y - ImGui::GetTextLineHeight() * maxTextLines;

						FileData* fileData = fd->fileDataCache.get(entry);
						const bool isImage = fileData && fileData->thumbnail;
						if (isImage && fileData->thumbnail->loadDone) {
							// Tooltip
							if (ImGui::IsItemHovered()) {
								ImGui::BeginTooltip();

								float size = 200;
								float img_width, img_height;

								if (fileData->thumbnail->width > fileData->thumbnail->height) {
									img_width = size;
									img_height = ((float)fileData->thumbnail->height / (float)fileData->thumbnail->width) * img_width;
								}
								else {
									img_height = size;
									img_width = ((float)fileData->thumbnail->width / (float)fileData->thumbnail->height) * img_height;
								}
								
								
								ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x/2 - img_width/2); // center image vertically
								ImGui::Image(fileData->thumbnail->texID, { img_width, img_height });
								ImGui::TextUnformatted(entry.name);

								ImGui::Separator();

								DrawDirFiles_IconsItemDesc(entry);

								if (fileData->thumbnail->origWidth != -1 && fileData->thumbnail->origHeight != -1) { // check if actual (non thumbnail scaled) size was entered
									ImGui::PushStyleColor(ImGuiCol_Text, settings.descTextCol);
									ImGui::Separator();
									ImGui::Text("Size: %dpx,%dpx",fileData->thumbnail->origWidth, fileData->thumbnail->origHeight);
									ImGui::PopStyleColor();
								}
								ImGui::EndTooltip();
							}

							float imgHeightMax = textY - cursorStart.y;
							float imgHeight = imgHeightMax;
							float imgWidth = ((float)fileData->thumbnail->width / (float)fileData->thumbnail->height) * imgHeight;
							if (imgWidth > itemWidth) {
								imgWidth = itemWidth;
								imgHeight = ((float)fileData->thumbnail->height / (float)fileData->thumbnail->width) * imgWidth;
							}
							ImGui::SetCursorPos(ImVec2{ cursorStart.x + itemWidth / 2 - imgWidth / 2, cursorStart.y + imgHeightMax/2 - imgHeight/2 });
							ImGui::Image(fileData->thumbnail->texID, ImVec2{ imgWidth, imgHeight });
						}
						else {
							// Tooltip
							if (ImGui::IsItemHovered()) {
								ImGui::BeginTooltip();
								ImGui::TextUnformatted(entry.name);
								ImGui::Separator();

								DrawDirFiles_IconsItemDesc(entry);

								ImGui::EndTooltip();
							}

							const char* iconText;
							if (!settings.asciiArtIcons) {
								iconText = entry.isFolder ? "[DIR]" : "[FILE]";
							}
							else {
								iconText = entry.isFolder ?
									"    "  "\n"
									"|=\\_."  "\n"
									"| D |"  "\n"
									"*---*"
									:
									" __ "  "\n"
									"|  \\" "\n"
									"|  |"  "\n"
									"*--*"
									;
							}
							
							ImVec2 textSize = ImGui::CalcTextSize(iconText);
							ImGui::SetCursorPos(ImVec2{ cursorStart.x + itemWidth / 2 - textSize.x / 2, cursorStart.y + (textY-cursorStart.y)/2-textSize.y/2 });
							ImGui::TextColored(settings.iconTextCol, "%s", iconText);
						}

						ImGui::SetCursorPos(ImVec2{ cursorStart.x,textY });
						{
							const bool isRenamingThis = entry.id == fd->renameId;
							const float maxWidth = cursorEnd.x - cursorStart.x;

							if (!isRenamingThis) {
								TextWrappedCentered(entry.name, maxWidth, maxTextLines);
							}
							else {
								const float textWidth = ImGui::CalcTextSize(fd->renameStr.c_str()).x + ImGui::GetStyle().FramePadding.x*2;
								const float inputWidth = textWidth < 70 ? 70 : textWidth;
								ImGui::SetCursorPosX(ImGui::GetCursorPosX() + maxWidth / 2 - inputWidth / 2);
								ImGui::SetKeyboardFocusHere();
								if (utils::InputTextString("##renameInput", "New Name", &fd->renameStr, ImGuiInputTextFlags_EnterReturnsTrue, { inputWidth,0 })) {
									ds::string path = fd->currentPath.toString();
									IM_ASSERT(path[path.size() - 1] == '/');
									bool success = Native::rename((path + entry.name).c_str(), (path + fd->renameStr).c_str());
									if (success) {
										fd->updateEntrys();
									}
									else {
										// TODO
									}
									fd->resetRename();
								}
								else {
									if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
										fd->resetRename();
									}
								}
							}
						}

						ImGui::SetCursorPos(cursorEnd);

						//ImGuiWindow* window = ImGui::GetCurrentWindow();
						//ImVec2 off = { window->Pos.x - window->Scroll.x, window->Pos.y - window->Scroll.y };
						//ImGui::GetWindowDrawList()->AddRect(cursorStart + off, cursorEnd + off, IM_COL32(255, 0, 0, 255));	

						ImGui::PopID();
					}
				}
			}
			
			ImGui::PopStyleVar();
		}
		ImGui::EndChild();
	}

	static void DrawContextMenu() {
		bool openNewFolderPopup = false;
		if (ImGui::BeginPopup("ContextMenu")) {
			if (fd->selected.size() == 1) {
				if (ImGui::MenuItem("Rename")) {
					fd->renameId = fd->getSelectedInd(0).id;
					fd->renameStr = fd->getSelectedInd(0).name;
				}
				ImGui::Separator();
			}

			if (ImGui::MenuItem("Clear Selection")) {
				fd->selected.clear();
				fd->setInputTextToSelected();
			}

			if (ImGui::MenuItem("New Folder")) {
				openNewFolderPopup = true;
			}

			ImGui::EndPopup();
		}
		if(openNewFolderPopup)
			ImGui::OpenPopup("Make New Folder");

		if (ImGui::BeginPopup("Make New Folder")) {
			ImGui::TextUnformatted("Make a new Folder");
			ImGui::Separator();
			ImGui::TextUnformatted("Enter the name of your new folder:");

			utils::InputTextString("EnterNewFolderName", "Enter the name of the new folder", &fd->newFolderNameStr);

			if (ImGui::Button("OK")) {
				bool success = Native::makeFolder((fd->currentPath.toString() + "/" + fd->newFolderNameStr).c_str());

				if(!success) {
					// TODO:
					abort(); // temporary
				}

				fd->newFolderNameStr = "";
				fd->needsEntrysUpdate = true;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				fd->newFolderNameStr = "";
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	static void DrawDirFiles() {
		float winHeight = ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y;
		float height = winHeight - (ImGui::GetCursorPosY()-ImGui::GetCursorStartPos().y) - ImGui::GetFrameHeightWithSpacing() * (fd->hasFilter ? 2 : 1);
		switch (settings.displayMode) {
			case GlobalSettings::DisplayMode_List:
				DrawDirFiles_Table(height);
				break;
			case GlobalSettings::DisplayMode_Icons:
				DrawDirFiles_Icons(height);
				break;
		}

		if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
			ImGui::OpenPopup("ContextMenu");
		}
		DrawContextMenu();
	}

	static bool canOpenNow() {
		if (fd->isFileMode()) {
			for (size_t i = 0; i < fd->selected.size(); i++) {
				auto& entry = fd->getSelectedInd(i);
				if (entry.isFolder)
					return false;
			}
			//for (auto& id : fd->selected) {
			//	
			//}
			return fd->inputText.size() > 0;
		}
		else if (fd->isDirMode()) {
			return true;
		}
		else {
			abort();
		}
	}
	static void DrawTextField() {
		const ImGuiStyle& style = ImGui::GetStyle();

		const char* openBtnStr = fd->mode == ImGuiFDMode_SaveFile ? "Save" : "Open";
		const char* cancelBtnStr = "Cancel";
		const float minBtnWidth = ImMin(100.0f, ImGui::GetContentRegionAvail().x/4);
		const float btnWidht = ImMax(ImMax(ImGui::CalcTextSize(openBtnStr).x, ImGui::CalcTextSize(cancelBtnStr).x), minBtnWidth) + style.FramePadding.x*2;
		const float widthWOBtns = ImGui::GetContentRegionAvail().x - 2 * (btnWidht + ImGui::GetStyle().ItemSpacing.x);

		ImGui::PushItemWidth(-FLT_MIN);
		// file name text input
		utils::InputTextString("##Name", "File Name", &fd->inputText, 0, { ImVec2{ fd->hasFilter ? 0 : widthWOBtns, 0 } });

		// filter combo
		if (fd->hasFilter) {
			IM_ASSERT(fd->entrys.filter.filters.size() > 0);
			ImGui::PushItemWidth(widthWOBtns);

			if (ImGui::BeginCombo("##filter", fd->entrys.filter.filters[fd->entrys.filter.filterSel].rawStr.c_str())) {
				for (size_t i = 0; i < fd->entrys.filter.filters.size(); i++) {
					ImGui::PushID((ImGuiID)i);

					bool isSelected = i == fd->entrys.filter.filterSel;
					if (ImGui::Selectable(fd->entrys.filter.filters[i].rawStr.c_str(), isSelected)) {
						if (fd->entrys.filter.filterSel != i) {
							fd->entrys.filter.filterSel = i;
							fd->updateFiltering();
						}
					}
						

					if (isSelected)
						ImGui::SetItemDefaultFocus();

					ImGui::PopID();
				}

				ImGui::EndCombo();
			}

			ImGui::PopItemWidth();
		}

		ImGui::SameLine();

		{
			const bool canOpen = canOpenNow();

			bool drawOpen = true;
			if (!canOpen && fd->isFileMode() && fd->selected.size() == 1 && fd->getSelectedInd(0).isFolder) {
				drawOpen = false;
				if (ImGui::Button("Open Folder", {btnWidht,0})) {
					fd->dirMoveDownInto(fd->entrys.get(*fd->selected.begin()).name);
				}
			}

			if (!canOpen) ImGui::BeginDisabled();
			if (drawOpen) {
				// Open Button
				if (ImGui::Button(openBtnStr, { btnWidht,0 })) {
					bool done = true;

					if (fd->mode == ImGuiFDMode_SaveFile) {
						ds::string path = fd->currentPath.toString() + "/" + fd->inputText.substr(1, fd->inputText.size()-1);
						if (Native::fileExists(path.c_str())) {
							done = false;
							ImGui::OpenPopup("Override File?");
						}
					}

					if (done) {
						OpenNow();
					}
				}
			}
			if (!canOpen) ImGui::EndDisabled();
		}

		ImGui::SameLine();
		// Close Button
		if(ImGui::Button(cancelBtnStr, { btnWidht,0 })) {
			fd->actionDone = true;
			fd->selectionMade = false;
		}
		ImGui::PopItemWidth();

		
		ImGui::SetNextWindowPos(ImGui::GetWindowPos() + ImGui::GetWindowSize() / 2, ImGuiCond_Appearing, {0.5f,0.5f});
		if (ImGui::BeginPopup("Override File?")) {
			ImGui::TextUnformatted("This file already Exists!");
			ImGui::Separator();

			ImGui::Text("Override the File %s?", fd->inputText.c_str());
			ImGui::Spacing();

			if (ImGui::Button("Override")) {
				OpenNow();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	

	static void CloseDialogID(ImGuiID id) {
		if (openDialogs.contains(id))
			openDialogs.getByID(id).toDelete = true;
	}
}

ImGuiFD::RequestFileDataCallback ImGuiFD::FileDataCache::requestFileDataCallB = 0;
ImGuiFD::FreeFileDataCallback ImGuiFD::FileDataCache::freeFileDataCallB = 0;

ImGuiFD::DirEntry::DirEntry() {

}
ImGuiFD::DirEntry::DirEntry(const DirEntry& src){
	operator=(src);
}
ImGuiFD::DirEntry& ImGuiFD::DirEntry::operator=(const DirEntry& src) {
	id = src.id;
	name = src.name ? ImStrdup(src.name) : 0;
	dir  = src.dir  ? ImStrdup(src.dir)  : 0;
	path = src.dir  ? ImStrdup(src.path) : 0;
	isFolder = src.isFolder;

	size = src.size;
	lastModified = src.lastModified;
	creationDate = src.creationDate;

	return *this;
}
ImGuiFD::DirEntry::~DirEntry() {
	IM_FREE((void*)name);
	IM_FREE((void*)dir );
	IM_FREE((void*)path);
}

uint64_t ImGuiFD::FileData::getSize() const {
	uint64_t size = 0;
	if (thumbnail)
		size += thumbnail->memSize;
	return size;
}

void ImGuiFD::SetFileDataCallback(RequestFileDataCallback loadCallB, FreeFileDataCallback unloadCallB) {
	FileDataCache::requestFileDataCallB = loadCallB;
	FileDataCache::freeFileDataCallB = unloadCallB;
}

ImGuiFD::FDInstance::FDInstance(const char* str_id) : str_id(str_id), id(ImHashStr(str_id)){

}
void ImGuiFD::FDInstance::OpenDialog(ImGuiFDMode mode, const char* path, const char* filter, ImGuiFDDialogFlags flags, size_t maxSelections) {
	ImGuiFD::OpenDialog(str_id.c_str(), mode, path, filter, flags, maxSelections);
}
bool ImGuiFD::FDInstance::Begin() const {
	return ImGuiFD::BeginDialog(id);
}
void ImGuiFD::FDInstance::End() const {
	ImGuiFD::EndDialog();
}
void ImGuiFD::FDInstance::DrawDialog(void (*callB)(void* userData), void* userData) const {
	if(Begin()) {
		if(ImGuiFD::ActionDone()) {
			if(ImGuiFD::SelectionMade()) {
				callB(userData);
			}
			ImGuiFD::CloseCurrentDialog();
		}
		End();
	}
}
size_t ImGuiFD::FDInstance::sizeBytes() const {
	return ds::size_bytes(str_id) + sizeof(id);
}




void ImGuiFD::OpenDialog(const char* str_id, ImGuiFDMode mode, const char* path, const char* filter, ImGuiFDDialogFlags flags, size_t maxSelections) {
	ImGuiID id = ImHashStr(str_id);
#if 0
	IM_ASSERT(!openDialogs.contains(id));
#else
	if (openDialogs.contains(id)) {
		if (openDialogs.getByID(id).toDelete) {
			openDialogs.erase(id);
		}
		else {
			return;
		}
	}
#endif
	openDialogs.insert(id, FileDialog(id, str_id, filter, path, mode, flags, maxSelections));
}

void ImGuiFD::CloseDialog(const char* str_id) {
	ImGuiID id = ImHashStr(str_id);
	CloseDialogID(id);
}
void ImGuiFD::CloseCurrentDialog() {
	IM_ASSERT(fd != 0);
	ImGuiID id = fd->id;
	CloseDialogID(id);
}

bool ImGuiFD::BeginDialog(const char* str_id) {
	ImGuiID id = ImHashStr(str_id);
	return BeginDialog(id);
}
bool ImGuiFD::BeginDialog(ImGuiID id) {
	// Begin/End mismatch
	IM_ASSERT(fd == 0);

	if (!openDialogs.contains(id))
		return false;
	
	fd = &openDialogs.getByID(id);

	ImGuiWindowFlags flags = 0;
	//if (fd->isModal) flags |= ImGuiWindowFlags_Modal;

	bool open = true;
	bool ret;

	ImGui::SetNextWindowSize(ImVec2(700, 400), ImGuiCond_Appearing);
	if (ImGui::Begin(fd->str_id.c_str(), &open, flags)) {
		fd->update();

		if (fd->showLoadErrorMsg) {
			fd->showLoadErrorMsg = false;
			ImGui::OpenPopup("Couldn't load directory! ");
		}

		if (ImGui::BeginPopupModal("Couldn't load directory! ")) {
			ImGui::TextUnformatted("Couln't load this directory :(");
			if (ImGui::Button("OK")) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
			goto skip;
		}
		
		DrawNavigation();

		DrawDirFiles();

		DrawTextField();

	skip:
		if (!open) {
			fd->actionDone = true;
			fd->selectionMade = false;
		}
		ret = true;
	}
	else {
		fd = 0;
		ret = false;
	}
	
	ImGui::End();

	return ret;
}
void ImGuiFD::EndDialog() {
	// Begin/End mismatch
	IM_ASSERT(fd != 0);

	if (fd->toDelete)
		openDialogs.erase(fd->id);

	fd = 0;
}

bool ImGuiFD::ActionDone() {
	IM_ASSERT(fd != 0);
	return fd->actionDone;
}
bool ImGuiFD::SelectionMade() {
	IM_ASSERT(fd != 0);
	return fd->selectionMade;
}
const char* ImGuiFD::GetResultStringRaw() {
	IM_ASSERT(fd != 0);

	IM_ASSERT(fd->selectionMade);

	return fd->inputText.c_str();
}
size_t ImGuiFD::GetSelectionStringsAmt() {
	IM_ASSERT(fd != 0);

	IM_ASSERT(fd->selectionMade);

	return fd->selected.size();
}
const char* ImGuiFD::GetSelectionNameString(size_t ind) {
	IM_ASSERT(fd != 0);

	IM_ASSERT(fd->selectionMade); // maybe you didn't check if a selection was made?

	return fd->inputStrs[ind].first.c_str();
}
const char* ImGuiFD::GetSelectionPathString(size_t ind) {
	IM_ASSERT(fd != 0);

	IM_ASSERT(fd->selectionMade); // maybe you didn't check if a selection was made?

	return fd->inputStrs[ind].second.c_str();
}

void ImGuiFD::DrawDebugWin(const char* str_id) {
	IM_ASSERT(fd == 0);

	ImGuiID id = ImHashStr(str_id);

	if (!openDialogs.contains(id))
		return;

	fd = &openDialogs.getByID(id);

	if (ImGui::Begin((fd->str_id + "_DEBUG").c_str())) {
		float perc = ((float)fd->fileDataCache.size() / (float)fd->fileDataCache.maxSize())*100;
		ImGui::Text("DataLoader: %" PRIu64 "/%" PRIu64 "(%f%%) used", fd->fileDataCache.size(), fd->fileDataCache.maxSize(), perc);

		/*ImGui::Text("%d loaded", fd->fileDataCache.getOrder().size());

		if (ImGui::BeginTable("Loaded", 5)) {
			const auto& order = fd->fileDataCache.getOrder();
			const auto& data = fd->fileDataCache.getData();

			ImGui::TableSetupColumn("TN");
			ImGui::TableSetupColumn("Need");
			ImGui::TableSetupColumn("Finished");

			for (size_t i = 0; i < order.size(); i++) {
				const auto& elem = data.getByID(order[i]);

				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				ImGui::Text("%s", !!elem->thumbnail ? "#" : ".");

				ImGui::TableNextColumn();

				ImGui::Text("%s", !!elem->stillNeeded ? "#" : ".");

				ImGui::TableNextColumn();

				ImGui::Text("%d", elem->loadingFinished);
			}

			ImGui::EndTable();
		}*/

	}
	ImGui::End();

	fd = 0;
}

void ImGuiFD::Shutdown() {
	openDialogs.clear(); // this is crucial to call all the deconstructors before the stuff they depend on gets shut down
}



/*

// remove buttons from the front until the ellipse button also fits in
//while (totalWidth + ellipseBtnWidth > width && lastToFit < fd->currentPath.parts.size()) {
//	totalWidth -= ImGui::CalcTextSize(fd->currentPath.parts[lastToFit].c_str(), NULL, true).x + style.FramePadding.x * 2 + style.ItemSpacing.x;
//	lastToFit++;
//}

*/