// UnevaluatedAliasToLambdaCheck_AlwaysPropagate_Fix.cpp
// Simplified clang-tidy check that always propagates the cpass<T> provider
// outward through every consuming CallExpr parent (no stopping heuristic).
// Fix: robust parent-argument identity by normalizing expressions so operator<<
// chains (which often introduce temporaries) are correctly detected.

#include "UnevaluatedAliasToLambdaCheck.h"

#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Lex/Lexer.h"
#include "clang/Basic/OperatorKinds.h"
#include "llvm/ADT/StringRef.h"

using namespace clang;
using namespace clang::ast_matchers;

namespace clang {
namespace tidy {
namespace uneval {

// --- Utilities --------------------------------------------------------------

static std::string getText(const Stmt *S, const SourceManager &SM, const LangOptions &LO) {
  CharSourceRange R = CharSourceRange::getTokenRange(S->getSourceRange());
  return Lexer::getSourceText(R, SM, LO).str();
}

static std::string getText(const Expr *E, const SourceManager &SM, const LangOptions &LO) {
  CharSourceRange R = CharSourceRange::getTokenRange(E->getSourceRange());
  return Lexer::getSourceText(R, SM, LO).str();
}

static std::string replaceFirst(std::string Haystack,
                                llvm::StringRef Needle,
                                llvm::StringRef Replacement) {
  size_t pos = Haystack.find(Needle);
  if (pos != std::string::npos) {
    Haystack.replace(pos, Needle.size(), Replacement.str());
  }
  return Haystack;
}

// Normalize expression by stripping parens/implicit casts/materialize/bind temporaries
static const Expr *normalizeExpr(const Expr *E) {
  if (!E) return nullptr;
  const Expr *B = E->IgnoreParenImpCasts();
  if (const auto *BTE = dyn_cast<CXXBindTemporaryExpr>(B)) {
    if (const Expr *SE = BTE->getSubExpr())
      return normalizeExpr(SE);
  }
  if (const auto *MTE = dyn_cast<MaterializeTemporaryExpr>(B)) {
    if (const Expr *SE = MTE->getSubExpr())
      return normalizeExpr(SE);
  }
  if (const auto *ICE = dyn_cast<ImplicitCastExpr>(B)) {
    if (const Expr *SE = ICE->getSubExpr())
      return normalizeExpr(SE);
  }
  return B;
}

static bool isAliasTemplateNamed(QualType QT, llvm::StringRef AliasName) {
  if (QT.isNull()) return false;
  const Type *TP = QT.getTypePtr();
  if (const auto *TST = dyn_cast<TemplateSpecializationType>(TP)) {
    TemplateName TN = TST->getTemplateName();
    if (const auto *TD = TN.getAsTemplateDecl()) {
      if (const auto *Alias = dyn_cast<TypeAliasTemplateDecl>(TD))
        return Alias->getName() == AliasName;
    }
  }
  if (const auto *TT = dyn_cast<TypedefType>(TP)) {
    if (const TypedefNameDecl *TD = TT->getDecl()) {
      if (TD->getName() == AliasName)
        return true;
    }
  }
  return false;
}

// Detect immediate cpass<T> provider (direct, materialized, or member-call-on-cpass)
static const Expr* findImmediateCpassProvider(const Expr *E) {
  if (!E) return nullptr;
  const Expr *Base = normalizeExpr(E);

  // Direct cpass<T> typed expression
  if (isAliasTemplateNamed(Base->getType(), "cpass"))
    return Base;

  // Materialized temporary of cpass<T>
  if (const auto *MTE = dyn_cast<MaterializeTemporaryExpr>(Base)) {
    if (isAliasTemplateNamed(MTE->getType(), "cpass")) {
      if (const Expr *Sub = MTE->getSubExpr())
        return normalizeExpr(Sub);
      return Base;
    }
  }

  // Member call whose implicit object is cpass<T>
  if (const auto *MCall = dyn_cast<CXXMemberCallExpr>(Base)) {
    if (const Expr *Obj = MCall->getImplicitObjectArgument()) {
      const Expr *ObjBase = normalizeExpr(Obj);
      if (isAliasTemplateNamed(ObjBase->getType(), "cpass"))
        return ObjBase;
      if (const auto *ObjMTE = dyn_cast<MaterializeTemporaryExpr>(ObjBase)) {
        if (isAliasTemplateNamed(ObjMTE->getType(), "cpass")) {
          if (const Expr *Sub = ObjMTE->getSubExpr())
            return normalizeExpr(Sub);
          return ObjBase;
        }
      }
    }
  }

  return nullptr;
}

// Render a call expression into source-like text, handling operator calls specially.
static std::string buildCallText(const CallExpr *CE,
                                 const SourceManager &SM,
                                 const LangOptions &LO,
                                 const std::vector<std::string> &Args) {
  if (const auto *OpCall = dyn_cast<CXXOperatorCallExpr>(CE)) {
    OverloadedOperatorKind OpKind = OpCall->getOperator();
    llvm::StringRef OpSp = getOperatorSpelling(OpKind);

    if (OpKind == OO_Call && !Args.empty()) {
      // object(arg1, arg2, ...)
      std::string S = Args[0] + "(";
      for (size_t i = 1; i < Args.size(); ++i) {
        if (i > 1) S += ", ";
        S += Args[i];
      }
      S += ")";
      return S;
    } else if (OpKind == OO_Subscript && Args.size() == 2) {
      return Args[0] + "[" + Args[1] + "]";
    } else if (!OpSp.empty()) {
      if (Args.size() == 2)
        return Args[0] + " " + OpSp.str() + " " + Args[1];
      if (Args.size() == 1)
        return OpSp.str() + Args[0];
    }
  }

  // Default: normal function call callee(args...)
  std::string CalleeText = getText(CE->getCallee(), SM, LO);
  std::string S = CalleeText + "(";
  for (unsigned i = 0; i < Args.size(); ++i) {
    if (i) S += ", ";
    S += Args[i];
  }
  S += ")";
  return S;
}

// Build a lambda that forwards cval into Expr, with explicit trailing return type.
static std::string makeLambda(const std::string &Expr) {
  return "([&](auto&& cval) -> decltype(" + Expr + ") { return " + Expr + "; })";
}

// --- Check ------------------------------------------------------------------

UnevaluatedAliasToLambdaCheck::UnevaluatedAliasToLambdaCheck(llvm::StringRef Name,
                                                             ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context) {}

void UnevaluatedAliasToLambdaCheck::registerMatchers(MatchFinder *Finder) {
  // unevaluated<T> parameter declarations
  auto UnevaluatedParam =
      parmVarDecl(hasType(templateSpecializationType(
                      hasDeclaration(typeAliasTemplateDecl(hasName("unevaluated"))))))
          .bind("parm");

  Finder->addMatcher(functionDecl(forEachDescendant(UnevaluatedParam)).bind("fdecl"), this);

  // Call sites: wrap arguments for unevaluated<T> parameters into lazy lambdas
  Finder->addMatcher(
      callExpr(
          forEachArgumentWithParam(ignoringParenImpCasts(expr().bind("arg")), UnevaluatedParam),
          callee(functionDecl()))
          .bind("call"),
      this);

  // Function body: uses of unevaluated<T> params become x()
  Finder->addMatcher(
      declRefExpr(to(UnevaluatedParam), hasAncestor(functionDecl().bind("body-fdecl")))
          .bind("ref"),
      this);

  // Provider trigger: consider all calls; filter in check()
  Finder->addMatcher(callExpr().bind("any-call"), this);
}

void UnevaluatedAliasToLambdaCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *Parm       = Result.Nodes.getNodeAs<ParmVarDecl>("parm");
  const auto *FDecl      = Result.Nodes.getNodeAs<FunctionDecl>("fdecl");
  const auto *Arg        = Result.Nodes.getNodeAs<Expr>("arg");
  const auto *Call       = Result.Nodes.getNodeAs<CallExpr>("call");
  const auto *Ref        = Result.Nodes.getNodeAs<DeclRefExpr>("ref");
  const auto *AnyCall    = Result.Nodes.getNodeAs<CallExpr>("any-call");

