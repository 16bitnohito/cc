#ifndef CC_PREPROCESSOR_CPP_CONFIG_H_
#define CC_PREPROCESSOR_CPP_CONFIG_H_

#include "config.h"
#include "util/utility.h"

namespace pp {

using target_intmax_t = cc::target_intmax_t;
using target_uintmax_t = cc::target_uintmax_t;

using String = lib::util::String;
using StringView = lib::util::StringView;

}   // namespace pp

#endif  // CC_PREPROCESSOR_PP_CONFIG_H_
