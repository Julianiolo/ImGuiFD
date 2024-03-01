#ifndef __IMGUIFD_INTERNAL_H__
#define __IMGUIFD_INTERNAL_H__

#define IMGUI_DEFINE_MATH_OPERATORS 1
#include "ImGuiFD.h"
#include "imgui_internal.h"

#ifdef IMGUIFD_ENABLE_STL
#include <functional>
#include <vector>
#include <string>
#include <iterator>
#endif

namespace ds {
#ifdef IMGUIFD_ENABLE_STL
	template<typename T>
	using vector = std::vector<T>;

	using string = std::string;
	template<typename T1, typename T2>
	using pair = std::pair<T1, T2>;
#else
	template<typename T>
	class vector{
	private:
		size_t              Size;
		size_t              Capacity;
		T*                  Data;
	public:
		// Provide standard typedefs but we don't use them ourselves.
		typedef T                   value_type;
		typedef value_type*         iterator;
		typedef const value_type*   const_iterator;

		// Constructors, destructor
		inline vector()                                         { Size = Capacity = 0; Data = NULL; }
		inline vector(const vector<T>& src)                     { Size = Capacity = 0; Data = NULL; operator=(src); }
		inline vector(vector<T>&& src) noexcept { 
			Size = src.Size; 
			Capacity = src.Capacity;
			Data = src.Data;
			src.Data = NULL;
			src.clear();
		}
		inline vector(size_t size_) { 
			Size = Capacity = 0; 
			Data = NULL; 
			resize(size_);
		}
		inline vector(size_t size_, const T& def) { 
			Size = Capacity = 0; 
			Data = NULL; 
			resize(size_); 
			for (size_t i = 0; i < size_; i++)
				this->operator[](i) = def;
		}
		inline vector<T>& operator=(const vector<T>& src)       { 
			clear(); 
			reserve(src.Size);
			Size = src.Size;
			for(size_t i = 0; i<src.Size;i++) 
				IM_PLACEMENT_NEW(&Data[i]) T(src.Data[i]); 
			return *this;
		}
		inline ~vector() {
			clear();
		}

		inline void clear()                             { 
			if (Data) { 
				for (size_t n = 0; n < Size; n++) 
					Data[n].~T(); 
				Size = Capacity = 0; 
				IM_FREE(Data); Data = NULL; 
			} 
		}

		inline bool         empty() const                       { return Size == 0; }
		inline size_t       size() const                        { return Size; }
		inline size_t       size_in_bytes() const               { return Size * (int)sizeof(T); }
		inline size_t       max_size() const                    { return 0x7FFFFFFF / (int)sizeof(T); }
		inline size_t       capacity() const                    { return Capacity; }
		inline T&           operator[](size_t i)                { IM_ASSERT(i < Size); return Data[i]; }
		inline const T&     operator[](size_t i) const          { IM_ASSERT(i < Size); return Data[i]; }

		inline T*           begin()                             { return Data; }
		inline const T*     begin() const                       { return Data; }
		inline T*           end()                               { return Data + Size; }
		inline const T*     end() const                         { return Data + Size; }
		inline T&           front()                             { IM_ASSERT(Size > 0); return Data[0]; }
		inline const T&     front() const                       { IM_ASSERT(Size > 0); return Data[0]; }
		inline T&           back()                              { IM_ASSERT(Size > 0); return Data[Size - 1]; }
		inline const T&     back() const                        { IM_ASSERT(Size > 0); return Data[Size - 1]; }
		inline void         swap(vector<T>& rhs)                { size_t rhs_size = rhs.Size; rhs.Size = Size; Size = rhs_size; size_t rhs_cap = rhs.Capacity; rhs.Capacity = Capacity; Capacity = rhs_cap; T* rhs_data = rhs.Data; rhs.Data = Data; Data = rhs_data; }