  auto &SM  = *Result.SourceManager;
  const auto &LO = Result.Context->getLangOpts();

  // 1) unevaluated<T> declaration parameter type rewrite
  if (FDecl && Parm) {
    if (const TypeSourceInfo *TSI = Parm->getTypeSourceInfo()) {
      TypeLoc TL = TSI->getTypeLoc();
      SourceRange TypeSR = TL.getSourceRange();

      std::string AutoText = "auto";
      QualType QT = Parm->getType();
      if (QT.isConstQualified())     AutoText = "const " + AutoText;
      if (QT.isVolatileQualified())  AutoText = "volatile " + AutoText;
      if (QT->isLValueReferenceType()) AutoText += " &";
      else if (QT->isRValueReferenceType()) AutoText += " &&";

      diag(Parm->getLocation(), "replace parameter type unevaluated<T> with %0")
          << AutoText
          << FixItHint::CreateReplacement(CharSourceRange::getTokenRange(TypeSR), AutoText);
    }
  }

  // 2) unevaluated<T> call-site argument -> lazy lambda
  if (Call && Arg && Parm) {
    if (Arg->getBeginLoc().isMacroID()) return;

    std::string ArgText = getText(Arg, SM, LO);
    std::string Replacement = "[&]{ return (" + ArgText + "); }";
    diag(Arg->getBeginLoc(), "wrap unevaluated<T> argument in lazy lambda")
        << FixItHint::CreateReplacement(Arg->getSourceRange(), Replacement);
  }

