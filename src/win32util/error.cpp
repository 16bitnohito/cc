#include <win32util/error.h>

#include <win32util/strings.h>

using namespace std;

namespace {

class Win32ErrorCategory : public std::error_category {
public:
    Win32ErrorCategory() noexcept = default;
    virtual ~Win32ErrorCategory() = default;

    [[nodiscard]] virtual const char* name() const noexcept override;
    [[nodiscard]] virtual std::string message(int _Errval) const override;
};

const char* Win32ErrorCategory::name() const noexcept {
    return "win32";
}

std::string Win32ErrorCategory::message(int code) const {
    auto s = lib::win32util::last_error_string(code);
    return lib::win32util::win32s_to_mbs({s.c_str(), s.length()});
}

}   // anonymous namespace

namespace lib::win32util {

void raise_win32_error(const char* message, DWORD code /*= GetLastError()*/) {
    error_code ec(code, win32_category());
    throw system_error(ec, message);
}

const std::error_category& win32_category() {
    static Win32ErrorCategory cat;
    return cat;
}

}
