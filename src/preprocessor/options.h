#ifndef PREPROCESSOR_OPTIONS_H_
#define PREPROCESSOR_OPTIONS_H_

#include <string>
#include <vector>
#include <preprocessor/utility.h>

namespace pp {

enum class MacroDefinitionOperationType {
    kDefine,
    kUndefine,
};

/**
 */
class MacroDefinitionOperation {
public:
    MacroDefinitionOperation(MacroDefinitionOperationType operation, const String& operand)
        : operation_(operation)
        , operand_(operand) {
    }

    MacroDefinitionOperationType operation() const noexcept {
        return operation_;
    }

    const String& operand() const noexcept {
        return operand_;
    }

private:
    MacroDefinitionOperationType operation_;
    String operand_;
};

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
    const std::vector<MacroDefinitionOperation>& macro_operations() const;

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
    std::vector<MacroDefinitionOperation> macro_operations_;
};

}   // namespace pp

#endif  // PREPROCESSOR_OPTIONS_H_
