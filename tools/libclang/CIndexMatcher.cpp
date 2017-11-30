//===- CIndexCXX.cpp - Clang-C Source Indexing Library --------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the libclang support for C++ Matchers.
//
//===----------------------------------------------------------------------===//

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/Dynamic/Parser.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang-c/Index.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Debug.h"

#include "CXTranslationUnit.h"
#include "CXCursor.h"

#include <vector>
#include <memory>

using namespace clang;
using namespace clang::cxcursor;
using namespace clang::cxtu;
using namespace clang::ast_matchers;
using namespace clang::ast_matchers;
using namespace clang::ast_matchers::dynamic;

struct CXMatchFinderImpl
{
  MatchFinder Finder;
  std::vector<std::unique_ptr<MatchFinder::MatchCallback>> Callbacks;
};

struct CXMatchResultImpl
{
  MatchFinder::MatchResult Result;
};

extern "C" {

CXMatchResult clang_cloneMatchResult(CXMatchResult Result)
{
  return new CXMatchResultImpl(*Result);
}

void clang_disposeMatchResult(CXMatchResult Result)
{
  delete Result;
}

CXMatchFinder clang_createMatchFinder()
{
  return new CXMatchFinderImpl();
}

namespace {
struct CXMatcherCallback : public MatchFinder::MatchCallback {
  CXMatchCallbackRun Callback;
  CXClientData ClientData;

  CXMatcherCallback(CXMatchCallbackRun Callback, CXClientData ClientData) :
   Callback(Callback),
   ClientData(ClientData) {}

  void run(const MatchFinder::MatchResult &Result) override {
    CXMatchResultImpl result{Result};
    Callback(&result, ClientData);
  }
};
} // namespace

void clang_finderAddMatcher(CXMatchFinder Finder,
                            const char *Matcher,
                            CXMatchCallbackRun Callback,
                            CXClientData ClientData)
{
  Diagnostics Diag;
  auto DynMatcher = Parser::parseMatcherExpression(
    Matcher, &Diag);
  if (DynMatcher)
  {

    Finder->Callbacks.push_back(
      llvm::make_unique<CXMatcherCallback>(Callback, ClientData));
    Finder->Finder.addDynamicMatcher(*DynMatcher,
                                     Finder->Callbacks.back().get());
  }
}

void clang_disposeMatchFinder(CXMatchFinder Finder)
{
  delete Finder;
}

void clang_finderMatchAST(CXMatchFinder Finder,
                          CXTranslationUnit TU)
{
  auto &Ctx = getASTUnit(TU)->getASTContext();
  Finder->Finder.matchAST(Ctx);
}

CXCursor clang_getMatchResultCursor(CXMatchResult Result,
                                    CXTranslationUnit TU,
                                    const char * ID)
{
  auto & Map = Result->Result.Nodes.getMap();
  auto It = Map.find(ID);
  if (It == Map.end())
    return MakeCXCursorInvalid(CXCursor_NoDeclFound);
  auto & Node  = It->second;
  if (const Decl *D = Node.get<Decl>())
    return MakeCXCursor(D, TU, D->getSourceRange());
  else if (const Stmt *S = Node.get<Stmt>())
    return MakeCXCursor(S, nullptr, TU, S->getSourceRange());

  return MakeCXCursorInvalid(CXCursor_NotImplemented);
}

} // end extern "C"