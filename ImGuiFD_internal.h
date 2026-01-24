#ifndef __IMGUIFD_INTERNAL_H__
#define __IMGUIFD_INTERNAL_H__

#include "ImGuiFD.h"
#include "imgui_internal.h"

#include <cstddef>
#include <imgui.h>
#include <time.h>

#ifdef IMGUIFD_ENABLE_STL
#include <functional>
#include <vector>
#include <string>
#include <iterator>
#include <type_traits>
#include <utility>
#endif

extern size_t imfd_num_alloc;
extern size_t imfd_num_free;

inline void* imfd_alloc(size_t s) {
    imfd_num_alloc++;
    void* p = IM_ALLOC(s);
    return p;
}
inline void imfd_free(void* p) {
    if(p != NULL) {
        imfd_num_free++;
    }
    IM_FREE(p);
}

#define IMFD_ALLOC(_x_) imfd_alloc(_x_)//IM_ALLOC(_x_)
#define IMFD_FREE(_x_) imfd_free(_x_)//IM_FREE(_x_)
#define IMFD_NEW(_TYPE) new(ImNewWrapper(), IMFD_ALLOC(sizeof(_TYPE))) _TYPE
template<typename T> void IMFD_DELETE(T* p) { if (p) { p->~T(); IMFD_FREE(p); } }

#define IMFD_ASSERT_PARANOID(_x_) IM_ASSERT(_x_)

#if __cplusplus >= 201103L
#define IMFD_NOEXCEPT noexcept
#define IMFD_CONSTEXPR constexpr
#else
#define IMFD_NOEXCEPT
#define IMFD_CONSTEXPR const
#endif

#ifdef _WIN32
#define IMFD_UNIX_PATHS 0
#else
#define IMFD_UNIX_PATHS 1
#endif

namespace ds {
#ifdef _WIN32
    IMFD_CONSTEXPR char preffered_separator = '\\';
#else
    IMFD_CONSTEXPR char preffered_separator = '/';
#endif

#if IMFD_USE_MOVE
#ifdef IMGUIFD_ENABLE_STL
    template<class T> using remove_reference = typename std::remove_reference<T>;
    using std::forward;
    using std::move;
    template<class T> using remove_cv = typename std::remove_cv<T>;
#else
    template<class T> struct remove_reference { typedef T type; };
    template<class T> struct remove_reference<T&> { typedef T type; };
    template<class T> struct remove_reference<T&&> { typedef T type; };

    template<typename T>
    typename remove_reference<T>::type&& move(T&& t) IMFD_NOEXCEPT {
        return static_cast<typename remove_reference<T>::type&&>(t);
    }

    // https://stackoverflow.com/a/27501467
    template<typename T> T&& forward(typename remove_reference<T>::type& param) IMFD_NOEXCEPT { return static_cast<T&&>(param); }
    template<typename T> T&& forward(typename remove_reference<T>::type&& param) IMFD_NOEXCEPT { return static_cast<T&&>(param); }
    template<class T> struct remove_cv { typedef T type; };
    template<class T> struct remove_cv<const T> { typedef T type; };
    template<class T> struct remove_cv<volatile T> { typedef T type; };
    template<class T> struct remove_cv<const volatile T> { typedef T type; };
#endif
    template<class T> struct remove_cvref { using type = typename remove_cv<typename remove_reference<T>::type>::type; };
#define imfd_move(_x_) ds::move(_x_)
#else
#define imfd_move(_x_) (_x_)
#endif


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
#if IMFD_USE_MOVE
        inline vector(vector<T>&& src) IMFD_NOEXCEPT {
            IM_ASSERT(this != &src);
            Size = src.Size; 
            Capacity = src.Capacity;
            Data = src.Data;
            src.Data = NULL;
            src.Size = 0;
            src.Capacity = 0;
        }
#endif
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
#if IMFD_USE_MOVE
        inline vector<T>& operator=(vector<T>&& src) IMFD_NOEXCEPT { 
            IM_ASSERT(this != &src);
            clear();

            Size = src.Size; 
            Capacity = src.Capacity;
            Data = src.Data;
            src.Data = NULL;
            src.Size = 0;
            src.Capacity = 0;

            return *this;
        }
#endif
        inline ~vector() {
            clear();
        }

