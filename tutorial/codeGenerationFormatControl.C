// This example will be made more sophisticated later, for now it just
// modifies the indentation of nested code blocks (from 2 spaces/block
// to 5 spaces/block).

#include "rose.h"

#include "unparseFormatHelp.h"

class CustomCodeFormat : public UnparseFormatHelp {
public:
  CustomCodeFormat();
  ~CustomCodeFormat();

  std::optional<OutputPosition>
  getPosition(SgLocatedNode *, SgUnparse_Info &info, FormatOpt opt) override;

  // return the value for indentation of code (part of control over style)
  virtual int tabIndent();

  // return the value for where line wrapping starts (part of control over
  // style)
  virtual int maxLineLength();

private:
  int defaultLineLength;
  int defaultIndentation;
};

CustomCodeFormat::CustomCodeFormat() {
  // default values here!
  defaultLineLength = 20;
  defaultIndentation = 5;
}

CustomCodeFormat::~CustomCodeFormat() {}

// Return no custom position to use the normal line and column placement.
std::optional<UnparseFormatHelp::OutputPosition>
CustomCodeFormat::getPosition(SgLocatedNode *, SgUnparse_Info &info,
                              FormatOpt opt) {
  return std::nullopt;
}

int CustomCodeFormat::tabIndent() {
  // Modify the indentation of the generated code (trival example of tailoring
  // code generation)
  return defaultIndentation;
}

int CustomCodeFormat::maxLineLength() { return defaultLineLength; }

int main(int argc, char *argv[]) {
  // Build the project object (AST) which we will fill up with multiple files
  // and use as a handle for all processing of the AST(s) associated with one or
  // more source files.
  SgProject *project = new SgProject(argc, argv);

  CustomCodeFormat *formatControl = new CustomCodeFormat();

  return backend(project, formatControl);
}
