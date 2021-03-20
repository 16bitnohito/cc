#include <preprocessor/options.h>

using namespace std;

namespace {

constexpr char kUnknownOptionError[] = "不明なオプション %cが指定された。\n";
constexpr char kNoOptionParameterError[] = "オプション %cの値が指定されていない。\n";

}   // anonymous namespace

namespace pp {

Options::Options() {
	input_encoding_ = "utf-8";	//  今は気分的なもの。
	//input_filepath_;
	output_encoding_ = "utf-8";	//  今は気分的なもの。
	//output_filepath_;
	output_line_directive_ = true;
	output_comment_ = false;
	support_trigraphs_ = false;
	//additional_include_dirs_;
}

Options::~Options() {
}

const std::string& Options::input_filepath() const {
	return input_filepath_;
}

const std::string& Options::output_filepath() const {
	return output_filepath_;
}

const std::string& Options::error_log_filepath() const {
	return error_log_filepath_;
}

bool Options::output_line_directive() const {
	return output_line_directive_;
}

bool Options::output_comment() const {
	return output_comment_;
}

bool Options::support_trigraphs() const {
	return support_trigraphs_;
}

const std::vector<std::string>& Options::additional_include_dirs() const {
	return additional_include_dirs_;
}

const std::vector<std::string>& Options::macro_defs() const {
	return macro_defs_;
}

const std::vector<std::string>& Options::macro_undefs() const {
	return macro_undefs_;
}

bool Options::parse_options(const std::vector<std::string>& args) {
	int argc = static_cast<int>(args.size());

	for (int i = 1; i < argc; i++) {
		const string& arg = args[i];

		if (arg[0] == '-') {
			switch (arg[1]) {
			case '\0':
				if (input_filepath_.empty()) {
					input_filepath_ = arg;
				}
				break;
			case 'I': {
				string path;
				if (arg[2] != '\0') {
					path = &arg[2];
				} else {
					if ((i + 1) < argc) {
						path = args[i + 1];
						i++;
					} else {
						fprintf(stderr, kNoOptionParameterError, arg[1]);
						return false;
					}
				}
				additional_include_dirs_.push_back(path);
				break;
			}
			case 'D': {
				string macro;
				if (arg[2] != '\0') {
					macro = &arg[2];
				} else {
					if ((i + 1) < argc) {
						macro = args[i + 1];
						i++;
					} else {
						fprintf(stderr, kNoOptionParameterError, arg[1]);
						return false;
					}
				}
				macro_defs_.push_back(macro);
				break;
			}
			case 'U': {
				string name;
				if (arg[2] != '\0') {
					name = &arg[2];
				} else {
					if ((i + 1) < argc) {
						name = args[i + 1];
						i++;
					} else {
						fprintf(stderr, kNoOptionParameterError, arg[1]);
						return false;
					}
				}
				macro_undefs_.push_back(name);
				break;
			}
			//  TODO: 将来的には。
			//case 'P': {
			//	output_line_directive_ = false;
			//	break;
			//}
			//case 'C': {
			//	output_comment_ = true;
			//	break;
			//}
			case 'o': {
				string path;
				if (arg[2] != '\0') {
					path = &arg[2];
				} else {
					if ((i + 1) < argc) {
						path = args[i + 1];
						i++;
					} else {
						fprintf(stderr, kNoOptionParameterError, arg[1]);
						return false;
					}
				}
				if (output_filepath_.empty()) {
					output_filepath_ = path;
				}
				break;
			}
			case 'e': {
				string path;
				if (arg[2] != '\0') {
					path = &arg[2];
				} else {
					if ((i + 1) < argc) {
						path = args[i + 1];
						i++;
					} else {
						fprintf(stderr, kNoOptionParameterError, arg[1]);
						return false;
					}
				}
				error_log_filepath_ = path;
				break;
			}
			case 't':
				if (arg == "-trigraphs") {
					support_trigraphs_ = true;
				}
				break;
			case 'h':
				return false;

			default:
				fprintf(stderr, kUnknownOptionError, arg[1]);
				return false;
			}
		} else {
			if (input_filepath_.empty()) {
				input_filepath_ = arg;
			}
		}
	}

	return true;
}

void Options::print_usage() {
	puts("使い方: cpp [options] input");
	puts("Options:\n");
	puts("-D <name>[=definition]\tマクロを定義する。");
	puts("-D <name(params)>[=definitiion]\tマクロを定義する。");
	puts("-U <name>\tマクロを削除する。");
	puts("-I <directory>\t\tインクルード検索パスを追加する。");
	puts("-o <file>\t出力先を指定する。(デフォルトは標準出力)");
	puts("-e <file>\tエラー出力先を指定する。(デフォルトは標準エラー出力)");
	puts("-h\t\tヘルプを出力する。");
}

}   // namespace pp
