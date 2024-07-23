#ifndef CC_PREPROCESSOR_SOURCEFILESTACK_H_
#define CC_PREPROCESSOR_SOURCEFILESTACK_H_

#include <functional>
#include <stack>

#include "pp_config.h"

namespace pp {

class SourceFile;

/**
 */
class SourceFileStack {
public:
    SourceFileStack();
    ~SourceFileStack();

    void push(SourceFile* source);
    void pop();

    SourceFile& current_source() const;
    SourceFile* current_source_pointer() const;
    String current_source_path() const;
    void current_source_path(const String& value);
    std::uint32_t current_source_line_number();

    void enum_files(std::function<bool (SourceFile*)> callback);

private:
    std::vector<SourceFile*> sources_;
};

}   // namespace pp

#endif  // CC_PREPROCESSOR_SOURCEFILESTACK_H_
