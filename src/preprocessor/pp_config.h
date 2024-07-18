#ifndef CC_PREPROCESSOR_CPP_CONFIG_H_
#define CC_PREPROCESSOR_CPP_CONFIG_H_

#include "config.h"
#include "util/utility.h"
#include "util/integer.h"

namespace pp {

#include "util/using_integer.inl"

using Char = lib::util::Char;
using String = lib::util::String;
using StringView = lib::util::StringView;

}   // namespace pp

#endif  // CC_PREPROCESSOR_PP_CONFIG_H_
