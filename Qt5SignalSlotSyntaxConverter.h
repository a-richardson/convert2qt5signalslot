#pragma once
#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Tooling/Refactoring.h>

namespace clang {
    class Sema;
}

#include <atomic>

using clang::ast_matchers::MatchFinder;

class ConnectCallMatcher : public MatchFinder::MatchCallback, public clang::tooling::SourceFileCallbacks {
public:
    ConnectCallMatcher(clang::tooling::Replacements* reps, const std::vector<std::string>& files)
        : foundMatches(0), convertedMatches(0), skippedMatches(0), failedMatches(0), replacements(reps),
          refactoringFiles(files) {}
    virtual void run(const MatchFinder::MatchResult& result) override;
    void convert(const MatchFinder::MatchResult& result);
    bool handleBeginSource(clang::CompilerInstance &CI, llvm::StringRef Filename) override;
    void handleEndSource() override;

    /**
     * @return The new signal/slot expression for the connect call
     */
    std::string calculateReplacementStr(const clang::CXXRecordDecl* type,
            const clang::StringLiteral* connectStr, const std::string& prepend);
    void printStats() const;
private:
    std::atomic_int foundMatches;
    std::atomic_int convertedMatches;
    std::atomic_int skippedMatches;
    std::atomic_int failedMatches;
    clang::CompilerInstance* currentCompilerInstance = nullptr;
    clang::Sema* sema = nullptr;
    clang::Preprocessor* pp = nullptr;
    clang::tooling::Replacements* replacements = nullptr;
    const std::vector<std::string>& refactoringFiles;
};


class ConnectConverter {
public:
    ConnectConverter(clang::tooling::Replacements* reps, const std::vector<std::string>& files) : matcher(reps, files) {}
    void setupMatchers(MatchFinder* matchFinder);
    void printStats() const {
        matcher.printStats();
    }
    clang::tooling::SourceFileCallbacks* sourceCallback() { return &matcher; }
private:
    ConnectCallMatcher matcher;
};

static inline std::string getRealPath(const std::string& path) {
    char buf[PATH_MAX];
    const char* ret = realpath(path.c_str(), buf);
    if (!ret) {
        //resolving failed, could be a virtual path
        return path;
    }
    return std::string(ret);
}
