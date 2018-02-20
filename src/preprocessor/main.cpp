#include <algorithm>
#include <string>
#include <vector>
#include <preprocessor/preprocessor.hpp>
#include <preprocessor/utility.hpp>

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

		Preprocessor pp;
		if (!pp.parse_options(args)) {
			pp.print_usage();
			return 1;
		}

		return pp.run();
	}
	catch (std::exception& e) {
		cerr << "main:" << e.what() << endl;
		return 1;
	}
	catch (...) {
		cerr << "main:unknown exception" << endl;
		return 1;
	}
}
