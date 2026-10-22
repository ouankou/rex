#include "clang-to-dot.hpp"
#include "sage3basic.h"

#include <clang/AST/ASTNodeTraverser.h>
#include <clang/AST/TextNodeDumper.h>
#include <clang/Frontend/CompilerInstance.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/Support/raw_ostream.h>

#include <fstream>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

namespace {
void writeDotString(std::ostream &output, llvm::StringRef text) {
  output << '"';
  for (char ch : text) {
    switch (ch) {
    case '\\':
      output << "\\\\";
      break;
    case '"':
      output << "\\\"";
      break;
    case '\n':
      output << "\\l";
      break;
    case '\r':
      output << "\\r";
      break;
    case '\t':
      output << "\\t";
      break;
    default:
      output << ch;
      break;
    }
  }
  output << '"';
}

class GraphNodeDumper {
  const clang::ASTContext &context;
  std::ostream &output;
  std::function<void(clang::QualType)> visitType;
  llvm::DenseMap<clang::QualType, size_t> typeNodes;
  struct Frame {
    size_t identifier;
    bool described = false;
  };
  std::vector<Frame> stack;
  size_t nextIdentifier = 0;

  void attachType(clang::QualType type) {
    auto existing = typeNodes.find(type);
    if (existing != typeNodes.end()) {
      output << "  ident" << stack.back().identifier << " -> ident"
             << existing->second << " [label=\"type\"];\n";
    } else {
      visitType(type);
    }
  }

public:
  GraphNodeDumper(const clang::ASTContext &context, std::ostream &output,
                  std::function<void(clang::QualType)> visitType)
      : context(context), output(output), visitType(std::move(visitType)) {}

  template <class Callback> void AddChild(Callback callback) {
    AddChild("", callback);
  }

  template <class Callback>
  void AddChild(llvm::StringRef edgeLabel, Callback callback) {
    const size_t identifier = nextIdentifier++;
    if (!stack.empty()) {
      output << "  ident" << stack.back().identifier << " -> ident"
             << identifier << " [label=";
      writeDotString(output, edgeLabel);
      output << "];\n";
    }
    stack.push_back({identifier});
    callback();
    ROSE_ASSERT(stack.back().identifier == identifier &&
                stack.back().described);
    stack.pop_back();
  }

  template <class Describe> void describe(Describe describeNode) {
    ROSE_ASSERT(!stack.empty() && !stack.back().described);
    const size_t identifier = stack.back().identifier;
    std::string label;
    llvm::raw_string_ostream stream(label);
    clang::TextNodeDumper description(stream, context, false);
    description.AddChild([&] { describeNode(description); });
    stream.flush();
    output << "  ident" << identifier << " [label=";
    writeDotString(output, label);
    output << "];\n";
    stack.back().described = true;
  }

  void ReferenceDecl(const clang::Decl *declaration) {
    AddChild("specialization", [&] {
      describe([&](clang::TextNodeDumper &description) {
        description.dumpBareDeclRef(declaration);
      });
    });
  }

  template <class Node, class... Extra>
  void Visit(const Node &node, const Extra &...extra) {
    describe([&](clang::TextNodeDumper &description) {
      description.Visit(node, extra...);
    });

    if constexpr (std::is_same_v<Node, const clang::Type *>) {
      typeNodes.try_emplace(clang::QualType(node, 0), stack.back().identifier);
    } else if constexpr (std::is_same_v<Node, clang::QualType>) {
      typeNodes.try_emplace(node, stack.back().identifier);
    }

    if constexpr (std::is_same_v<Node, const clang::Decl *>) {
      if (const auto *value = llvm::dyn_cast_or_null<clang::ValueDecl>(node)) {
        ROSE_ASSERT(!value->getType().isNull());
        attachType(value->getType());
      }
    } else if constexpr (std::is_same_v<Node, const clang::Stmt *>) {
      if (const auto *expression = llvm::dyn_cast_or_null<clang::Expr>(node)) {
        // Clang retains syntax-only expressions such as ParenListExpr and
        // unevaluated strings with a null QualType. Display that typed edge.
        attachType(expression->getType());
      }
    }
  }
};

class ClangAstGraph
    : public clang::ASTNodeTraverser<ClangAstGraph, GraphNodeDumper> {
  GraphNodeDumper dumper;

public:
  ClangAstGraph(const clang::ASTContext &context, std::ostream &output)
      : dumper(context, output, [this](clang::QualType type) { Visit(type); }) {
  }
  GraphNodeDumper &doGetNodeDelegate() { return dumper; }

  // Match ASTDumper: a redeclaration references specialization identities;
  // expanding them again follows friend redeclarations back into their owner.
  template <class Template>
  void VisitRedeclarableTemplate(const Template *declaration,
                                 bool includeExplicitInstantiations) {
    dumpTemplateParameters(declaration->getTemplateParameters());
    Visit(declaration->getTemplatedDecl());
    for (const auto *specialization : declaration->specializations()) {
      if (!declaration->isCanonicalDecl()) {
        dumper.ReferenceDecl(specialization);
        continue;
      }
      using Specialization =
          std::remove_cv_t<std::remove_pointer_t<decltype(specialization)>>;
      for (const auto *redecl : specialization->redecls()) {
        const auto *specific = llvm::dyn_cast<Specialization>(redecl);
        if (specific == nullptr) {
          dumper.ReferenceDecl(redecl);
          continue;
        }
        const auto kind = specific->getTemplateSpecializationKind();
        const bool explicitInstantiation =
            kind == clang::TSK_ExplicitInstantiationDeclaration ||
            kind == clang::TSK_ExplicitInstantiationDefinition;
        if (kind == clang::TSK_ExplicitSpecialization ||
            (explicitInstantiation && !includeExplicitInstantiations)) {
          dumper.ReferenceDecl(specific);
        } else {
          Visit(specific);
        }
      }
    }
  }

  void VisitClassTemplateDecl(const clang::ClassTemplateDecl *declaration) {
    VisitRedeclarableTemplate(declaration, false);
  }
  void
  VisitFunctionTemplateDecl(const clang::FunctionTemplateDecl *declaration) {
    VisitRedeclarableTemplate(declaration, true);
  }
  void VisitVarTemplateDecl(const clang::VarTemplateDecl *declaration) {
    VisitRedeclarableTemplate(declaration, false);
  }
};
} // namespace

void writeClangAstGraph(clang::CompilerInstance &compiler,
                        const std::string &filename) {
  ROSE_ASSERT(!compiler.getDiagnostics().hasErrorOccurred());
  std::ofstream output(filename);
  if (!output) {
    llvm::errs() << "Cannot open Clang AST graph: " << filename << "\n";
    ROSE_ABORT();
  }
  output << "digraph {\n";
  ClangAstGraph graph(compiler.getASTContext(), output);
  graph.Visit(compiler.getASTContext().getTranslationUnitDecl());
  output << "}\n";
  output.close();
  if (!output) {
    llvm::errs() << "Cannot write Clang AST graph: " << filename << "\n";
    ROSE_ABORT();
  }
}
