#include "preprocessor/options.h"

#include "util/logger.h"

using namespace lib::util;
using namespace std;

namespace {

const pp::StringView kUnknownOptionError = T_("不明なオプション {}が指定された。\n");
const pp::StringView kNoOptionParameterError = T_("オプション {}の値が指定されていない。\n");

}   // anonymous namespace

namespace pp {

Options::Options() {
    input_encoding_ = T_("utf-8");  // 今は気分的なもの。
    //input_filepath_;
    output_encoding_ = T_("utf-8"); // 今は気分的なもの。
    //output_filepath_;
    output_line_directive_ = true;
    output_comment_ = false;
    support_trigraphs_ = false;
    //additional_include_dirs_;
    //macro_operations_;
}

Options::~Options() {
}

const String& Options::input_filepath() const {
    return input_filepath_;
}

const String& Options::output_filepath() const {
    return output_filepath_;
}

const String& Options::error_log_filepath() const {
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

const std::vector<String>& Options::additional_include_dirs() const {
    return additional_include_dirs_;
}

const std::vector<MacroDefinitionOperation>& Options::macro_operations() const {
    return macro_operations_;
}

bool Options::parse_options(const std::vector<String>& args) {
    if (ssize(args) > numeric_limits<int>::max()) {
        return false;   // too many options
    }

    int argc = static_cast<int>(ssize(args));
    for (int i = 1; i < argc; i++) {
        const String& arg = args[i];

        if (arg[0] == T_('-')) {
            switch (arg[1]) {
            case T_('\0'):
                if (input_filepath_.empty()) {
                    input_filepath_ = arg;
                }
                break;
            case T_('I'): {
                String path;
                if (arg[2] != T_('\0')) {
                    path = &arg[2];
                } else {
                    if ((i + 1) < argc) {
                        path = args[i + 1];
                        i++;
                    } else {
                        log_error(kNoOptionParameterError, arg[1]);
                        return false;
                    }
                }
                additional_include_dirs_.push_back(path);
                break;
            }
            case T_('D'): {
                String macro;
                if (arg[2] != T_('\0')) {
                    macro = &arg[2];
                } else {
                    if ((i + 1) < argc) {
                        macro = args[i + 1];
                        i++;
                    } else {
                        log_error(kNoOptionParameterError, arg[1]);
                        return false;
                    }
                }
                macro_operations_.push_back({MacroDefinitionOperationType::kDefine, macro});
                break;
            }
            case T_('U'): {
                String name;
                if (arg[2] != T_('\0')) {
                    name = &arg[2];
                } else {
                    if ((i + 1) < argc) {
                        name = args[i + 1];
                        i++;
                    } else {
                        log_error(kNoOptionParameterError, arg[1]);
                        return false;
                    }
                }
                macro_operations_.push_back({ MacroDefinitionOperationType::kUndefine, name });
                break;
            }
            //  TODO: 将来的には。
            //case 'P': {
            //    output_line_directive_ = false;
            //    break;
            //}
            //case 'C': {
            //    output_comment_ = true;
            //    break;
            //}
            case T_('o'): {
                String path;
                if (arg[2] != T_('\0')) {
                    path = &arg[2];
                } else {
                    if ((i + 1) < argc) {
                        path = args[i + 1];
                        i++;
                    } else {
                        log_error(kNoOptionParameterError, arg[1]);
                        return false;
                    }
                }
                if (output_filepath_.empty()) {
                    output_filepath_ = path;
                }
                break;
            }
            case T_('e'): {
                String path;
                if (arg[2] != T_('\0')) {
                    path = &arg[2];
                } else {
                    if ((i + 1) < argc) {
                        path = args[i + 1];
                        i++;
                    } else {
                        log_error(kNoOptionParameterError, arg[1]);
                        return false;
                    }
                }
                error_log_filepath_ = path;
                break;
            }
            case T_('t'):
                if (arg == T_("-trigraphs")) {
                    support_trigraphs_ = true;
                }
                break;
            case T_('h'):
                return false;

            default:
                log_error(kUnknownOptionError, arg[1]);
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
