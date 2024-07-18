#ifndef CC_UTIL_INTEGER_H_
#define CC_UTIL_INTEGER_H_

#include <cstdint>
#include <cinttypes>

namespace lib::util {

using target_int = std::int16_t;
using target_long = std::int32_t;
using target_longlong = std::int32_t;
//using target_longlong = std::int64_t;
using target_uint = std::uint16_t;
using target_ulong = std::uint32_t;
using target_ulonglong = std::uint32_t;
//using target_ulonglong = std::uint64_t;

using target_intmax_t = target_longlong;
using target_uintmax_t = target_ulonglong;

#define CC_TARGET_INT_MIN           INT16_C(-32768)
#define CC_TARGET_LONG_MIN          (INT32_C(-2147483647) - 1)
#define CC_TARGET_LLONG_MIN         (INT32_C(-2147483647) - 1)
//#define CC_TARGET_LLONG_MIN         (INT64_C(-9223372036854775807) - 1)
#define CC_TARGET_INT_MAX           INT16_C(32767)
#define CC_TARGET_LONG_MAX          INT32_C(2147483647)
#define CC_TARGET_LLONG_MAX         INT32_C(2147483647)
//#define CC_TARGET_LLONG_MAX         INT64_C(9223372036854775807)

#define CC_TARGET_UINT_MAX          UINT16_C(0xffff)
#define CC_TARGET_ULONG_MAX         UINT32_C(0xffffffff)
#define CC_TARGET_ULLONG_MAX        UINT32_C(0xffffffff)
//#define CC_TARGET_ULLONG_MAX        UINT64_C(0xffffffffffffffff)

#define CC_TARGET_BOOL_WIDTH         1
#define CC_TARGET_CHAR_BIT           8
#define CC_TARGET_USHRT_WIDTH       16
#define CC_TARGET_INT_WIDTH         16
#define CC_TARGET_UINT_WIDTH        16
#define CC_TARGET_LONG_WIDTH        32
#define CC_TARGET_ULONG_WIDTH       32
#define CC_TARGET_LLONG_WIDTH       32
#define CC_TARGET_ULLONG_WIDTH      32
//#define CC_TARGET_LLONG_WIDTH       64
//#define CC_TARGET_ULLONG_WIDTH      64
#define CC_TARGET_BITINT_MAXWIDTH   CC_TARGET_ULLONG_WIDTH

#define CC_TARGET_INTMAX_WIDTH      CC_TARGET_LLONG_WIDTH
#define CC_TARGET_UINTMAX_WIDTH     CC_TARGET_ULLONG_WIDTH

}   // namespace lib::util

#endif  // CC_UTIL_INTEGER_H_