		inline size_t       _grow_capacity(size_t sz) const     { size_t new_capacity = Capacity ? (Capacity + Capacity / 2) : 8; return new_capacity > sz ? new_capacity : sz; }
		inline void         resize(size_t new_size)             { 
			if (new_size > Capacity) 
				reserve(_grow_capacity(new_size)); 
			for (size_t i = Size; i < new_size; i++) {
				IM_PLACEMENT_NEW(&Data[i]) T();
			}
			Size = new_size; 
		}
		inline void         resize(size_t new_size, const T& v) { if (new_size > Capacity) reserve(_grow_capacity(new_size)); if (new_size > Size) for (size_t n = Size; n < new_size; n++) IM_PLACEMENT_NEW(&Data[n]) T(v); Size = new_size; }
		inline void         shrink(size_t new_size)             {  // Resize a vector to a smaller size, guaranteed not to cause a reallocation
			IM_ASSERT(new_size <= Size); 
			for (size_t i = new_size; i < Size; i++) {
				Data[i].~T();
			}
			Size = new_size; 
		}
		inline void         reserve(size_t new_capacity)        { 
			if (new_capacity <= Capacity) 
				return; 
			T* new_data = (T*)IM_ALLOC((size_t)new_capacity * sizeof(T)); 
			if (Data) { 
				//memcpy(new_data, Data, (size_t)Size * sizeof(T));
				for(size_t i = 0; i < Size; i++) {
					IM_PLACEMENT_NEW(&new_data[i]) T((T&&)Data[i]);
					Data[i].~T();
				}
				IM_FREE(Data);
			}
			Data = new_data; 
			Capacity = new_capacity;
		}

		// NB: It is illegal to call push_back/push_front/insert with a reference pointing inside the Imvector data itself! e.g. v.push_back(v[10]) is forbidden.
		inline void         push_back(const T& v)               { 
			if (Size == Capacity) 
				reserve(_grow_capacity(Size + 1)); 
			IM_PLACEMENT_NEW(&Data[Size]) T(v); 
			Size++; 
		}
		inline void         pop_back()                          { IM_ASSERT(Size > 0); Size--; }
		inline void         push_front(const T& v)              { if (Size == 0) push_back(v); else insert(Data, v); }
		inline T*           erase(const T* it)                  { 
			IM_ASSERT(it >= Data && it < Data + Size); 
			it->~T(); 
			const ptrdiff_t off = it - Data; 
			//memmove(Data + off, Data + off + 1, (Size - (size_t)off - 1) * sizeof(T)); 
			for(size_t i = off; i+1 < Size; i++) {
				Data[i] = Data[i+1];
			}
			Size--; 
			return Data + off;
		}
		inline T*           erase(const T* it, const T* it_last){ 
			IM_ASSERT(it >= Data && it < Data + Size && it_last > it && it_last <= Data + Size); 
			it->~T(); const ptrdiff_t count = it_last - it; 
			const ptrdiff_t off = it - Data; 
			//memmove(Data + off, Data + off + count, ((size_t)Size - (size_t)off - count) * sizeof(T)); 
			for(size_t i = off; i+count < Size; i++) {
				Data[i] = Data[i+count];
			}
			Size -= (int)count; 
			return Data + off; 
		}
		inline T*           insert(const T* it, const T& v)     { 
			IM_ASSERT(it >= Data && it <= Data + Size); 
			const ptrdiff_t off = it - Data; 
			if (Size == Capacity) 
				reserve(_grow_capacity(Size + 1)); 
			if (off < (ptrdiff_t)Size) {
				//memmove(Data + off + 1, Data + off, ((size_t)Size - (size_t)off) * sizeof(T));
				for(size_t i = off; i+1 < Size; i++) {
					Data[i+1] = Data[i];
				}
			}
				 
			IM_PLACEMENT_NEW(&Data[off]) T(v); Size++; 
			return Data + off; 
		}
		inline bool         contains(const T& v) const          { const T* data = Data;  const T* data_end = Data + Size; while (data < data_end) if (*data++ == v) return true; return false; }
		inline T*           find(const T& v)                    { T* data = Data;  const T* data_end = Data + Size; while (data < data_end) if (*data == v) break; else ++data; return data; }
		inline const T*     find(const T& v) const              { const T* data = Data;  const T* data_end = Data + Size; while (data < data_end) if (*data == v) break; else ++data; return data; }
		inline bool         find_erase(const T& v)              { const T* it = find(v); if (it < Data + Size) { erase(it); return true; } return false; }
		inline size_t       index_from_ptr(const T* it) const   { IM_ASSERT(it >= Data && it < Data + Size); const ptrdiff_t off = it - Data; return (size_t)off; }
	};

	template<typename T0, typename T1>
	class pair {
	public:
		T0 first;
		T1 second;
		inline pair(const T0& t0_, const T1& t1_) : first(t0_), second(t1_){}

