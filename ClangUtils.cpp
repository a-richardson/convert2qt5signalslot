#include "ClangUtils.h"

#include <clang/AST/DeclCXX.h>

using namespace clang;
using namespace llvm;

static void collectAllUsingNamespaceDeclsHelper(const clang::DeclContext* ctx, std::vector<UsingDirectiveDecl*>& buf,
        SourceLocation callLocation, const SourceManager& sm) {
    if (!ctx) {
        return;
    }
    for (auto&& udir : ctx->using_directives()) {
        // udir->dump(outs());
        if (!sm.isBeforeInTranslationUnit(udir->getLocStart(), callLocation)) {
            // outs() << "Using directive is after call location, not adding!\n";
            continue;
        }
        buf.push_back(udir);
    }
    collectAllUsingNamespaceDeclsHelper(ctx->getLookupParent(), buf, callLocation, sm);
}

static std::vector<UsingDirectiveDecl*> collectAllUsingNamespaceDecls(const clang::DeclContext* ctx,
        SourceLocation callLocation, const SourceManager& sm) {
    std::vector<UsingDirectiveDecl*> ret;
    collectAllUsingNamespaceDeclsHelper(ctx, ret, callLocation, sm);
    return ret;
}

static void collectAllUsingDeclsHelper(const clang::DeclContext* ctx, std::vector<UsingShadowDecl*>& buf,
        SourceLocation callLocation, const SourceManager& sm) {
    if (!ctx) {
        return;
    }
    for (auto it = ctx->decls_begin(); it != ctx->decls_end(); ++it) {
        auto usingDecl = dyn_cast<UsingDecl>(*it);
        if (!usingDecl) {
            continue;
        }
        if (!sm.isBeforeInTranslationUnit(usingDecl->getLocStart(), callLocation)) {
            //outs() << "Using decl is after call location, not adding!\n";
            continue;
        }
        //usingDecl->dump(outs());
        for (auto shadowIt = usingDecl->shadow_begin(); shadowIt != usingDecl->shadow_end(); shadowIt++) {
            //(*shadowIt)->dump(outs());
            buf.push_back(*shadowIt);
        }
    }
    collectAllUsingDeclsHelper(ctx->getLookupParent(), buf, callLocation, sm);
}

static std::vector<UsingShadowDecl*> collectAllUsingDecls(const clang::DeclContext* ctx, SourceLocation callLocation,
        const SourceManager& sm) {
    std::vector<UsingShadowDecl*> ret;
    collectAllUsingDeclsHelper(ctx, ret, callLocation, sm);
    return ret;
}

//FIXME don't return int * but int*
std::string ClangUtils::getLeastQualifiedName(clang::QualType type,
        const clang::DeclContext* containingFunction, const clang::CallExpr* callExpression, bool verbose,
        clang::ASTContext* ast) {

    const TagType* tt = type->getAs<TagType>();
    if (!tt) {
        return withoutUselessWhitespace(type.getAsString()); // could be builtin type e.g. int
    }
    auto td = tt->getDecl();
    auto targetTypeQualifiers = getNameQualifiers(td);
    //printParentContexts(type);
    //outs() << ", name qualifiers: " << targetTypeQualifiers.size() << "\n";
    assert(td->Equals(targetTypeQualifiers[0]));
    // TODO template arguments


    if (targetTypeQualifiers.size() < 2) {
        // no need to qualify the name if there is no surrounding context
        return withoutUselessWhitespace(td->getNameAsString());
    }
    auto usingDecls = collectAllUsingDecls(containingFunction,
            sourceLocationBeforeStmt(callExpression, ast), ast->getSourceManager());

    auto isUsingType = [td](UsingShadowDecl* usd) {
        auto cxxRecord = dyn_cast<CXXRecordDecl>(usd->getTargetDecl());
        return cxxRecord && td->Equals(cxxRecord);
    };
    if (contains(usingDecls, isUsingType)) {
        if (verbose) {
            outs() << "Using decl for " << td->getQualifiedNameAsString() << " exists, not qualifying\n";
        }
        // no need to qualify the name if there is a using decl for the current type
        return withoutUselessWhitespace(td->getNameAsString());
    }

    auto usingNamespaceDecls = collectAllUsingNamespaceDecls(containingFunction,
            sourceLocationBeforeStmt(callExpression, ast), ast->getSourceManager());

    // have to qualify, but check the current scope first
    auto containingScopeQualifiers = getNameQualifiers(containingFunction->getLookupParent());
    // type must always be included, now check whether the other scopes have to be explicitly named
    // it's not neccessary if the current function scope is also inside that namespace/class
    // for some reason twine crashes here, use std::string
    llvm::SmallString<64> buffer;
    buffer = td->getName();
    auto prependNamedDecl = [](llvm::SmallString<64>& buf, const NamedDecl* decl) {
        llvm::SmallString<64> buf2 = decl->getName();
        buf2 += "::";
        buf2 += buf;
        buf = buf2;
    };
    for (uint i = 1; i < targetTypeQualifiers.size(); ++i) {
        const DeclContext* ctx = targetTypeQualifiers[i];
        assert(ctx->isNamespace() || ctx->isRecord());
        auto declContextEquals = [ctx](const DeclContext* dc) {
            return ctx->Equals(dc);
        };
        auto isUsingNamespaceForContext = [ctx](UsingDirectiveDecl* ud) {
            return ud->getNominatedNamespace()->Equals(ctx);
        };
        auto isUsingDeclForContext = [ctx](UsingShadowDecl* usd) {
            auto usedCtx = dyn_cast<DeclContext>(usd->getTargetDecl());
            return usedCtx && usedCtx->Equals(ctx);
        };
        if (contains(containingScopeQualifiers, declContextEquals)) {
            auto named = cast<NamedDecl>(ctx);
            if (verbose) {
                outs() << "Don't need to add " << named->getQualifiedNameAsString() << " to lookup\n";
            }
        } else if (contains(usingNamespaceDecls, isUsingNamespaceForContext)) {
            auto ns = cast<NamespaceDecl>(ctx);
            if (verbose) {
                outs() << "Ending qualifier search: using namespace for '" << ns->getQualifiedNameAsString() << "' exists\n";
            }
            break;
        } else if (contains(usingDecls, isUsingDeclForContext)) {
            auto named = cast<NamedDecl>(ctx);
            // this is the last one we have to add since a using directive exists
            prependNamedDecl(buffer, named);
            if (verbose) {
                outs() << "Ending qualifier search: using for '" << named->getQualifiedNameAsString() << "' exists\n";
            }
            break;
        } else {
            if (auto record = dyn_cast<CXXRecordDecl>(ctx)) {
                prependNamedDecl(buffer, record);
            } else if (auto ns = dyn_cast<NamespaceDecl>(ctx)) {
                prependNamedDecl(buffer, ns);
            } else {
                // this should never happen
                outs() << "Weird type:" << ctx->getDeclKindName() << ":" << (void*) ctx << "\n";
                printParentContexts(td);
            }
        }
    }
    return withoutUselessWhitespace(buffer.str().str());
}

