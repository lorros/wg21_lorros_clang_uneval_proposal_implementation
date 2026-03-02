#pragma once

#include "clang-tidy/ClangTidyCheck.h"

namespace clang {
namespace tidy {
namespace uneval {

class UnevaluatedAliasToLambdaCheck : public ClangTidyCheck {
public:
  UnevaluatedAliasToLambdaCheck(llvm::StringRef Name, ClangTidyContext *Context);

  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
};

} // namespace uneval
} // namespace tidy
} // namespace clang
