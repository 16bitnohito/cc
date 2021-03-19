#include <preprocessor/utility.hpp>

#include <cassert>
#include <system_error>
#if HOST_PLATFORM == PLATFORM_WINDOWS
#include <Windows.h>
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

using namespace std;


namespace pp {

std::string get_current_dir() {
	string result;

#if HOST_PLATFORM == PLATFORM_WINDOWS
	wstring temp;
	DWORD needs = GetCurrentDirectory(0, nullptr);
	if (needs == 0) {
		last_error();
	}
	temp.resize(needs - 1);
	DWORD written = GetCurrentDirectory(needs, &temp[0]);
	if (written == 0) {
		last_error();
	}

	result = to_u8string(temp);
#else
	size_t size = 256;
	char* p = nullptr;
	do {
		size *= 2;
		result.resize(size);
		p = getcwd(&result[0], size);
	} while (p == nullptr && errno == ERANGE);
#endif

	return result;
}

#if HOST_PLATFORM == PLATFORM_WINDOWS
std::string to_cp932(const std::wstring& ws) {
	const DWORD flags = WC_NO_BEST_FIT_CHARS;
	int needs = WideCharToMultiByte(
		CP_ACP,
		flags,
		ws.c_str(), static_cast<int>(ws.length()),
		nullptr, 0,
		nullptr,
		nullptr);
	if (needs == 0) {
		last_error();
	}
	string result;
	result.resize(needs);
	int converted = WideCharToMultiByte(
		CP_ACP,
		flags,
		ws.c_str(), static_cast<int>(ws.length()),
		&result[0], static_cast<int>(result.length()),
		nullptr,
		nullptr);
	if (converted == 0) {
		last_error();
	}
	assert(needs > 0 && strlen(result.c_str()) == static_cast<size_t>(needs));

	return result;
}

std::string to_u8string(const std::wstring& ws) {
	//  元のワイド文字列の長さとして、終端文字までを示す -1を渡さずに、length()を
	//  渡しているので、needsには終端文字の数が含まれないことに注意。
	const DWORD flags = 0;
	int needs = WideCharToMultiByte(
			CP_UTF8,
			flags,
			ws.c_str(), static_cast<int>(ws.length()),
			nullptr, 0,
			nullptr,
			nullptr);
	if (needs == 0) {
		last_error();
	}
	string result;
	result.resize(needs);
	int converted = WideCharToMultiByte(
			CP_UTF8,
			flags,
			ws.c_str(), static_cast<int>(ws.length()),
			&result[0], static_cast<int>(result.length()),
			nullptr,
			nullptr);
	if (converted == 0) {
		last_error();
	}
	assert(needs > 0 && strlen(result.c_str()) == static_cast<size_t>(needs));

	return result;
}

std::wstring to_wstring(const std::string& u8s) {
	//  第 4引数について to_u8stringに同じ。
	int needs = MultiByteToWideChar(
			CP_UTF8,
			0,
			u8s.c_str(), static_cast<int>(u8s.length()),
			nullptr, 0);
	if (needs == 0) {
		last_error();
	}
	wstring result;
	result.resize(needs);
	int converted = MultiByteToWideChar(
			CP_UTF8,
			0,
			u8s.c_str(), static_cast<int>(u8s.length()),
			&result[0], static_cast<int>(result.length()));
	if (converted == 0) {
		last_error();
	}
	assert(needs > 0 && wcslen(result.c_str()) == static_cast<size_t>(needs));

	return result;
}
#endif

std::string trim_string(const std::string& s) {
	const std::string blank = "\x09\x0b\x0c\x20";

	string result;
	string::size_type l = s.find_first_not_of(blank);
	if (l != string::npos) {
		string::size_type r = s.find_last_not_of(blank);
		result = s.substr(l, r - l + 1);
	}

	return result;
}

std::string& to_upper_string(std::string& s, size_t pos) {
	for (size_t i = pos; i < s.length(); i++) {
		s[i] = (toupper(s[i]) & 0xff);
	}
	return s;
}

#if HOST_PLATFORM == PLATFORM_WINDOWS
void last_error() {
	DWORD error = GetLastError();
	LPSTR message;
	DWORD result = FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
				FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr,
			error,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			reinterpret_cast<LPSTR>(&message),
			0,
			nullptr);
	if (result == 0) {
		return ;
	}

	LocalFree(message);
}
#else
void last_error() {
	//  適当
	perror("");
}
#endif

bool file_exists(const Path& path) {
#if HOST_PLATFORM == PLATFORM_WINDOWS
	struct _stat64 s;
	int e = _wstat64(path.c_str(), &s);

	return (e == 0);
#else
	struct stat s;
	int e = stat(path.c_str(), &s);

	return (e == 0);
#endif
}

void setup_console() {
#if HOST_PLATFORM == PLATFORM_WINDOWS
	if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
		raise_generic_error("_setmode(stdin)", errno);
	}
	if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
		raise_generic_error("_setmode(stdout)", errno);
	}
	if (_setmode(_fileno(stderr), _O_TEXT) == -1) {
		raise_generic_error("_setmode(stderr)", errno);
	}
#else
#endif
}

[[noreturn]]
void raise_generic_error(const char* message, int error) {
	std::error_code ec(error, generic_category());
	throw std::system_error(ec, message);
}

}   //  namespace pp
