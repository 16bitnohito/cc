#include <algorithm>
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
		vector<string> args;
		transform(&argv[0], &argv[argc], back_inserter(args), internal_string);

		setup_console();
		init_calculator();
		init_preprocessor();

		Preprocessor pp;
		if (!pp.parse_options(args)) {
			pp.print_usage();
			return 1;
		}

		return pp.run();
	} catch (const system_error& e) {
		cerr << __func__ << ": " << e.what() << "(" << e.code() << ")" << endl;
	} catch (const exception& e) {
		cerr << __func__ << ": " << e.what() << endl;
	} catch (...) {
		cerr << __func__ << ": unknown exception" << endl;
	}

	return EXIT_FAILURE;
}