        inline void clear()                             { 
            if (Data) { 
                for (size_t n = 0; n < Size; n++) 
                    Data[n].~T(); 
                Size = Capacity = 0; 
                IMFD_FREE(Data); Data = NULL; 
            }
        }

        inline bool         empty() const                       { return Size == 0; }
        inline size_t       size() const                        { return Size; }
        inline size_t       max_size() const                    { return 0x7FFFFFFF / (int)sizeof(T); }
        inline size_t       capacity() const                    { return Capacity; }
        inline T&           operator[](size_t i)                { IM_ASSERT(i < Size); return Data[i]; }
        inline const T&     operator[](size_t i) const          { IM_ASSERT(i < Size); return Data[i]; }

        inline T*           data()                              { return Data; }
        inline const T*     data() const                        { return Data; }
        inline T*           begin()                             { return Data; }
        inline const T*     begin() const                       { return Data; }
        inline T*           end()                               { return Data + Size; }
        inline const T*     end() const                         { return Data + Size; }
        inline T&           front()                             { IM_ASSERT(Size > 0); return Data[0]; }
        inline const T&     front() const                       { IM_ASSERT(Size > 0); return Data[0]; }
        inline T&           back()                              { IM_ASSERT(Size > 0); return Data[Size - 1]; }
        inline const T&     back() const                        { IM_ASSERT(Size > 0); return Data[Size - 1]; }
        inline friend void  swap(vector<T>& lhs, vector<T>& rhs){ size_t rhs_size = rhs.Size; rhs.Size = lhs.Size; lhs.Size = rhs_size; size_t rhs_cap = rhs.Capacity; rhs.Capacity = lhs.Capacity; lhs.Capacity = rhs_cap; T* rhs_data = rhs.Data; rhs.Data = lhs.Data; lhs.Data = rhs_data; }

