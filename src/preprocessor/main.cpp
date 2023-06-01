#include <algorithm>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include "util/logger.h"
#include "util/utility.h"
#include "preprocessor/preprocessor.h"

using namespace lib::util;
using namespace pp;
using namespace std;

#if HOST_PLATFORM == PLATFORM_WINDOWS
int wmain(int argc, wchar_t* argv[]) {
#else
int main(int argc, char* argv[]) {
#endif
    try {
        vector<String> args;
        args.reserve(argc);
        for (int i = 0; i < argc; ++i) {
            args.push_back(internal_string(argv[i]));
        }

        setup_console();
        init_calculator();
        init_preprocessor();

        Options opts;
        if (!opts.parse_options(args)) {
            opts.print_usage();
            return EXIT_FAILURE;
        }

        Diagnostics diag;
        SourceFileStack sources;
        Preprocessor pp(opts, diag, sources);
        return pp.run();
    } catch (const system_error& e) {
        auto& ec = e.code();
#if HOST_PLATFORM == PLATFORM_WINDOWS
        if (ec.category() == system_category() ||
            ec.category() == lib::win32util::win32_category()) {
            // 挙動が異なると分かっているものだけ特別扱い。
            auto message = lib::win32util::mbs_to_u8s(e.what());
            log_error(T_("{}: {}({}:{:#x})"),
                    __func__, lib::win32util::as_native(message.c_str()),
                    ec.category().name(), ec.value());
        } else {
            log_error("{}: {}({}:{})", __func__, e.what(), ec.category().name(), ec.value());
        }
#else
        log_error("{}: {}({}{:x})", __func__, e.what(), ec.category().name(), ec.value());
#endif
    } catch (const exception& e) {
        log_error(T_("{}: {}"), __func__, e.what());
    } catch (...) {
        log_error(T_("{}: {}"), __func__, "unknown exception");
    }

    return EXIT_FAILURE;
}
