#ifndef CC_PREPROCESSOR_CALCULATOR_H_
#define CC_PREPROCESSOR_CALCULATOR_H_

#include <cstdint>
#include <string>

#include "pp_config.h"

namespace pp {

enum class OperatorId {
    kUnknown,
    kCloseParen,
    kOpenParen,
    //kComma,
    kColon,
    kCond,
    kOr,
    kAnd,
    kBitOr,
    kBitXor,
    kBitAnd,
    kEq,
    kNeq,
    kLt,
    kGt,
    kLeq,
    kGeq,
    kShl,
    kShr,
    kAdd,
    kSub,
    kMul,
    kDiv,
    kMod,
    kPlus,
    kMinus,
    kCompl,
    kNot,
};

const char* operator_id_to_string(OperatorId id);

struct Operator {
    OperatorId id;
    std::string sequence;
    std::uint8_t precedence;
    std::uint8_t arity;
};

void init_calculator();

/**
 * 整数定数を解析した結果の型。
 */
class IntegerType {
public:
    constexpr static int kSignedBitIntMinWidth = 2;
    constexpr static int kUnsignedBitIntMinWidth = 1;

    enum Value : uint32_t {
        kIntBase = 0x00000001U,
        kLongBase = 0x00000002U,
        kLongLongBase = 0x00000003U,
        kBitIntBase = 0x00000004U,
        kBaseMask = 0x000000ffU,

        kSigned = 0x00000000U,
        kUnsigned = 0x00000100U,
        kSignMask = 0x0000ff00U,

        kWidthMask = 0x00ff0000U,
        kWidthShift = 16U,

        kNoType = 0,
        kInt = kIntBase | kSigned | (CC_TARGET_INT_WIDTH << kWidthShift),
        kLong = kLongBase | kSigned | (CC_TARGET_LONG_WIDTH << kWidthShift),
        kLongLong = kLongLongBase | kSigned | (CC_TARGET_LLONG_WIDTH << kWidthShift),
        kBitInt = kBitIntBase | kSigned,
        kUInt = kIntBase | kUnsigned | (CC_TARGET_UINT_WIDTH << kWidthShift),
        kULong = kLongBase | kUnsigned | (CC_TARGET_ULONG_WIDTH << kWidthShift),
        kULongLong = kLongLongBase | kUnsigned | (CC_TARGET_ULLONG_WIDTH << kWidthShift),
        kUBitInt = kBitIntBase | kUnsigned,

        kIntMaxT = kLongLong,
        kUIntMaxT = kULongLong,
    };

    constexpr static IntegerType bitint(int width) {
        return static_cast<Value>(kBitInt | (width << kWidthShift));
    }

    constexpr static IntegerType ubitint(int width) {
        return static_cast<Value>(kUBitInt | (width << kWidthShift));
    }

    constexpr static IntegerType make_unsigned(IntegerType type) {
        return static_cast<Value>(type.value_ | kUnsigned);
    }

    constexpr static bool is_valid_bitint_width(int width) {
        return (kSignedBitIntMinWidth <= width && width <= CC_TARGET_BITINT_MAXWIDTH);
    }

    constexpr static bool is_valid_ubitint_width(int width) {
        return (kUnsignedBitIntMinWidth <= width && width <= CC_TARGET_BITINT_MAXWIDTH);
    }

    constexpr IntegerType() : value_() {}
    constexpr IntegerType(Value value) : value_(value) {}

    constexpr bool operator==(const IntegerType& rhs) const {
        return value() == rhs.value();
    }

    constexpr bool operator!=(const IntegerType& rhs) const {
        return value() != rhs.value();
    }

    constexpr Value base() const {
        return static_cast<Value>(value() & kBaseMask);
    }

    constexpr Value sign() const {
        return static_cast<Value>(value() & kSignMask);
    }

    constexpr Value type() const {
        return static_cast<Value>(value() & (kBaseMask | kSignMask));
    }

    constexpr Value value() const {
        return value_;
    }

    constexpr int width() const {
        return static_cast<int>((value() & kWidthMask) >> kWidthShift);
    }

    constexpr bool is_signed() const {
        return sign() == kSigned;
    }

    constexpr bool is_unsigned() const {
        return sign() == kUnsigned;
    }

    constexpr bool is_bitint() const {
        return base() == kBitIntBase;
    }

    std::string to_string() const;

private:
    Value value_;
};

/**
 */
struct Integer {
    IntegerType type;
    target_uintmax_t value;
};

/**
 */
std::errc parse_int(const std::string& s, Integer& result);

}   // namespace pp

#endif  // CC_PREPROCESSOR_CALCULATOR_H_