        inline size_t       _grow_capacity(size_t sz) const     { size_t new_capacity = Capacity ? (Capacity + Capacity / 2) : 8; return new_capacity > sz ? new_capacity : sz; }
        inline void         resize(size_t new_size)             { 
            if(new_size == Size) return;
            if(new_size < Size) {
                shrink(new_size);
                return;
            }
            if (new_size > Capacity) {
                reserve(_grow_capacity(new_size));
            }
            for (size_t i = Size; i < new_size; i++) {
                IM_PLACEMENT_NEW(&Data[i]) T();
            }
            Size = new_size; 
        }
        inline void         resize(size_t new_size, const T& v) {
            if(new_size == Size) return;
            if(new_size < Size) {
                shrink(new_size);
                return;
            }
            if (new_size > Capacity) 
                reserve(_grow_capacity(new_size)); 
            for (size_t n = Size; n < new_size; n++) 
                IM_PLACEMENT_NEW(&Data[n]) T(v); 
            Size = new_size; 
        }
        inline void         shrink(size_t new_size)             {  // Resize a vector to a smaller size
            IM_ASSERT(new_size <= Size);
            for (size_t i = new_size; i < Size; i++) {
                Data[i].~T();
            }
            Size = new_size; 
            if(Size < (Capacity * 2) / 3) {
                // reduce capacity
                _set_capacity(_grow_capacity(Size));
            }
        }
        inline void         reserve(size_t new_capacity)        {  // only ever makes the vector bigger
            if (new_capacity <= Capacity)  {
                return;
            }
            _set_capacity(new_capacity);
        }
        inline void         _set_capacity(size_t new_capacity) {
            IM_ASSERT(new_capacity >= Size);
            T* new_data = (T*)IMFD_ALLOC((size_t)new_capacity * sizeof(T)); 
            if (Data) { 
                for(size_t i = 0; i < Size; i++) {
                    IM_PLACEMENT_NEW(&new_data[i]) T(imfd_move(Data[i]));
                    Data[i].~T();
                }
                IMFD_FREE(Data);
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
#if IMFD_USE_MOVE
        inline void         push_back(T&& v)              { 
            if (Size == Capacity) 
                reserve(_grow_capacity(Size + 1)); 
            IM_PLACEMENT_NEW(&Data[Size]) T(imfd_move(v)); 
            Size++; 
        }
#endif
        inline void         pop_back()                          { IM_ASSERT(Size > 0); Size--; }
        inline void         push_front(const T& v)              { if (Size == 0) push_back(v); else insert(Data, v); }
        inline T*           erase(const T* it)                  { 
            IM_ASSERT(it >= Data && it < Data + Size); 
            it->~T(); 
            const ptrdiff_t off = it - Data; 
            for(size_t i = off; i+1 < Size; i++) {
                Data[i] = imfd_move(Data[i+1]);
            }
            Size--; 
            return Data + off;
        }
        inline T*           erase(const T* it, const T* it_last){ 
            IM_ASSERT(it >= Data && it < Data + Size && it_last > it && it_last <= Data + Size);
            it->~T();
            const ptrdiff_t count = it_last - it;
            const ptrdiff_t off = it - Data;
            for(size_t i = off; i+count < Size; i++) {
                Data[i] = imfd_move(Data[i+count]);
            }
            Size -= (int)count;
            return Data + off;
        }
        inline void         _move_to_back_one(size_t off) {
            if (Size == Capacity) 
                reserve(_grow_capacity(Size + 1)); 
            if (off < Size) {
                //memmove(Data + off + 1, Data + off, ((size_t)Size - (size_t)off) * sizeof(T));
                for(size_t i = off; i+1 < Size; i++) {
                    Data[i+1] = imfd_move(Data[i]);
                }
            }
        }
        inline T*           insert(const T* it, const T& v)     { 
            IM_ASSERT(it >= Data && it <= Data + Size); 
            const size_t off = it - Data; 
            _move_to_back_one(off);
                 
            IM_PLACEMENT_NEW(&Data[off]) T(v);
            Size++;
            return Data + off;
        }
#if IMFD_USE_MOVE
        inline T*           insert(const T* it, T&& v)     { 
            IM_ASSERT(it >= Data && it <= Data + Size); 
            const size_t off = it - Data; 
            _move_to_back_one(off);
                 
            IM_PLACEMENT_NEW(&Data[off]) T(imfd_move(v));
            Size++;
            return Data + off;
        }
#endif
        //inline bool         contains(const T& v) const          { const T* data = Data;  const T* data_end = Data + Size; while (data < data_end) if (*data++ == v) return true; return false; }
        inline T*           find(const T& v)                    { T* data = Data;  const T* data_end = Data + Size; while (data < data_end) if (*data == v) break; else ++data; return data; }
        inline const T*     find(const T& v) const              { const T* data = Data;  const T* data_end = Data + Size; while (data < data_end) if (*data == v) break; else ++data; return data; }
        inline bool         find_erase(const T& v)              { const T* it = find(v); if (it < Data + Size) { erase(it); return true; } return false; }
        //inline size_t       index_from_ptr(const T* it) const   { IM_ASSERT(it >= Data && it < Data + Size); const ptrdiff_t off = it - Data; return (size_t)off; }
    };

    template<typename T0, typename T1>
    class pair {
    public:
        T0 first;
        T1 second;
#if !IMFD_USE_MOVE
        inline pair(const T0& t0_, const T1& t1_) : first(t0_), second(t1_) {}
#else
        template<typename T0_, typename T1_>
        inline pair(T0_&& t0_, T1_&& t1_) : first(ds::forward<T0_>(t0_)), second(ds::forward<T1_>(t1_)) {}
#endif

        inline bool operator==(const pair<T0, T1>& other) {
            return first == other.first && second == other.second;
        }
    };

    class string {
        char single_char;
        vector<char> m_data;
    public:
        inline string() : single_char(0) {
            
        }
        inline string(const char* s, const char* s_end = 0) : single_char(0) {
            size_t len = s_end ? (s_end-s) : strlen(s);
            if(len > 0) {
                m_data.resize(len+1);
                for (size_t i = 0; i < len; i++) {
                    m_data[i] = s[i];
                }
                m_data.back() = 0;
            }
        }

        inline string(const string& o) : single_char(0), m_data(o.m_data) {

        }
        string& operator=(const string& o) {
            m_data = o.m_data;
            return *this;
        }
#if IMFD_USE_MOVE
        inline string(string&&) IMFD_NOEXCEPT = default;
        string& operator=(string&&) IMFD_NOEXCEPT = default;
#endif
        inline friend void swap(string& a, string& b) IMFD_NOEXCEPT {
            swap(a.m_data, b.m_data);
            char as = a.single_char;
            a.single_char = b.single_char;
            b.single_char = as;
        }

        inline char* data() IMFD_NOEXCEPT {
            return m_data.size() == 0 ? &single_char : &m_data[0];
        }
        inline const char* c_str() const IMFD_NOEXCEPT {
            return m_data.size() == 0 ? &single_char : &m_data[0];
        }

        inline string substr(size_t from, size_t to) const {
            IM_ASSERT(from < size());
            IM_ASSERT(to <= size());
            IM_ASSERT(from <= to);
            return ds::string(begin() + from, begin() + to);
        }

        // this size does not include the null termination
        inline size_t size() const IMFD_NOEXCEPT {
            return m_data.size() > 0 ? (m_data.size()-1) : 0;
        }
        inline size_t capacity() const IMFD_NOEXCEPT {
            return m_data.capacity();
        }

        // this size does not include the null termination
        inline void resize(size_t size_) {
            m_data.resize(size_ + 1);
        }
        // this size does not include the null termination
        inline void reserve(size_t size_) {
            m_data.reserve(size_ + 1);
        }


        inline char& operator[](size_t off) IMFD_NOEXCEPT {
            IM_ASSERT(off < m_data.size());
            return m_data[off];
        }
        inline const char& operator[](size_t off) const IMFD_NOEXCEPT {
            IM_ASSERT(off < m_data.size());
            return m_data[off];
        }
        inline const char* begin() const IMFD_NOEXCEPT {
            return m_data.size() == 0 ? &single_char : m_data.begin();
        }
        inline const char* end() const IMFD_NOEXCEPT {
            return m_data.size() == 0 ? &single_char+1 : m_data.end()-1;
        }

        inline string& operator+=(const char* s) {
            const size_t len = strlen(s);
            size_t startSize = size();
            m_data.resize(size() + len + 1);

            for (size_t i = 0; i < len; i++) {
                m_data[startSize+i] = s[i];
            }
            m_data[m_data.size()-1] = 0;
            return *this;
        }
        inline string& operator+=(const ds::string& s) {
            size_t startSize = size();
            m_data.resize(size() + s.size() + 1);

            for (size_t i = 0; i < s.size(); i++) {
                m_data[startSize+i] = s.begin()[i];
            }
            m_data[m_data.size()-1] = 0;
            return *this;
        }
        inline string& operator+=(char c) {
            if(m_data.size() == 0) {
                m_data.resize(2);
                m_data[0] = c;
                m_data[1] = 0;
            } else {
                m_data.back() = c;
                m_data.push_back(0);
            }
            return *this;
        }

        inline string operator+(const ds::string& s) const {
            string out;
            out.m_data.resize(size() + s.size() + 1);

            for (size_t i = 0; i < size(); i++) {
                out.m_data[i] = m_data[i];
            }

            size_t startSize = size();
            for (size_t i = 0; i < s.size(); i++) {
                out.m_data[startSize+i] = s.m_data[i];
            }
            out.m_data[out.m_data.size()-1] = 0;
            return imfd_move(out);
        }

        inline bool operator==(const ds::string& s) const IMFD_NOEXCEPT {
            return strcmp(c_str(), s.c_str()) == 0;
        }
        inline bool operator==(const char* s) const IMFD_NOEXCEPT {
            return strcmp(c_str(), s) == 0;
        }

        inline bool operator!=(const ds::string& s) const IMFD_NOEXCEPT {
            return !(*this == s);
        }
        inline bool operator!=(const char* s) const IMFD_NOEXCEPT {
            return !(*this == s);
        }
    };
#endif

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

    inline int own_vsnprintf(char* buf, size_t buf_size, const char* fmt, va_list args) {
        #ifdef IMGUI_USE_STB_SPRINTF
        int w = stbsp_vsnprintf(buf, (int)buf_size, fmt, args);
        #else
        int w = vsnprintf(buf, buf_size, fmt, args);
        #endif
        return w;
    }
    inline int own_snprintf(char* buf, size_t buf_size, const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        int w = own_vsnprintf(buf, (int)buf_size, fmt, args);
        va_end(args);
        return w;
    }

    // https://stackoverflow.com/a/26221725
    char* format_(const char* fmt, ...) IM_FMTARGS(1);
    inline char* format_(const char* fmt, ...) { 
        va_list args;
        va_start(args, fmt);
        int size_i = own_vsnprintf(NULL, 0, fmt, args);
        va_end(args);

        if (size_i <= 0) {
            IM_ASSERT(0 && "error during string formatting");
            return NULL;
        }

        size_i++; // add size for null term

        char* out = (char*)IMFD_ALLOC(size_i);

        va_start(args, fmt);
        own_vsnprintf(out, size_i, fmt, args);
        va_end(args);

        return out;
    }

    ds::string format(const char* fmt, ...) IM_FMTARGS(1);
    inline ds::string format(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        int size_i = own_vsnprintf(NULL, 0, fmt, args);
        va_end(args);
        if (size_i <= 0) {
            IM_ASSERT(0 && "error during string formatting");
            return NULL;
        }

        size_i++; // add size for null term

        ds::string out;
        out.resize(size_i-1);

        va_start(args, fmt);
        own_vsnprintf(out.data(), size_i, fmt, args);
        va_end(args);

        return imfd_move(out);
    }

    // find value, if not found return (size_t)-1; compare needs to be a function like object with (const T& a, size_t ind_of_b) -> int
    template<typename T,typename CMP>
    inline size_t binarySearchExclusive(size_t len, const T& value, const CMP& compare) IMFD_NOEXCEPT {
        if (len == 0)
            return (size_t)-1;

        size_t from = 0;
        size_t to = len-1;
        while (from <= to) {
            size_t mid = from + (to-from) / 2;

            int cmp = compare(value, mid);

            if (cmp == 0) {
                return mid;
            } else if (cmp < 0) {
                if(mid == 0) goto fail;
                to = mid - 1;
            } else {
                from = mid + 1;
            }
        }

    fail:
        return (size_t)-1;
    }

    // find value, if not found return where to insert it; compare needs to be a function like object with (const T& a, size_t ind_of_b) -> int
    template<typename T,typename CMP>
    inline size_t binarySearchInclusive(size_t len, const T& value, const CMP& compare) IMFD_NOEXCEPT {
        if (len == 0)
            return 0;

        size_t from = 0;
        size_t to = len;
        while (from < to) {
            size_t mid = from + (to-from) / 2;

            int cmp = compare(value, mid);

            if (cmp == 0) {
                return mid;
            } else if (cmp < 0) {
                to = mid;
            } else {
                from = mid + 1;
            }
        }
        return from;
    }

    template<typename T>
    class map {
    private:
        typedef pair<ImGuiID, T> pair_type;
        vector<pair_type> data;

        struct Comparer {
            const map<T>& the_map;
            Comparer(const map<T>& the_map) : the_map(the_map) {}
            int operator()(ImGuiID v, size_t ind) const {
                ImGuiID o = the_map.data[ind].first;
                if (v == o) return 0;
                return v < o ? -1 : 1;
            }
        };

        inline size_t getIndContains(const ImGuiID val) const {
            return binarySearchExclusive(data.size(), val, Comparer(*this));
        }
        inline size_t getIndInsert(const ImGuiID val) const {
            return binarySearchInclusive(data.size(), val, Comparer(*this));
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
            data.insert(data.begin() + ind, pair_type(id,t));
            return data[ind].second;
        }
#if IMFD_USE_MOVE
        inline T& insert(ImGuiID id, T&& t) {
            size_t ind = getIndInsert(id);
            data.insert(data.begin() + ind, pair_type(id,imfd_move(t)));
            return data[ind].second;
        }
#endif

        inline void erase(ImGuiID id) {
            size_t ind = getIndContains(id);
            IM_ASSERT(ind != (size_t)-1);
            data.erase(data.begin() + ind);
        }
        inline ds::pair<ImGuiID,T> erase_get(ImGuiID id) {
            size_t ind = getIndContains(id);
            IM_ASSERT(ind != (size_t)-1);
            ds::pair<ImGuiID,T> t = imfd_move(data[ind]);
            data.erase(data.begin() + ind);
            return t;
        }

        inline bool contains(ImGuiID id) {
            return getIndContains(id) != (size_t)-1;
        }

        inline void clear() {
            data.clear();
        }

        inline pair<ImGuiID, T>* begin() {
            return data.begin();
        }
        inline pair<ImGuiID, T>* end() {
            return data.end();
        }
    };

    template<typename T>
    class set {
    private:
        typedef vector<T> container;
        container data;
        typedef typename container::iterator iterator;

        struct Comparer {
            const set<T>& the_set;
            Comparer(const set<T>& the_set) : the_set(the_set) {}
            int operator()(const T& v, size_t ind) const {
                const T& o = the_set.data[ind];
                if (v == o) return 0;
                return v < o ? -1 : 1;
            }
        };

        inline size_t getIndContains(const T& val) const {
            return binarySearchExclusive(data.size(), val, Comparer(*this));
        }
        inline size_t getIndInsert(const T& val) const {
            return binarySearchInclusive(data.size(), val, Comparer(*this));
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
        vector<size_t> valuesSorted;

        inline void resetInds() {
            valuesSorted.resize(values.size());

            for (size_t i = 0; i < values.size();i++) {
                valuesSorted[i] = i;
            }
        }
    public:
        inline sortarray() {

        }

        inline sortarray(const vector<T>& srcV) : values(srcV) {
            resetInds();
        }

        inline sortarray(const sortarray<T>& src) : values(src.values) {
            resetInds();
        }

        inline sortarray& operator=(const sortarray<T>& src) {
            values = src.values;
            resetInds();
            return *this;
        }

        inline void push_back(const T& t) {
            valuesSorted.push_back(values.size());
            values.push_back(t);
        }

        inline void clear() {
            values.clear();
            valuesSorted.clear();
        }

        inline size_t size() const {
            return values.size();
        }

        inline T& getSorted(size_t i) {
            return values[valuesSorted[i]];
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

        inline size_t getActualIndex(size_t sortInd) const {
            return valuesSorted[sortInd];
        }
    };

    template<typename T>
    class OverrideStack {
    protected:
        ds::vector<T*> arr;
        size_t start;
        size_t currSize;

    public:
        OverrideStack(size_t size_) : arr(size_, NULL), start(0), currSize(0){
            
        }
        OverrideStack(const OverrideStack& o) : arr(o.arr.size(), NULL), start(0), currSize(o.currSize) {
            for(size_t i = 0; i<o.currSize; i++) {
                push(*o.arr[(o.start+i) % o.arr.size()]);
            }
        }
        OverrideStack& operator=(const OverrideStack& o) {
            clear();
            arr.resize(o.arr.size(), NULL);
            for(size_t i = 0; i<o.currSize; i++) {
                push(*o.arr[(o.start+i) % o.arr.size()]);
            }
            IMFD_ASSERT_PARANOID(currSize == o.currSize);
            return *this;
        }
#if IMFD_USE_MOVE
        OverrideStack(OverrideStack&& o) : arr(imfd_move(o.arr)), start(o.start), currSize(o.currSize) {
            o.currSize = 0;
        }
        OverrideStack& operator=(OverrideStack&& o) {
            IM_ASSERT(this != &o);
            clear();
            arr = imfd_move(o.arr);
            start = o.start;
            currSize = o.currSize;
            o.currSize = 0;
            return *this;
        }
#endif
        ~OverrideStack() {
            clear();
        }
        

        void push(const T& t) {
            const size_t nextSlot = (start + currSize) % arr.size();
            if (currSize < arr.size()) {
                currSize++;
            } else {
                start++;
                // about to override this, so dealloc it first
                arr[nextSlot]->~T();
                IMFD_FREE(arr[nextSlot]);
            }
            arr[nextSlot] = (T*)IMFD_ALLOC(sizeof(t));
            IM_PLACEMENT_NEW(arr[nextSlot]) T(t);
        }
        void push(const T* t) {
            const size_t nextSlot = (start + currSize) % arr.size();
            if (currSize < arr.size()) {
                currSize++;
            } else {
                start++;
                // about to override this, so dealloc it first
                arr[nextSlot]->~T();
                IMFD_FREE(arr[nextSlot]);
            }
            arr[nextSlot] = t;
        }
        bool pop(T* out) {
            if (!isEmpty()) {
                currSize--;

                const size_t slot = (start + currSize) % arr.size();
                if (out != NULL) {
                    *out = imfd_move(*arr[slot]);
                }

                arr[slot]->~T();
                IMFD_FREE(arr[slot]);
                arr[slot] = NULL;

                return true;
            }
            else {
                return false;
            }
        }
        bool popPtr(T** out) { // doesnt free
            if (!isEmpty()) {
                currSize--;

                const size_t slot = (start + currSize) % arr.size();
                IM_ASSERT(out != NULL);
                *out = arr[slot];

                arr[slot] = NULL;

                return true;
            }
            else {
                return false;
            }
        }
        // 0 == last pushed entry
        T*& at(size_t ind) {
            int stackInd = (start + arr.size() - 1 - ind) % arr.size();
            return arr[stackInd];
        }
        void clear() {
            for (size_t i = 0; i < currSize; i++) {
                size_t ind = (start+i) % arr.size();
                IM_ASSERT_PARANOID(arr[ind] != NULL)
                arr[ind]->~T();
                IMFD_FREE(arr[ind]);
                arr[ind] = NULL;
            }

            start = 0;
            currSize = 0;
        }

        bool isEmpty() const{
            return currSize == 0;
        }
        size_t size() const{
            return currSize;
        }

        void resize(size_t newSize) {
            if (newSize == arr.size())
                return;

            ds::vector<T*> newVec(newSize, NULL);

            if (newSize > arr.size()) {
                for (size_t i = 0; i < currSize; i++) {
                    newVec[i].push(arr[(start+i) % arr.size()]);
                }
            }
            else {  // newSize < len
                for (size_t i = 0; i < newSize; i++) {
                    newVec[i].push(arr[(start+i) % arr.size()]);
                }
                for (size_t i = newSize; i < arr.size(); i++) {
                    arr[i]->~T();
                    IMFD_FREE(arr[i]);
                }
            }

            arr = imfd_move(newVec);
            start = 0;
        }
    };

    template<typename T>
    struct _ResultOk {
        T t;
#if IMFD_USE_MOVE
        template<typename U>
        explicit _ResultOk(U&& v) : t(ds::forward<U>(v)) {}
#else
        explicit _ResultOk(T t) : t(t) {}
#endif
    };
    template<typename T>
    struct _ResultErr {
        T t;
#if IMFD_USE_MOVE
        template<typename U>
        explicit _ResultErr(U&& v) : t(ds::forward<U>(v)) {}
#else
        explicit _ResultErr(T t) : t(t) {}
#endif
    };

    struct None {};
    
    template<typename OkT, typename ErrT>
    class Result {
    public:
        bool has_value() const IMFD_NOEXCEPT {
            return state == State_Ok;
        }
        bool has_err() const IMFD_NOEXCEPT {
            return state == State_Err;
        }

        OkT& value() IMFD_NOEXCEPT {
            IM_ASSERT(state == State_Ok);
            return *reinterpret_cast<OkT*>(data);
        }
        const OkT& value() const IMFD_NOEXCEPT {
            IM_ASSERT(state == State_Ok);
            return *reinterpret_cast<const OkT*>(data);
        }


        ErrT& error() IMFD_NOEXCEPT {
            IM_ASSERT(state == State_Err);
            return *reinterpret_cast<ErrT*>(data);
        }
        const ErrT& error() const IMFD_NOEXCEPT {
            IM_ASSERT(state == State_Err);
            return *reinterpret_cast<const ErrT*>(data);
        }
        _ResultErr<ErrT> error_prop() IMFD_NOEXCEPT {
            IM_ASSERT(state == State_Err);
#if IMFD_USE_MOVE
            return _ResultErr<ErrT>(imfd_move(error()));
#else
#ifdef IMGUIFD_ENABLE_STL
            using std::swap;
#endif
            // this should enable NRVO?
            ErrT err_;
            swap(error(), err_);
            return _ResultErr<ErrT>(err_);
#endif
        }

#if IMFD_USE_MOVE
        template<typename OkT_>
        Result(_ResultOk<OkT_> ok_) : state(State_Ok) {
            IM_PLACEMENT_NEW(data) OkT(imfd_move(ok_.t));
        }
        template<typename ErrT_>
        Result(_ResultErr<ErrT_> err_) : state(State_Err) {
            IM_PLACEMENT_NEW(data) ErrT(imfd_move(err_.t));
        }
#else
        template<typename OkT_>
        Result(_ResultOk<OkT_> ok_) : state(State_Ok) {
            IM_PLACEMENT_NEW(data) OkT(ok_.t);
        }
        template<typename ErrT_>
        Result(_ResultErr<ErrT_> err_) : state(State_Err) {
            IM_PLACEMENT_NEW(data) ErrT(err_.t);
        }
#endif

        Result(const Result& o) : state(o.state)  {
            if(state == State_Ok) {
                IM_PLACEMENT_NEW(data) OkT(o.value());
            } else if(state == State_Err) {
                IM_PLACEMENT_NEW(data) ErrT(o.error());
            } else {
                IM_ASSERT(0);
            }
        }
        Result& operator=(const Result& o) {
            IM_ASSERT(this != &o);

            clear();
            state = o.state;
            if(state == State_Ok) {
                IM_PLACEMENT_NEW(data) OkT(o.value());
            } else if(state == State_Err) {
                IM_PLACEMENT_NEW(data) ErrT(o.error());
            } else {
                IM_ASSERT(0);
            }
            return *this;
        }

        ~Result() {
            clear();
        }
    private:
        enum State {
            State_Ok,
            State_Err
        };
        State state;
        union {
            char data[sizeof(OkT) > sizeof(ErrT) ? sizeof(OkT) : sizeof(ErrT)];
            uint64_t _alignment_dummy;
        };
    
        void clear() {
            if(state == State_Ok) {
                reinterpret_cast<OkT*>(data)->~OkT();
            } else if(state == State_Err) {
                reinterpret_cast<ErrT*>(data)->~ErrT();
            } else {
                IM_ASSERT(0);
            }
        }
    };

    inline _ResultOk<None> Ok() {
        return _ResultOk<None>(None());
    }
#if IMFD_USE_MOVE
    template <typename T>
    _ResultOk<typename remove_cvref<T>::type> Ok(T&& t) {
        return _ResultOk<typename remove_cvref<T>::type>(ds::forward<T>(t));
    }
    template <typename T>
    _ResultErr<typename remove_cvref<T>::type> Err(T&& t) {
        return _ResultErr<typename remove_cvref<T>::type>(ds::forward<T>(t));
    }
#else
    template <typename T>
    _ResultOk<T> Ok(T value) {
        return _ResultOk<T>(value);
    }
    template <typename T>
    _ResultErr<T> Err(T error) {
        return _ResultErr<T>(error);
    }
#endif

}

#define IMGUIFD_RETURN_IF_ERR(expr) \
    do { \
        auto e = (expr); \
        if(e.has_err()) \
            return e.error_prop(); \
    } while(0)

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
    };

    namespace Native {
        bool isAbsolutePath(const char* path);
        ds::Result<ds::string, ds::string> getAbsolutePath(const char* path);
        bool isValidDir(const char* dir);
        bool fileExists(const char* path);

        // the DirEntrys will have their dir member be set to the here given dir pointer (no string copy)
        ds::Result<ds::vector<DirEntry>, ds::string> loadDirEntrys(const char* dir);

        ds::Result<ds::None, ds::string> rename(const char* name, const char* newName);
        ds::Result<ds::None, ds::string> makeFolder(const char* path);

        void Shutdown();
    }
}

#endif
