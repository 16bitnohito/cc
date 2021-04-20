#include <win32util/heap.h>

#include <win32util/error.h>

namespace lib::win32util {

[[nodiscard]]
UINT LocalHandle::flags() const {
    auto result = LocalFlags(handle());
    if (result == LMEM_INVALID_HANDLE) {
        raise_win32_error("LocalFlags");
    }
    return result;
}

void LocalHandle::close() {
    if (handle_ != nullptr) {
        HLOCAL result = LocalFree(handle_);
        handle_ = nullptr;

        if (result != nullptr) {
            raise_win32_error("LocalFree");
        }
    }
}

void* LocalHandle::lock() {
    auto result = LocalLock(handle());
    if (!result) {
        raise_win32_error("LocalLock");
    }
    return result;
}

void LocalHandle::unlock() {
    auto count = LocalUnlock(handle());
    if (count) {
        return;
    }

    auto error = GetLastError();
    if (error != NO_ERROR) {
        raise_win32_error("LocalUnlock", error);
    }
}

void LocalHandle::realloc(SIZE_T new_size, UINT flags) {
    auto new_handle = LocalReAlloc(handle_, new_size, flags);
    if (!new_handle) {
        raise_win32_error("LocalReAlloc");
    }
    handle_ = new_handle;
}

}   // namespace lib::win32util