		inline bool operator==(const pair<T0, T1>& other) {
			return first == other.first && second == other.second;
		}
	};

	class string {
	public:
		vector<char> data;
		inline string() : data(1,0) {
			
		}
		inline string(const char* s, const char* s_end = 0) : data((s_end ? (s_end-s) : strlen(s))+1) {
			for (size_t i = 0; i < data.size(); i++) {
				data[i] = s[i];
			}
			data.back() = 0;
		}

		inline const char* c_str() const {
			return &data[0];
		}

		inline string substr(ptrdiff_t from, ptrdiff_t to) const {
			size_t from_ = from >= 0 ? from : len() + from;
			size_t to_ = to >= 0 ? to : len() + to;
			return ds::string(data.begin() + from_, data.begin() + to_);
		}

		inline size_t len() const {
			return data.size() > 0 ? (data.size()-1) : 0;
		}
		inline size_t size() const {
			return len();
		}
		inline size_t capacity() const {
			return data.capacity();
		}

		inline void resize(size_t s) {
			data.resize(s + 1);
		}
		inline void reserve(size_t s) {
			data.reserve(s + 1);
		}


		inline char& operator[](size_t off) {
			return data[off];
		}
		inline const char& operator[](size_t off) const {
			return data[off];
		}

		inline string& operator+=(const ds::string& s) {
			size_t startSize = len();
			data.resize(len() + s.len() + 1);

			for (size_t i = 0; i < s.data.size(); i++) {
				data[startSize+i] = s.data[i];
			}
			return *this;
		}
		inline string& operator+=(char c) {
			data.back() = c;
			data.push_back(0);
			return *this;
		}

		inline string operator+(const ds::string& s) const {
			string out;
			out.data.resize(len() + s.len() + 1);

			for (size_t i = 0; i < data.size(); i++) {
				out.data[i] = data[i];
			}

			size_t startSize = len();
			for (size_t i = 0; i < s.data.size(); i++) {
				out.data[startSize+i] = s.data[i];
			}
			return out;
		}

		inline bool operator==(const ds::string& s) const {
			return strcmp(c_str(), s.c_str()) == 0;
		}
		inline bool operator==(const char* s) const {
			return strcmp(c_str(), s) == 0;
		}

		inline bool operator!=(const ds::string& s) const {
			return !(*this == s);
		}
		inline bool operator!=(const char* s) const {
			return !(*this == s);
		}
	};
#endif

	inline size_t size_bytes(const ds::string& s) {
		return sizeof(s) + s.size();
	}

	inline string replace(const char* str, const char* find, const char* replace) {
		string out;

		const size_t strLen = strlen(str);
		const size_t findLen = strlen(find);

		size_t nextOcc = strstr(str, find) - str;
		for (size_t i = 0; i < strLen; i++) {
			if (nextOcc == i) {
				out += replace;
				i += findLen - 1;
				nextOcc = strstr(str + i + 1, find) - str;
				continue;
			}

			out += str[i];
		}

		return out;
	}

	// find value, if not found return (size_t)-1; compare needs to be a function like object with (const T& a, size_t ind_of_b) -> int
	template<typename T,typename CMP>
	inline size_t binarySearchExclusive(size_t len, const T& value, const CMP& compare) {
		if (len == 0)
			return (size_t)-1;

		size_t from = 0;
		size_t to = len-1;
		while (from != to) {
			size_t mid = from + (to-from) / 2;

			int cmp = compare(value, mid);

			if (cmp == 0) {
				return mid;
			}
			else if (cmp < 0) {
				if (mid == to)
					goto fail;
				to = mid;
			}
			else {
				if (mid == from)
					goto fail;
				from = mid;
			}
		}

		if (compare(value, from) == 0)
			return from;

	fail:
		return (size_t)-1;
	}

	// find value, if not found return where to insert it; compare needs to be a function like object with (const T& a, size_t ind_of_b) -> int
	template<typename T,typename CMP>
	inline size_t binarySearchInclusive(size_t len, const T& value, const CMP& compare) {
		if (len == 0)
			return 0;

		size_t from = 0;
		size_t to = len-1;
		while (from < to) {
			size_t mid = from + (to-from) / 2;

			int cmp = compare(value, mid);

			if (cmp == 0) {
				return mid;
			}
			else if (cmp < 0) {
				if (mid == to) {
					return from;
				}
				to = mid;
			}
			else {
				if (mid == from) {
					return from;
				}
				from = mid;
			}
		}

		return from;
	}

