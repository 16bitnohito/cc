#ifndef PREPROCESSOR_OPTIONS_H_
#define PREPROCESSOR_OPTIONS_H_

#include <string>
#include <vector>

namespace pp {

/**
 */
class Options {
public:
    Options();
    ~Options();

    const std::string& input_filepath() const;
    const std::string& output_filepath() const;
    const std::string& error_log_filepath() const;
    bool output_line_directive() const;
    bool output_comment() const;
    bool support_trigraphs() const;
    const std::vector<std::string>& additional_include_dirs() const;
    const std::vector<std::string>& macro_defs() const;
    const std::vector<std::string>& macro_undefs() const;

    bool parse_options(const std::vector<std::string>& args);
    void print_usage();

private:
    std::string input_encoding_;
    std::string input_filepath_;
    std::string output_encoding_;
    std::string output_filepath_;
    std::string error_log_filepath_;
    bool output_line_directive_;
    bool output_comment_;
    bool support_trigraphs_;
    std::vector<std::string> additional_include_dirs_;
    std::vector<std::string> macro_defs_;
    std::vector<std::string> macro_undefs_;
};

}   // namespace pp

#endif  // PREPROCESSOR_OPTIONS_H_
