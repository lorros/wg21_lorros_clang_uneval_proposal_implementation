#include "UnevaluatedAliasToLambdaCheck.h"
#include "clang-tidy/ClangTidy.h"
#include "clang-tidy/ClangTidyModule.h"
#include "clang-tidy/ClangTidyModuleRegistry.h"

using namespace clang::tidy;

namespace clang {
namespace tidy {
namespace uneval {

class UnevalTidyModule : public ClangTidyModule {
public:
  void addCheckFactories(ClangTidyCheckFactories &Factories) override {
    Factories.registerCheck<UnevaluatedAliasToLambdaCheck>("uneval-unevaluated-to-lambda");
  }
};

} // namespace uneval
} // namespace tidy
} // namespace clang

// Register the module with the global registry.
static ClangTidyModuleRegistry::Add<clang::tidy::uneval::UnevalTidyModule>
    X("uneval-module", "Adds unevaluated<T> -> auto + lazy lambda transformation.");

// This anchor is used to force the linker to link in the generated object file
volatile int UnevalModuleAnchorSource = 0;