	template<typename T>
	class map {
	private:
		vector<pair<ImGuiID, T>> data;

		struct Comparer {
			const map<T>& map;
			int operator()(ImGuiID v, size_t ind) const {
				ImGuiID o = map.data[ind].first;
				if (v == o) return 0;
				return v < o ? -1 : 1;
			}
		};

		inline size_t getIndContains(const ImGuiID val) const {
			return binarySearchExclusive(data.size(), val, Comparer{ *this });
		}
		inline size_t getIndInsert(const ImGuiID val) const {
			return binarySearchInclusive(data.size(), val, Comparer{ *this });
		}
	public:

		inline T& getByID(ImGuiID id) {
			size_t ind = getIndContains(id);
			IM_ASSERT(ind != (size_t)-1);
			return data[ind].second;
		}
		inline const T& getByID(ImGuiID id) const {
			size_t ind = getIndContains(id);
			IM_ASSERT(ind != (size_t)-1);
			return data[ind].second;
		}

		inline T& insert(ImGuiID id, const T& t) {
			size_t ind = getIndInsert(id);
			data.insert(data.begin() + ind, { id,t });
			return data[ind].second;
		}

		inline void erase(ImGuiID id) {
			size_t ind = getIndContains(id);
			IM_ASSERT(ind != (size_t)-1);
			data.erase(data.begin() + ind);
		}

		inline bool contains(ImGuiID id) {
			return getIndContains(id) != (size_t)-1;
		}

		inline void clear() {
			data.clear();
		}
	};

	template<typename T>
	class set {
	private:
		using container = vector<T>;
		container data;
		using iterator = typename container::iterator;

		struct Comparer {
			const set<T>& set;
			int operator()(const T& v, size_t ind) const {
				const T& o = set.data[ind];
				if (v == o) return 0;
				return v < o ? -1 : 1;
			}
		};

		inline size_t getIndContains(const T& val) const {
			return binarySearchExclusive(data.size(), val, Comparer{ *this });
		}
		inline size_t getIndInsert(const T& val) const {
			return binarySearchInclusive(data.size(), val, Comparer{ *this });
		}
	public:

		inline void add(const T& t) {
			size_t ind = getIndInsert(t);
			if (ind >= data.size())
				data.push_back(t);
			else
				data.insert(data.begin() + ind, t);
		}

		inline T& get(size_t i) {
			return data[i];
		}
		inline T& operator[](size_t i) {
			return get(i);
		}

		inline size_t size() const {
			return data.size();
		}

		inline bool contains(const T& t) {
			return getIndContains(t) != (size_t)-1;
		}

		inline T& back() {
			return data.back();
		}

		inline void erase(iterator ptr) {
			data.erase(ptr);
		}
		inline void eraseItem(const T& t) {
			size_t ind = getIndContains(t);
			IM_ASSERT(ind != (size_t)-1);
			data.erase(data.begin() + ind);
		}

		inline void clear() {
			data.clear();
		}

		inline iterator begin() {
			return data.begin();
		}

		inline iterator end() {
			return data.end();
		}
	};

	template <typename T>
	class sortarray {
	private:
		vector<T> values;
		vector<T*> valuesSorted;

		inline void resetPtrs() {
			if (valuesSorted.size() != values.size())
				valuesSorted.resize(values.size());

			for (size_t i = 0; i < values.size();i++) {
				valuesSorted[i] = &values[i];
			}
		}
	public:
		inline sortarray() {

		}

		inline sortarray(const vector<T>& srcV) : values(srcV) {
			resetPtrs();
		}

		inline sortarray(const sortarray<T>& src) : values(src.values) {
			resetPtrs();
		}

		inline sortarray& operator=(const sortarray<T>& src) {
			values = src.values;
			resetPtrs();
			return *this;
		}

		inline void push_back(const T& t) {
			values.push_back(t);
			valuesSorted.push_back(&values.back());
		}

		inline void clear() {
			values.clear();
			valuesSorted.clear();
		}

		inline size_t size() const {
			return values.size();
		}

		inline T& getSorted(size_t i) {
			return *valuesSorted[i];
		}

