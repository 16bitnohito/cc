#ifndef CC_WIN32UTIL_ERROR_H_
#define CC_WIN32UTIL_ERROR_H_

#include <system_error>
#include <Windows.h>

namespace lib::win32util {

/**
 * Win32エラーを std::system_error例外として送出します。
 * @param code GetLastErrorの戻り値、又は、HRESULT値を渡します。
 */
[[noreturn]]
void raise_win32_error(const char* message, DWORD code = GetLastError());

/**
 * raise_system_errorから送出される std::system_errorのカテゴリーです。
 */
const std::error_category& win32_category();

}   // namespace lib::win32util

#endif  // CC_WIN32UTIL_ERROR_H_
