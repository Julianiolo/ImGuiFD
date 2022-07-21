#include "ImGuiFD.h"
#include "ImGuiFD_internal.h"

#include <string.h>

#include <stdint.h>
#include <time.h> // used for localtime() and strftime()

#include <inttypes.h> // used for u64 in format string

namespace ImGuiFD {
	namespace utils {
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

				out.data.resize(len + 1);
			}

			size_t ind = 0;
			size_t indOut = 0;
			bool lastWasSlash = false;
			while (path[ind]) {
				char c = path[ind];
				switch (c) {
				case '\\':
					c = '/'; // no break here is intended, I think?
				case '/':
				{
					if (!lastWasSlash) {
						lastWasSlash = true;
						out.data[indOut++] = c;
					}
					break;
				}
				default:
					lastWasSlash = false;
					out.data[indOut++] = c;
					break;
				}
				ind++;
			}
			out.data[indOut] = 0;

			if (out[out.len() - 1] != '/') {
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

		int TextCallBack(ImGuiInputTextCallbackData* data) {
			ds::string* str = (ds::string*)data->UserData;

			if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
				str->data.resize(data->BufTextLen+1);
				data->Buf = (char*)str->c_str();
			}
			return 0;
		}
		bool InputTextString(const char* label, const char* hint, ds::string* str, ImGuiInputTextFlags flags = 0, const ImVec2& size = { 0,0 }) {
			return ImGui::InputTextEx(
				label, hint, (char*)str->c_str(), (int)str->data.capacity(), 
				size, flags | ImGuiInputTextFlags_CallbackResize, TextCallBack, str
			);
		}
	}

	ImGuiTableSortSpecs* sortSpecs = 0;

	enum {
		DEIG_NAME = 0,
		DEIG_SIZE = 1,
		DEIG_LASTMOD_DATE,
		DEIG_CREATION_DATE,
	};

	int compareSortSpecs(const void* lhs, const void* rhs){
		auto& a = **(DirEntry**)lhs;
		auto& b = **(DirEntry**)rhs;

		if (settings.showDirFirst) {
			if (a.isFolder && !b.isFolder) return -1;
			if (!a.isFolder && b.isFolder) return 1;
		}

		for (int i = 0; i < sortSpecs->SpecsCount; i++) {
			auto& specs = sortSpecs->Specs[i];
			int delta = 0;
			switch (specs.ColumnUserID) {
			case DEIG_NAME:
				delta = strcoll(a.name, b.name);
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
				len += parts[i].len();
			}
			ds::string out;
			out.data.reserve(len+1);
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
	public:
		ds::string searchText;
		size_t filterSel = 0; // currently selected filter
		ds::vector<ds::string> filters;

		FileNameFilter() {

		}
		FileNameFilter(const char* filter) : filters(parseFilterStr(filter)) {

		}

		bool passes(const char* name) {
			if (searchText.len() == 0)
				return true;

			if (strstr(name, searchText.c_str())) {
				return true;
			}
			return false;
		}

		bool draw(float width = -1) {
			ImGui::PushItemWidth(width);
			bool ret = utils::InputTextString("##Search", "Search", &searchText);
			ImGui::PopItemWidth();
			return ret;
		}

		static ds::vector<ds::string> parseFilterStr(const char* str) {
			ds::vector<ds::string> out;
			size_t len = str ? strlen(str) : 0;
			size_t last = 0;
			for (size_t i = 0; i < len; i++) {
				if (str[i] == ',' && i>last) {
					out.push_back(ds::string(str+last,str+i));
					last = i + 1;
				}
			}
			if (len-last > 0) {
				out.push_back(ds::string(str+last,str+len));
			}
			return out;
		}
	};

	class FileDataCache {
	private:
		ds::set<size_t> loaded;
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

		bool isEditingPath = false;
		ds::string editOnPathStr;
		ds::OverrideStack<ds::string> undoStack;
		ds::OverrideStack<ds::string> redoStack;
		
		class EntryManager {
			ds::vector<DirEntry> data;
			ds::vector<DirEntry*> dataModed;

			bool loadedSucessfully = false;
		public:
			bool sorted = false;
			FileNameFilter filter;

			EntryManager(const char* filter) : filter(filter) {
				
			}

			EntryManager(const EntryManager& src) : data(src.data), filter(src.filter) {
				updateFilter();
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
				updateFilter();
			}

			DirEntry& getRaw(size_t i) {
				return data[i];
			}

			DirEntry& get(size_t i) {
				return *dataModed[i];
			}

			size_t size() {
				return dataModed.size();
			}

			size_t getActualIndex(size_t i) {
				return dataModed[i] - &data[0];
			}

			void updateFilter() {
				dataModed.clear();
				for (size_t i = 0; i < data.size(); i++) {
					if (filter.passes(data[i].name)) {
						dataModed.push_back(&data[i]);
					}
				}
				sorted = false;
			}

			void sort(ImGuiTableSortSpecs* sorts_specs) {
				sortSpecs = sorts_specs;
				if(dataModed.size() > 1)
					qsort(dataModed.begin(), (size_t)dataModed.size(), sizeof(dataModed[0]), compareSortSpecs);
				sortSpecs = 0;
				sorted = true;
			}

			void drawSeachBar(float width = -1) {
				if (filter.draw(width))
					updateFilter();
			}

			bool wasLoadedSuccesfully() {
				return loadedSucessfully;
			}
		};
		EntryManager entrys;
		FileDataCache fileDataCache;

		size_t lastSelected = -1;
		ds::set<size_t> selected;

		bool isFileDialog = true;
		bool isModal = false;
		bool hasFilter;
		
		size_t maxSelections;
		
		ds::string inputText = "";
		ds::string newFolderNameStr = "";

		bool needsEntrysUpdate = false;

		bool actionDone = false;
		bool selectionMade = false;
		bool toDelete = false;
		bool showLoadErrorMsg = false;

		FileDialog(ImGuiID id, const char* str_id, const char* filter, const char* path, bool isFileDialog, ImGuiFDDialogFlags flags = 0, size_t maxSelections = 1) : 
			str_id(str_id), id(id), path(utils::fixDirStr(Native::getAbsolutePath(path).c_str())), 
			currentPath(this->path.c_str()), oldPath(this->path),
			undoStack(32), redoStack(32),
			entrys(filter),
			isFileDialog(isFileDialog), isModal((flags&ImGuiFDDialogFlags_Modal)!=0), hasFilter(filter != NULL), maxSelections(maxSelections)
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
				if (isFileDialog) {
					inputText = "";
				}
				else {
					inputText = currentPath.toString(); // if selecting a directory, always have the folder we are in as default if nothing is selected
				}
			}
			else {
				inputText = "\"";
				for (size_t i = 0; i < selected.size(); i++) {
					if(i>0)
						inputText += "\", \"";
					inputText += entrys.get(selected[i]).name;
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

				lastSelected = -1;
				selected.clear();
				oldPath = curDirStr;
			}
			else {
				showLoadErrorMsg = true;
				couldntLoadPath = curDirStr;
				currentPath.setToStr(oldPath.c_str());
			}
		}

		bool canUndo() {
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
		bool canRedo() {
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
	};

	FileDialog* fd = 0;
	ds::map<FileDialog> openDialogs;

	void TextWrappedCentered(const char* text, float maxWidth, int maxLines = -1) {
		ImGuiContext& g = *GImGui;
		ImVec2 cursorStart = ImGui::GetCursorPos();

		size_t len = strlen(text);

		const char* s = text;
		int lineInd = 0;

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

			ImGui::SetCursorPos(cursorStart + ImVec2{maxWidth/2-lineSize.x/2, ImGui::GetTextLineHeightWithSpacing()*lineInd});
			if (lineInd + 1 >= maxLines && wrap < text+len) {
				ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(), ImGui::GetCursorScreenPos(), ImGui::GetCursorScreenPos() + lineSize + ImVec2{0,g.Style.FramePadding.y}, ImGui::GetCursorScreenPos().x + maxWidth, ImGui::GetCursorScreenPos().x + maxWidth, s, text + len, 0);
			}
			else {
				ImGui::TextUnformatted(s, wrap);
			}
			

			s = wrap;
			lineInd++;
		}
		ImGui::PopStyleVar();
	}
	bool ComboVertical(const char* str_id, size_t* v, const char** labels, size_t labelCnt, const ImVec2& size_arg = { 0,0 }) {
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

	void formatTime(char* buf, size_t bufSize, time_t unixTime) {
		tm* tm = localtime(&unixTime);
		strftime(buf, bufSize, "%d.%m.%y %H:%M", tm);
	}
	void formatSize(char* buf, size_t bufSize, uint64_t size) {
		snprintf(buf, bufSize, "%" PRIu64 " Bytes", size);
	}

	void ClickedOnEntrySelect(size_t ind, bool isSel, bool isFolder) {
		if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			return;
		if (!fd->isFileDialog && !isFolder)
			return;

		if (ImGui::GetIO().KeyShift == ImGui::GetIO().KeyCtrl) { // both on or both off => standard select
			if (isSel) {
				if (fd->selected.size() != 1) {
					fd->selected.clear();
					fd->selected.add(ind);
				}
			}
			else {
				if (fd->selected.size() > 0)
					fd->selected.clear();
				fd->selected.add(ind);
			}
		}
		else if (ImGui::GetIO().KeyShift) {
			size_t a = fd->lastSelected;
			if (a == (size_t)-1)
				a = ind;
			size_t b = ind;
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
				fd->selected.add(ind);

				// if too many selected delete the earlier selected ones
				if (fd->selected.size() > fd->maxSelections)
					fd->selected.erase(fd->selected.begin());
			}
			else {
				fd->selected.eraseItem(ind);
			}
		}

		fd->setInputTextToSelected();
	}
	void CheckDoubleClick(const DirEntry& entry) {
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
			if (entry.isFolder) {
				fd->dirMoveDownInto(entry.name);
			}
			else {
				if (fd->isFileDialog) {
					fd->actionDone = true;
					fd->selectionMade = true;
				}
			}
		}
	}
	
	void DrawContorls() {
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
	void DrawDirBar() {
		const float width = ImGui::GetContentRegionAvail().x;
		const ImVec2 borderPad(2, 2);
		const ImRect rec(ImGui::GetCursorScreenPos(), ImGui::GetCursorScreenPos() + ImVec2(width, ImGui::GetFrameHeight()));
		const ImRect recOuter(rec.Min - borderPad, rec.Max + borderPad);

		if (!fd->isEditingPath) {
			bool enterEditMode = false;

			ImGui::BeginGroup();

			// draw Background
			ImGui::GetWindowDrawList()->AddRectFilled(recOuter.Min, recOuter.Max, ImColor(ImGui::GetStyleColorVec4(ImGuiCol_TableRowBgAlt)));

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 2,0 });
			for (size_t i = 0; i < fd->currentPath.parts.size(); i++) {
				ImGui::PushID((ImGuiID)i);
				if (i > 0)
					ImGui::SameLine();
				if (ImGui::Button(fd->currentPath.parts[i].c_str())) {
					fd->dirShrinkTo(i);
				}
				ImGui::PopID();
			}
			ImGui::PopStyleVar();

			// draw border
			ImGui::GetWindowDrawList()->AddRect(recOuter.Min, recOuter.Max, ImColor(ImGui::GetStyleColorVec4(ImGuiCol_Border)));

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
	}
	void DrawNavigation() {
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
			ImGui::Checkbox("Show dir first", &settings.showDirFirst);
			ImGui::Checkbox("Adjust Icon Width", &settings.adjustIconWidth);
			ImGui::DragFloat("IconScale", &settings.iconModeSize, 1, 10, 300);
			ImGui::EndPopup();
		}
	}

	void DrawDirFiles_TableRow(size_t row) {
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
	void DrawDirFiles_Table(float height) {
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

		const ImVec2 tableScreenPos = ImGui::GetCursorScreenPos();

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
	void DrawDirFiles_IconsItemDesc(const DirEntry& entry) {
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
	void DrawDirFiles_Icons(float height) {
		if (height <= 0)
			return;

		// calculating item sizes
		const size_t numOfItems = fd->entrys.size();

		float itemWidthRaw = settings.iconModeSize;
		float itemHeight = itemWidthRaw * (3.0f / 4.0f);
		ImVec2 padding(5,5);

		float width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ScrollbarSize;

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
			clipper.Begin((int)numOfLines);
			clipper.ItemsHeight = itemHeight + style.ItemSpacing.y*2;
			while (clipper.Step()) {
				for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
					size_t itemsInThisLine = itemsPerLine;
					if ((size_t)row == numOfLines - 1)
						itemsInThisLine = numOfItems - itemsPerLine*(numOfLines-1);

					for (size_t col = 0; col < itemsInThisLine; col++) {
						size_t ind = row * itemsPerLineRaw + col;

						ImGui::PushID((ImGuiID)ind);

						auto& entry = fd->entrys.get(ind);
						bool isSel = fd->selected.contains(ind);

						ImVec2 cursorStart = totalCursorStart + ImVec2{(itemWidth+style.ItemSpacing.x*2)*col, (itemHeight+style.ItemSpacing.y*2)*row};
						ImVec2 cursorEnd = cursorStart + ImVec2{itemWidth, itemHeight};
						ImGui::SetCursorPos(cursorStart);
						ImGui::Selectable("", isSel, 0, { itemWidth, itemHeight });
						if (ImGui::IsItemClicked()) { // directly using return value of Selectable doesnt work when going into folder (instantly selects hovered item) => ImGui bug?
							ClickedOnEntrySelect(ind, isSel, entry.isFolder);
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
								float width, height;

								if (fileData->thumbnail->width > fileData->thumbnail->height) {
									width = size;
									height = ((float)fileData->thumbnail->height / (float)fileData->thumbnail->width) * width;
								}
								else {
									height = size;
									width = ((float)fileData->thumbnail->width / (float)fileData->thumbnail->height) * height;
								}
								
								
								ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x/2 - width/2); // center image vertically
								ImGui::Image(fileData->thumbnail->texID, { width, height });
								ImGui::Text("%s",entry.name);

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

							const char* iconText = entry.isFolder ? "[DIR]" : "[FILE]";
							ImVec2 textSize = ImGui::CalcTextSize(iconText);
							ImGui::SetCursorPos(ImVec2{ cursorStart.x + itemWidth / 2 - textSize.x / 2, cursorStart.y + (textY-cursorStart.y)/2-textSize.y/2 });
							ImGui::TextColored(ImColor(IM_COL32(20,20,200,255)),iconText);
						}

						ImGui::SetCursorPos(ImVec2{ cursorStart.x,textY });
						TextWrappedCentered(entry.name, cursorEnd.x - cursorStart.x, maxTextLines);

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
	void DrawDirFiles() {
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

		bool openNewFolderPopup = false;
		if (ImGui::BeginPopup("ContextMenu")) {
			if (ImGui::MenuItem("Clear Selection")) {
				fd->selected.clear();
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


	bool canOpenNow() {
		if (fd->isFileDialog) {
			return fd->selected.size() > 0;
		}
		else {
			return true;
		}
	}
	void DrawTextField() {
		ImGuiStyle& style = ImGui::GetStyle();

		const char* openBtnStr = "Open";
		const char* cancelBtnStr = "Cancel";
		const float minBtnWidth = ImMin(100.0f, ImGui::GetContentRegionAvail().x/4);
		const float btnWidht = ImMax(ImMax(ImGui::CalcTextSize(openBtnStr).x, ImGui::CalcTextSize(cancelBtnStr).x), minBtnWidth) + style.FramePadding.x*2;
		float widthWOBtns = ImGui::GetContentRegionAvail().x - 2 * (btnWidht + ImGui::GetStyle().ItemSpacing.x);

		ImGui::PushItemWidth(-FLT_MIN);
		// file name text input
		utils::InputTextString("##Name", "File Name", &fd->inputText, 0, { ImVec2{ fd->hasFilter ? 0 : widthWOBtns, 0 } });

		// filter combo
		if (fd->hasFilter) {
			IM_ASSERT(fd->entrys.filter.filters.size() > 0);
			ImGui::PushItemWidth(widthWOBtns);

			if (ImGui::BeginCombo("##filter", fd->entrys.filter.filters[fd->entrys.filter.filterSel].c_str())) {
				for (size_t i = 0; i < fd->entrys.filter.filters.size(); i++) {
					ImGui::PushID((ImGuiID)i);

					bool isSelected = i == fd->entrys.filter.filterSel;
					if (ImGui::Selectable(fd->entrys.filter.filters[i].c_str(), isSelected))
						fd->entrys.filter.filterSel = i;

					if (isSelected)
						ImGui::SetItemDefaultFocus();

					ImGui::PopID();
				}

				ImGui::EndCombo();
			}

			ImGui::PopItemWidth();
		}

		ImGui::SameLine();

		const bool canOpen = canOpenNow();
		if (!canOpen) ImGui::BeginDisabled();

		// Open Button
		if (ImGui::Button(openBtnStr, { btnWidht,0 })) {
			fd->actionDone = true;
			fd->selectionMade = true;
		}

		if (!canOpen) ImGui::EndDisabled();

		ImGui::SameLine();
		// Close Button
		if(ImGui::Button(cancelBtnStr, { btnWidht,0 })) {
			fd->actionDone = true;
			fd->selectionMade = false;
		}
		ImGui::PopItemWidth();
	}

	void CloseDialogID(ImGuiID id) {
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
	if (name) IM_FREE((void*)name);
	if (dir)  IM_FREE((void*)dir );
	if (path) IM_FREE((void*)path);
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

void ImGuiFD::OpenFileDialog(const char* str_id, const char* filter, const char* path, ImGuiFDDialogFlags flags, size_t maxSelections) {
	ImGuiID id = ImHashStr(str_id);

#if 0
	IM_ASSERT(!openDialogs.contains(id));
#else
	if (openDialogs.contains(id)) {
		if (openDialogs.getByID(id).toDelete) {
			openDialogs.erase(id);
		}
		return;
	}
#endif
	bool isFileDialog = true;
	openDialogs.insert(id, FileDialog(id, str_id, filter, path, isFileDialog, flags, maxSelections));
}
void ImGuiFD::OpenDirDialog(const char* str_id, const char* path, ImGuiFDDialogFlags flags) {
	ImGuiID id = ImHashStr(str_id);

#if 0
	IM_ASSERT(!openDialogs.contains(id));
#else
	if (openDialogs.contains(id)) {
		if (openDialogs.getByID(id).toDelete) {
			openDialogs.erase(id);
		}
		return;
	}
#endif
	bool isFileDialog = false;
	openDialogs.insert(id, FileDialog(id, str_id, NULL, path, isFileDialog, flags));
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
	// Begin/End mismatch
	IM_ASSERT(fd == 0);

	ImGuiID id = ImHashStr(str_id);

	if (!openDialogs.contains(id))
		return false;
	
	fd = &openDialogs.getByID(id);

	ImGuiWindowFlags flags = 0;
	if (fd->isModal) flags |= ImGuiWindowFlags_Modal;

	bool open = true;
	bool ret;
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
const char* ImGuiFD::GetSelectionStringRaw() {
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

	if (fd->selected.size() == 0) {
		return ""; // TODO
	}
	else {
		return fd->entrys.getRaw(fd->selected[ind]).name;
	}
}
const char* ImGuiFD::GetSelectionPathString(size_t ind) {
	IM_ASSERT(fd != 0);

	IM_ASSERT(fd->selectionMade); // maybe you didn't check if a selection was made?

	if (fd->selected.size() == 0) {
		return Native::makePathStrOSComply(fd->inputText.c_str());
	}
	else {
		return Native::makePathStrOSComply(fd->entrys.getRaw(fd->selected[ind]).path);
	}
	
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