		inline T& get(size_t i) {
			return values[i];
		}

		inline T* begin() {
			return values.begin();
		}
		inline T* end() {
			return values.end();
		}

		inline const T* begin() const {
			return values.begin();
		}
		inline const T* end() const {
			return values.end();
		}

		inline T** beginSortPtrs() {
			return valuesSorted.begin();
		}
		inline T** endSortPtrs() {
			return valuesSorted.end();
		}

		inline size_t getActualIndex(size_t sortInd) const {
			return valuesSorted[sortInd] - begin();
		}
	};

	template<typename T>
	class OverrideStack {
	protected:
		ds::vector<T*> arr;
		size_t start = 0; //points to the next free slot
		size_t len, currSize = 0;

	public:
		OverrideStack(size_t size_) : arr(size_, nullptr), len(size_){

		}

		void push(const T& t) {
			arr[start] = (T*)IM_ALLOC(sizeof(t));
			IM_PLACEMENT_NEW(arr[start]) T(t);
			start = (start+1)%len;
			if (currSize < len) {
				currSize++;
			}
		}
		void push(const T* t) {
			arr[start] = t;
			start = (start+1)%len;
			if (currSize < len) {
				currSize++;
			}
		}
		bool pop(T* out) {
			if (!isEmpty()) {
				if (currSize > 0) {
					currSize--;
				}

				start = (start + len - 1) % len;
				if (out != NULL) {
					*out = *arr[start];
				}

				arr[start]->~T();
				IM_FREE(arr[start]);
				arr[start] = nullptr;

				return true;
			}
			else {
				return false;
			}
		}
		bool popPtr(T** out) { // doesnt free
			if (!isEmpty()) {
				if (currSize > 0) {
					currSize--;
				}

				start = (start + len - 1) % len;
				if (out != NULL) {
					*out = arr[start];
				}

				return true;
			}
			else {
				return false;
			}
		}
		T*& at(size_t ind) {
			int stackInd = (start + len - 1 - ind) % len;
			return arr[stackInd];
		}
		void clear() {
			for (size_t i = 0; i < len; i++) {
				if (arr[i] != nullptr) {
					arr[i]->~T();
					IM_FREE(arr[i]);
					arr[i] = nullptr;
				}
			}

			currSize = 0;
		}

		bool isEmpty() const{
			return currSize == 0;
		}
		size_t size() const{
			return currSize;
		}

		void resize(size_t newSize) {
			if (newSize == len)
				return;

			ds::vector<T*> newVec(newSize, nullptr);

			if (newSize > len) {
				for (size_t i = 0; i < len; i++) {
					newVec[i] = at(len - 1 - i);
				}
				start = len;
			}
			else {
				for (size_t i = 0; i < newSize; i++) {
					newVec[newSize - 1 - i] = at(i);
				}
				for (size_t i = newSize; i < len; i++) {
					arr[i]->~T();
					IM_FREE(arr[i]);
				}
				start = 0;
			}

			len = newSize;
			arr = newVec;
		}
	};
}

namespace ImGuiFD {
	struct FDInstance {
		ds::string str_id;
        ImGuiID id;

        FDInstance(const char* str_id);

        void OpenDialog(ImGuiFDMode mode, const char* path, const char* filter = NULL, ImGuiFDDialogFlags flags = 0, size_t maxSelections = 1);

        bool Begin() const;
        void End() const;
		
		// for ease of use, not nescessary
		void DrawDialog(void (*callB)(void* userData), void* userData = NULL) const;

#ifdef IMGUIFD_ENABLE_STL
		inline void DrawDialog(std::function<void(void)> callB) const {
			if(Begin()) {
				if(ImGuiFD::ActionDone()) {
					if(ImGuiFD::SelectionMade()) {
						callB();
					}
					ImGuiFD::CloseCurrentDialog();
				}
				End();
			}
		}
#endif

		size_t sizeBytes() const;
    };

	namespace Native {
		constexpr size_t MAX_PATH_LEN = 1024;

		ds::string getAbsolutePath(const char* path);
		bool isValidDir(const char* dir);

		ds::vector<DirEntry> loadDirEnts(const char* path, bool* success = 0);
		bool fileExists(const char* path);

		bool rename(const char* name, const char* newName);

		bool makeFolder(const char* path);

		ds::string makePathStrOSComply(const char* path);
	}
}

#endif