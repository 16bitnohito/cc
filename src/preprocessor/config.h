#ifndef PREPROCESSOR_CONFIG_H_
#define PREPROCESSOR_CONFIG_H_

// HOST
#define PLATFORM_UNKNOWN    0
#define PLATFORM_WINDOWS    1

#if defined(_WIN32)
#define HOST_PLATFORM   PLATFORM_WINDOWS
#else
#define HOST_PLATFORM   PLATFORM_UNKNOWN
#endif

namespace pp {

// TARGET
using target_intmax_t = std::int32_t;
using target_uintmax_t = std::uint32_t;

// BUILD
//#if USE_STD_FILESYSTEM
//using Path = std::filesystem::path;
//#else
//using Path = std::wstring;
//#endif

}   // namespace pp

#endif  // PREPROCESSOR_CONFIG_H_