std::vector<const clang::DeclContext*> ClangUtils::getNameQualifiers(const clang::DeclContext* ctx) {
    std::vector<const clang::DeclContext*> ret;
    while (ctx) {
        // only namespaces and classes/structs/unions add additional qualifiers to the name lookup
        if (auto ns = dyn_cast<clang::NamespaceDecl>(ctx)) {
            if (!ns->isAnonymousNamespace()) {
                ret.push_back(ns);
            }
        } else if (isa<clang::CXXRecordDecl>(ctx)) {
            ret.push_back(ctx);
        }
        ctx = ctx->getLookupParent();
    }
    return ret;
}

void ClangUtils::printParentContexts(const clang::DeclContext* base) {
    for (auto ctx : getParentContexts(base)) {
        outs() << "::";
        if (auto record = dyn_cast<clang::CXXRecordDecl>(ctx)) {
            outs() << record->getName() << "(record)";
        } else if (auto ns = dyn_cast<clang::NamespaceDecl>(ctx)) {
            outs() << ns->getName() << "(namespace)";
        } else if (auto func = dyn_cast<clang::FunctionDecl>(ctx)) {
            if (isa<clang::CXXConstructorDecl>(ctx)) {
                outs() << "(ctor)";
            } else if (isa<clang::CXXDestructorDecl>(ctx)) {
                outs() << "(dtor)";
            } else if (isa<clang::CXXConversionDecl>(ctx)) {
                outs() << "(conversion)";
            } else {
                outs() << func->getName() << "(func)";
            }
        } else if (dyn_cast<clang::TranslationUnitDecl>(ctx)) {
            outs() << "(translation unit)";
        } else {
            outs() << "unknown(" << ctx->getDeclKindName() << ")";
        }
    }
}

bool ClangUtils::inheritsFrom(const clang::CXXRecordDecl* cls, const char* name, clang::AccessSpecifier access) {
    for (auto it = cls->bases_begin(); it != cls->bases_end(); ++it) {
        clang::QualType type = it->getType();
        auto baseDecl = type->getAsCXXRecordDecl(); // will always succeed
        assert(baseDecl);
        // llvm::outs() << "Checking " << cls->getName() << " for " << name << "\n";
        if (it->getAccessSpecifier() == access && baseDecl->getName() == name) {
            return true;
        }
        // now check the base classes for the base class
        if (inheritsFrom(baseDecl, name, access)) {
            return true;
        }
    }
    return false;
}

bool ClangUtils::isOrInheritsFrom(const clang::CXXRecordDecl* cls, const char* name, clang::AccessSpecifier access) {
    // llvm::outs() << "Checking " << cls->getName() << " for " << name << "\n";
    if (cls->getName() == name) {
        return true;
    }
    return inheritsFrom(cls, name, access);
}