  // 3) unevaluated<T> body uses: x -> x()
  if (Ref && Parm) {
    if (Ref->getBeginLoc().isMacroID()) return;

    const auto &Parents = Result.Context->getParents(*Ref);
    if (!Parents.empty()) {
      if (const auto *CE = Parents[0].get<CallExpr>()) {
        if (CE->getCallee() == Ref) return;
      }
      if (Parents[0].get<MemberExpr>()) return; // skip x.member
    }

    CharSourceRange RefRange = CharSourceRange::getTokenRange(Ref->getSourceRange());
    std::string Replacement = Ref->getNameInfo().getAsString() + "()";
    diag(Ref->getBeginLoc(), "replace use of unevaluated<T> parameter with call to lambda")
        << FixItHint::CreateReplacement(RefRange, Replacement);
  }

  // 4) cpass<T> call rewrite with unconditional outward chaining.
  //    Replace the immediate provider argument with provider(lambda(inner)),
  //    then unconditionally wrap every parent call that directly consumes the previous node.
  if (AnyCall) {
    if (AnyCall->getBeginLoc().isMacroID()) return;

    // Collect original argument texts
    std::vector<std::string> ArgTexts;
    ArgTexts.reserve(AnyCall->getNumArgs());
    for (unsigned i = 0; i < AnyCall->getNumArgs(); ++i)
      ArgTexts.push_back(getText(AnyCall->getArg(i), SM, LO));

    // Find one immediate provider argument
    int ProviderIndex = -1;
    const Expr *ProviderExpr = nullptr;
    std::string ProviderText;
    std::vector<std::string> ArgTextsForwarded = ArgTexts;

    for (unsigned i = 0; i < AnyCall->getNumArgs(); ++i) {
      const Expr *E = AnyCall->getArg(i);
      if (const Expr *P = findImmediateCpassProvider(E)) {
        ProviderIndex = static_cast<int>(i);
        ProviderExpr = P;
        ProviderText = getText(P, SM, LO);
        ArgTextsForwarded[i] = replaceFirst(ArgTexts[i], ProviderText,
                                            "std::forward<decltype(cval)>(cval)");
        break; // rewrite one provider per step
      }
    }
    if (ProviderIndex < 0) return;

    // Build inner rewritten call via provider
    std::string InnerCallForwarded = buildCallText(AnyCall, SM, LO, ArgTextsForwarded);
    std::string CurrentChain = ProviderText + makeLambda(InnerCallForwarded);

    // Walk up: append a lambda at each parent that immediately consumes the current expr.
    // Unconditionally continue until there are no more consuming parent CallExprs.
    const Stmt *CurrentNode = AnyCall;
    const CallExpr *OutermostReplaceNode = AnyCall;
    std::string FinalReplacementText = CurrentChain;

    while (true) {
      auto Parents = Result.Context->getParents(*CurrentNode);
      if (Parents.empty()) break;
      const Stmt *ParentStmt = Parents[0].get<Stmt>();
      if (!ParentStmt) break;

      const auto *ParentCall = dyn_cast<CallExpr>(ParentStmt);
      if (!ParentCall) break;

      // Identify immediate-argument position by AST identity using normalized expressions.
      int ArgPos = -1;
      const Expr *CurrentExprNode = dyn_cast<Expr>(CurrentNode);
      const Expr *CurrentNorm = normalizeExpr(CurrentExprNode);

      for (unsigned i = 0; i < ParentCall->getNumArgs(); ++i) {
        const Expr *ArgI = ParentCall->getArg(i);
        const Expr *ArgINorm = normalizeExpr(ArgI);
        if (ArgINorm && CurrentNorm && ArgINorm == CurrentNorm) {
          ArgPos = static_cast<int>(i);
          break;
        }
      }

      // If we didn't find by normalized pointer equality, fall back to source-text match of the first occurrence.
      if (ArgPos < 0) {
        // Render parent args and try to find the textual occurrence of the current node's text.
        std::vector<std::string> ParentArgTexts;
        ParentArgTexts.reserve(ParentCall->getNumArgs());
        for (unsigned i = 0; i < ParentCall->getNumArgs(); ++i)
          ParentArgTexts.push_back(getText(ParentCall->getArg(i), SM, LO));

        std::string CurrText = getText(CurrentExprNode, SM, LO);
        for (unsigned i = 0; i < ParentArgTexts.size(); ++i) {
          if (ParentArgTexts[i].find(CurrText) != std::string::npos) {
            ArgPos = static_cast<int>(i);
            break;
          }
        }
      }

      if (ArgPos < 0) break;

      // Prepare rendering of the parent expression where the immediate argument is forwarded.
      std::vector<std::string> ParentArgTexts;
      ParentArgTexts.reserve(ParentCall->getNumArgs());
      for (unsigned i = 0; i < ParentCall->getNumArgs(); ++i)
        ParentArgTexts.push_back(getText(ParentCall->getArg(i), SM, LO));

      std::vector<std::string> ParentArgTextsForwarded = ParentArgTexts;
      ParentArgTextsForwarded[ArgPos] = "std::forward<decltype(cval)>(cval)";
      std::string ParentInnerForwarded = buildCallText(ParentCall, SM, LO, ParentArgTextsForwarded);

      // Append a lambda to replace the parent.
      FinalReplacementText += makeLambda(ParentInnerForwarded);
      OutermostReplaceNode = ParentCall;

      // Move up unconditionally
      CurrentNode = ParentCall;
    }

    // Emit replacement for the outermost parent we appended to (or the inner call if no parents consumed it).
    CharSourceRange ReplaceRange = CharSourceRange::getTokenRange(OutermostReplaceNode->getSourceRange());
    diag(OutermostReplaceNode->getBeginLoc(),
         "unconditionally rewrite chained calls using immediate cpass<T>; wrap every consuming parent")
        << FixItHint::CreateReplacement(ReplaceRange, FinalReplacementText);
  }
}

} // namespace uneval
} // namespace tidy
} // namespace clang
