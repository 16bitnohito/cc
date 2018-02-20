#if !defined(PREPROCESSOR_UTILITY_HPP_)
#define PREPROCESSOR_UTILITY_HPP_

#include <array>
#include <string>
#include <vector>
#include <preprocessor/config.hpp>


namespace pp {

std::string get_current_dir();

#if HOST_PLATFORM == PLATFORM_WINDOWS
std::string to_cp932(const std::wstring& ws);
std::string to_u8string(const std::wstring& ws);
std::wstring to_wstring(const std::string& u8s);
#endif

#if HOST_PLATFORM == PLATFORM_WINDOWS
using Path = std::wstring;

inline
std::string internal_string(const std::wstring& s) {
	return to_u8string(s);
}

inline
Path path_string(const std::string& s) {
	return to_wstring(s);
}
#else
using Path = std::string;

inline
std::string internal_string(const std::string& s) {
	return s;
}

inline
Path path_string(const std::string& s) {
	return s;
}
#endif

std::string trim_string(const std::string& s);
std::string& to_upper_string(std::string& s, size_t pos = 0);

void last_error();

template <class T>
size_t size_in_bytes(const std::vector<T>& container) {
	return container.size() * sizeof(std::vector<T>::value_type);
}

bool file_exists(const Path& path);

struct Sorter {
	template <class Container>
	Sorter(Container& c) {
		sort(c.begin(), c.end());
	}

	template <class Container, class Pred>
	Sorter(Container& c, Pred pred) {
		sort(c.begin(), c.end(), pred);
	}
};

template <class Iter, class T, class Pred = std::less<T>>
Iter binary_find(Iter first, Iter last, const T& x, Pred pred = Pred()) {
	Iter it = std::lower_bound(first, last, x, pred);
	return ((it != last) && !pred(x, *it)) ? it : last;
}

}	//  namespace pp

#endif	//  PREPROCESSOR_UTILITY_HPP_
