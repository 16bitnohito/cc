#if !defined(PREPROCESSOR_UTILITY_HPP_)
#define PREPROCESSOR_UTILITY_HPP_

#include <algorithm>
#include <array>
#include <string>
#include <type_traits>
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
std::string& to_upper_string(std::string& s, std::size_t pos = 0);

void last_error();

template <class T>
std::size_t size_in_bytes(const std::vector<T>& container) {
	return container.size() * sizeof(std::vector<T>::value_type);
}

bool file_exists(const Path& path);

struct Sorter {
	template <class Container>
	Sorter(Container& c) {
		using std::sort;
		sort(c.begin(), c.end());
	}

	template <class Container, class Pred>
	Sorter(Container& c, Pred pred) {
		using std::sort;
		sort(c.begin(), c.end(), pred);
	}
};

template <class Iter, class T, class Pred = std::less<T>>
Iter binary_find(Iter first, Iter last, const T& x, Pred pred = Pred()) {
	Iter it = std::lower_bound(first, last, x, pred);
	return ((it != last) && !pred(x, *it)) ? it : last;
}

/**
 */
constexpr
auto enum_ordinal(auto value) {
	return static_cast<std::underlying_type_t<decltype(value)>>(value);
}

/**
 */
void setup_console();

/**
 * @param error std::errcで定義されているエラーに対応するエラー値、errno、或いは EINVAL等を渡します。
 */
[[noreturn]]
void raise_generic_error(const char* message, int error);

}	//  namespace pp

#endif	//  PREPROCESSOR_UTILITY_HPP_
