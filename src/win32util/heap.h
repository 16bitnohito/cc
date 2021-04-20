#ifndef CC_WIN32UTIL_HEAP_H_
#define CC_WIN32UTIL_HEAP_H_

#include <utility>
#include <Windows.h>

namespace lib::win32util {

/**
 * HLOCAL
 */
class LocalHandle {
public:
    LocalHandle() noexcept
        : handle_(nullptr) {
    }

    explicit LocalHandle(HLOCAL handle) noexcept
        : handle_(handle) {
    }

    LocalHandle(const LocalHandle&) = delete;

    LocalHandle(LocalHandle&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)) {
    }

    ~LocalHandle() {
        try {
            close();
        } catch (...) {
            // IGNORE
        }
    }

    LocalHandle& operator=(const LocalHandle&) = delete;

    LocalHandle& operator=(LocalHandle&& rhs) & noexcept {
        if (this != &rhs) {
            LocalHandle temp = std::move(rhs);
            temp.swap(*this);
        }
        return *this;
    }

    LocalHandle& operator=(LocalHandle&&) && = delete;

    explicit operator bool() const noexcept {
        return handle() != nullptr;
    }

    void swap(LocalHandle& other) noexcept {
        std::swap(handle_, other.handle_);
    }

    [[nodiscard]]
    HLOCAL handle() const noexcept {
        return handle_;
    }

    [[nodiscard]]
    UINT flags() const;

    void close();

    [[nodiscard]]
    void* lock();

    void unlock();

    void realloc(SIZE_T new_size, UINT flags);

private:
    HLOCAL handle_;
};

/**
 */
inline
void swap(LocalHandle& x, LocalHandle& y) noexcept {
    x.swap(y);
}

inline
bool operator==(const LocalHandle& lhs, const LocalHandle& rhs) noexcept {
    return lhs.handle() == rhs.handle();
}

inline
bool operator!=(const LocalHandle& lhs, const LocalHandle& rhs) noexcept {
    return !(lhs == rhs);
}

}   // namespace lib::win32util

#endif  // CC_WIN32UTIL_HEAP_H_
