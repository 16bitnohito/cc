#ifndef PREPROCESSOR_OPTIONS_H_
#define PREPROCESSOR_OPTIONS_H_

#include <string>
#include <vector>
#include <preprocessor/utility.h>

namespace pp {

/**
 */
class Options {
public:
    Options();
    ~Options();

    const String& input_filepath() const;
    const String& output_filepath() const;
    const String& error_log_filepath() const;
    bool output_line_directive() const;
    bool output_comment() const;
    bool support_trigraphs() const;
    const std::vector<String>& additional_include_dirs() const;
    const std::vector<String>& macro_defs() const;
    const std::vector<String>& macro_undefs() const;

    bool parse_options(const std::vector<String>& args);
    void print_usage();

private:
    String input_encoding_;
    String input_filepath_;
    String output_encoding_;
    String output_filepath_;
    String error_log_filepath_;
    bool output_line_directive_;
    bool output_comment_;
    bool support_trigraphs_;
    std::vector<String> additional_include_dirs_;
    std::vector<String> macro_defs_;
    std::vector<String> macro_undefs_;
};

}   // namespace pp

#endif  // PREPROCESSOR_OPTIONS_H_
