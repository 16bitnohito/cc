#include <algorithm>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>
#include <preprocessor/preprocessor.h>
#include <preprocessor/utility.h>

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

		Preprocessor pp(opts);
		return pp.run();
	} catch (const system_error& e) {
#if HOST_PLATFORM == PLATFORM_WINDOWS
		// 挙動が異なると分かっているものだけ特別扱い。
		auto message = lib::win32util::mbs_to_u8s(e.what());
		cerr << __func__ << ": " << lib::win32util::as_native(message.c_str()) << "(" << e.code() << ")" << endl;
#else
		cerr << __func__ << ": " << e.what() << "(" << e.code() << ")" << endl;
#endif
	} catch (const exception& e) {
		cerr << __func__ << ": " << e.what() << endl;
	} catch (...) {
		cerr << __func__ << ": unknown exception" << endl;
	}

	return EXIT_FAILURE;
}
