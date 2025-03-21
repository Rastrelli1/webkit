/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2006, 2007, 2008, 2009, 2010, 2011, 2013 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "ExecutableInfo.h"
#include "Lexer.h"
#include "ModuleScopeData.h"
#include "Nodes.h"
#include "ParseHash.h"
#include "ParserArena.h"
#include "ParserError.h"
#include "ParserFunctionInfo.h"
#include "ParserTokens.h"
#include "SourceProvider.h"
#include "SourceProviderCache.h"
#include "SourceProviderCacheItem.h"
#include "VariableEnvironment.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>

namespace JSC {

class FunctionMetadataNode;
class FunctionParameters;
class Identifier;
class VM;
class SourceCode;
class SyntaxChecker;
struct DebuggerParseData;

// Macros to make the more common TreeBuilder types a little less verbose
#define TreeStatement typename TreeBuilder::Statement
#define TreeExpression typename TreeBuilder::Expression
#define TreeFormalParameterList typename TreeBuilder::FormalParameterList
#define TreeSourceElements typename TreeBuilder::SourceElements
#define TreeClause typename TreeBuilder::Clause
#define TreeClauseList typename TreeBuilder::ClauseList
#define TreeArguments typename TreeBuilder::Arguments
#define TreeArgumentsList typename TreeBuilder::ArgumentsList
#define TreeFunctionBody typename TreeBuilder::FunctionBody
#define TreeClassExpression typename TreeBuilder::ClassExpression
#define TreeProperty typename TreeBuilder::Property
#define TreePropertyList typename TreeBuilder::PropertyList
#define TreeDestructuringPattern typename TreeBuilder::DestructuringPattern

COMPILE_ASSERT(LastUntaggedToken < 64, LessThan64UntaggedTokens);

enum SourceElementsMode { CheckForStrictMode, DontCheckForStrictMode };
enum FunctionBodyType { ArrowFunctionBodyExpression, ArrowFunctionBodyBlock, StandardFunctionBodyBlock };
enum class FunctionNameRequirements { None, Named, Unnamed };

enum class DestructuringKind {
    DestructureToVariables,
    DestructureToLet,
    DestructureToConst,
    DestructureToCatchParameters,
    DestructureToParameters,
    DestructureToExpressions
};

enum class DeclarationType { 
    VarDeclaration, 
    LetDeclaration,
    ConstDeclaration
};

enum class DeclarationImportType {
    Imported,
    ImportedNamespace,
    NotImported
};

enum DeclarationResult {
    Valid = 0,
    InvalidStrictMode = 1 << 0,
    InvalidDuplicateDeclaration = 1 << 1
};

typedef uint8_t DeclarationResultMask;

enum class DeclarationDefaultContext {
    Standard,
    ExportDefault,
};

enum class InferName {
    Allowed,
    Disallowed,
};

template <typename T> inline bool isEvalNode() { return false; }
template <> inline bool isEvalNode<EvalNode>() { return true; }

struct ScopeLabelInfo {
    UniquedStringImpl* uid;
    bool isLoop;
};

ALWAYS_INLINE static bool isArguments(const VM* vm, const Identifier* ident)
{
    return vm->propertyNames->arguments == *ident;
}
ALWAYS_INLINE static bool isEval(const VM* vm, const Identifier* ident)
{
    return vm->propertyNames->eval == *ident;
}
ALWAYS_INLINE static bool isEvalOrArgumentsIdentifier(const VM* vm, const Identifier* ident)
{
    return isEval(vm, ident) || isArguments(vm, ident);
}
ALWAYS_INLINE static bool isIdentifierOrKeyword(const JSToken& token)
{
    return token.m_type == IDENT || token.m_type & KeywordTokenFlag;
}
// _Any_ContextualKeyword includes keywords such as "let" or "yield", which have a specific meaning depending on the current parse mode
// or strict mode. These helpers allow to treat all contextual keywords as identifiers as required.
ALWAYS_INLINE static bool isAnyContextualKeyword(const JSToken& token)
{
    return token.m_type >= FirstContextualKeywordToken && token.m_type <= LastContextualKeywordToken;
}
ALWAYS_INLINE static bool isIdentifierOrAnyContextualKeyword(const JSToken& token)
{
    return token.m_type == IDENT || isAnyContextualKeyword(token);
}
// _Safe_ContextualKeyword includes only contextual keywords which can be treated as identifiers independently from parse mode. The exeption
// to this rule is `await`, but matchSpecIdentifier() always treats it as an identifier regardless.
ALWAYS_INLINE static bool isSafeContextualKeyword(const JSToken& token)
{
    return token.m_type >= FirstSafeContextualKeywordToken && token.m_type <= LastSafeContextualKeywordToken;
}

JS_EXPORT_PRIVATE extern std::atomic<unsigned> globalParseCount;

struct Scope {
    WTF_MAKE_NONCOPYABLE(Scope);

public:
    Scope(const VM* vm, bool isFunction, bool isGenerator, bool strictMode, bool isArrowFunction, bool isAsyncFunction)
        : m_vm(vm)
        , m_shadowsArguments(false)
        , m_usesEval(false)
        , m_needsFullActivation(false)
        , m_hasDirectSuper(false)
        , m_needsSuperBinding(false)
        , m_allowsVarDeclarations(true)
        , m_allowsLexicalDeclarations(true)
        , m_strictMode(strictMode)
        , m_isFunction(isFunction)
        , m_isGenerator(isGenerator)
        , m_isGeneratorBoundary(false)
        , m_isArrowFunction(isArrowFunction)
        , m_isArrowFunctionBoundary(false)
        , m_isAsyncFunction(isAsyncFunction)
        , m_isAsyncFunctionBoundary(false)
        , m_isLexicalScope(false)
        , m_isGlobalCodeScope(false)
        , m_isSimpleCatchParameterScope(false)
        , m_isFunctionBoundary(false)
        , m_isValidStrictMode(true)
        , m_hasArguments(false)
        , m_isEvalContext(false)
        , m_hasNonSimpleParameterList(false)
        , m_evalContextType(EvalContextType::None)
        , m_constructorKind(static_cast<unsigned>(ConstructorKind::None))
        , m_expectedSuperBinding(static_cast<unsigned>(SuperBinding::NotNeeded))
        , m_loopDepth(0)
        , m_switchDepth(0)
        , m_innerArrowFunctionFeatures(0)
    {
        m_usedVariables.append(UniquedStringImplPtrSet());
    }

    Scope(Scope&&) = default;

    void startSwitch() { m_switchDepth++; }
    void endSwitch() { m_switchDepth--; }
    void startLoop() { m_loopDepth++; }
    void endLoop() { ASSERT(m_loopDepth); m_loopDepth--; }
    bool inLoop() { return !!m_loopDepth; }
    bool breakIsValid() { return m_loopDepth || m_switchDepth; }
    bool continueIsValid() { return m_loopDepth; }

    void pushLabel(const Identifier* label, bool isLoop)
    {
        if (!m_labels)
            m_labels = makeUnique<LabelStack>();
        m_labels->append(ScopeLabelInfo { label->impl(), isLoop });
    }

    void popLabel()
    {
        ASSERT(m_labels);
        ASSERT(m_labels->size());
        m_labels->removeLast();
    }

    ScopeLabelInfo* getLabel(const Identifier* label)
    {
        if (!m_labels)
            return 0;
        for (int i = m_labels->size(); i > 0; i--) {
            if (m_labels->at(i - 1).uid == label->impl())
                return &m_labels->at(i - 1);
        }
        return 0;
    }

    void setSourceParseMode(SourceParseMode mode)
    {
        switch (mode) {
        case SourceParseMode::AsyncGeneratorBodyMode:
            setIsAsyncGeneratorFunctionBody();
            break;
        case SourceParseMode::AsyncArrowFunctionBodyMode:
            setIsAsyncArrowFunctionBody();
            break;

        case SourceParseMode::AsyncFunctionBodyMode:
            setIsAsyncFunctionBody();
            break;

        case SourceParseMode::GeneratorBodyMode:
            setIsGenerator();
            break;

        case SourceParseMode::GeneratorWrapperFunctionMode:
        case SourceParseMode::GeneratorWrapperMethodMode:
            setIsGeneratorFunction();
            break;

        case SourceParseMode::AsyncGeneratorWrapperMethodMode:
        case SourceParseMode::AsyncGeneratorWrapperFunctionMode:
            setIsAsyncGeneratorFunction();
            break;
    
        case SourceParseMode::NormalFunctionMode:
        case SourceParseMode::GetterMode:
        case SourceParseMode::SetterMode:
        case SourceParseMode::MethodMode:
            setIsFunction();
            break;

        case SourceParseMode::ArrowFunctionMode:
            setIsArrowFunction();
            break;

        case SourceParseMode::AsyncFunctionMode:
        case SourceParseMode::AsyncMethodMode:
            setIsAsyncFunction();
            break;

        case SourceParseMode::AsyncArrowFunctionMode:
            setIsAsyncArrowFunction();
            break;

        case SourceParseMode::ProgramMode:
        case SourceParseMode::ModuleAnalyzeMode:
        case SourceParseMode::ModuleEvaluateMode:
            break;
        }
    }

    bool isFunction() const { return m_isFunction; }
    bool isFunctionBoundary() const { return m_isFunctionBoundary; }
    bool isGenerator() const { return m_isGenerator; }
    bool isGeneratorBoundary() const { return m_isGeneratorBoundary; }
    bool isAsyncFunction() const { return m_isAsyncFunction; }
    bool isAsyncFunctionBoundary() const { return m_isAsyncFunctionBoundary; }

    bool hasArguments() const { return m_hasArguments; }

    void setIsGlobalCodeScope() { m_isGlobalCodeScope = true; }
    bool isGlobalCodeScope() const { return m_isGlobalCodeScope; }

    void setIsSimpleCatchParameterScope() { m_isSimpleCatchParameterScope = true; }
    bool isSimpleCatchParameterScope() { return m_isSimpleCatchParameterScope; }

    void setIsLexicalScope() 
    { 
        m_isLexicalScope = true;
        m_allowsLexicalDeclarations = true;
    }
    bool isLexicalScope() { return m_isLexicalScope; }
    bool usesEval() { return m_usesEval; }

    const HashSet<UniquedStringImpl*>& closedVariableCandidates() const { return m_closedVariableCandidates; }
    VariableEnvironment& declaredVariables() { return m_declaredVariables; }
    VariableEnvironment& lexicalVariables() { return m_lexicalVariables; }
    VariableEnvironment& finalizeLexicalEnvironment() 
    { 
        if (m_usesEval || m_needsFullActivation)
            m_lexicalVariables.markAllVariablesAsCaptured();
        else
            computeLexicallyCapturedVariablesAndPurgeCandidates();

        return m_lexicalVariables;
    }

    void computeLexicallyCapturedVariablesAndPurgeCandidates()
    {
        // Because variables may be defined at any time in the range of a lexical scope, we must
        // track lexical variables that might be captured. Then, when we're preparing to pop the top
        // lexical scope off the stack, we should find which variables are truly captured, and which
        // variable still may be captured in a parent scope.
        if (m_lexicalVariables.size() && m_closedVariableCandidates.size()) {
            for (UniquedStringImpl* impl : m_closedVariableCandidates)
                m_lexicalVariables.markVariableAsCapturedIfDefined(impl);
        }

        // We can now purge values from the captured candidates because they're captured in this scope.
        {
            for (auto entry : m_lexicalVariables) {
                if (entry.value.isCaptured())
                    m_closedVariableCandidates.remove(entry.key.get());
            }
        }
    }

    DeclarationResultMask declareCallee(const Identifier* ident)
    {
        auto addResult = m_declaredVariables.add(ident->impl());
        // We want to track if callee is captured, but we don't want to act like it's a 'var'
        // because that would cause the BytecodeGenerator to emit bad code.
        addResult.iterator->value.clearIsVar();

        DeclarationResultMask result = DeclarationResult::Valid;
        if (isEvalOrArgumentsIdentifier(m_vm, ident))
            result |= DeclarationResult::InvalidStrictMode;
        return result;
    }

    DeclarationResultMask declareVariable(const Identifier* ident)
    {
        ASSERT(m_allowsVarDeclarations);
        DeclarationResultMask result = DeclarationResult::Valid;
        bool isValidStrictMode = !isEvalOrArgumentsIdentifier(m_vm, ident);
        m_isValidStrictMode = m_isValidStrictMode && isValidStrictMode;
        auto addResult = m_declaredVariables.add(ident->impl());
        addResult.iterator->value.setIsVar();
        if (!isValidStrictMode)
            result |= DeclarationResult::InvalidStrictMode;
        return result;
    }

    DeclarationResultMask declareFunction(const Identifier* ident, bool declareAsVar, bool isSloppyModeHoistingCandidate)
    {
        ASSERT(m_allowsVarDeclarations || m_allowsLexicalDeclarations);
        DeclarationResultMask result = DeclarationResult::Valid;
        bool isValidStrictMode = !isEvalOrArgumentsIdentifier(m_vm, ident);
        if (!isValidStrictMode)
            result |= DeclarationResult::InvalidStrictMode;
        m_isValidStrictMode = m_isValidStrictMode && isValidStrictMode;
        auto addResult = declareAsVar ? m_declaredVariables.add(ident->impl()) : m_lexicalVariables.add(ident->impl());
        if (isSloppyModeHoistingCandidate)
            addResult.iterator->value.setIsSloppyModeHoistingCandidate();
        if (declareAsVar) {
            addResult.iterator->value.setIsVar();
            if (m_lexicalVariables.contains(ident->impl()))
                result |= DeclarationResult::InvalidDuplicateDeclaration;
        } else {
            addResult.iterator->value.setIsLet();
            ASSERT_WITH_MESSAGE(!m_declaredVariables.size(), "We should only declare a function as a lexically scoped variable in scopes where var declarations aren't allowed. I.e, in strict mode and not at the top-level scope of a function or program.");
            if (!addResult.isNewEntry) {
                if (!isSloppyModeHoistingCandidate || !addResult.iterator->value.isFunction())
                    result |= DeclarationResult::InvalidDuplicateDeclaration;
            }
        }

        addResult.iterator->value.setIsFunction();

        return result;
    }

    void addVariableBeingHoisted(const Identifier* ident)
    {
        ASSERT(!m_allowsVarDeclarations);
        m_variablesBeingHoisted.add(ident->impl());
    }

    void addSloppyModeHoistableFunctionCandidate(const Identifier* ident)
    {
        ASSERT(m_allowsVarDeclarations);
        m_sloppyModeHoistableFunctionCandidates.add(ident->impl());
    }

    void appendFunction(FunctionMetadataNode* node)
    { 
        ASSERT(node);
        m_functionDeclarations.append(node);
    }
    DeclarationStacks::FunctionStack&& takeFunctionDeclarations() { return WTFMove(m_functionDeclarations); }
    

    DeclarationResultMask declareLexicalVariable(const Identifier* ident, bool isConstant, DeclarationImportType importType = DeclarationImportType::NotImported)
    {
        ASSERT(m_allowsLexicalDeclarations);
        DeclarationResultMask result = DeclarationResult::Valid;
        bool isValidStrictMode = !isEvalOrArgumentsIdentifier(m_vm, ident);
        m_isValidStrictMode = m_isValidStrictMode && isValidStrictMode;
        auto addResult = m_lexicalVariables.add(ident->impl());
        if (isConstant)
            addResult.iterator->value.setIsConst();
        else
            addResult.iterator->value.setIsLet();

        if (importType == DeclarationImportType::Imported)
            addResult.iterator->value.setIsImported();
        else if (importType == DeclarationImportType::ImportedNamespace) {
            addResult.iterator->value.setIsImported();
            addResult.iterator->value.setIsImportedNamespace();
        }

        if (!addResult.isNewEntry || m_variablesBeingHoisted.contains(ident->impl()))
            result |= DeclarationResult::InvalidDuplicateDeclaration;
        if (!isValidStrictMode)
            result |= DeclarationResult::InvalidStrictMode;

        return result;
    }

    ALWAYS_INLINE bool hasDeclaredVariable(const Identifier& ident)
    {
        return hasDeclaredVariable(ident.impl());
    }

    bool hasDeclaredVariable(const RefPtr<UniquedStringImpl>& ident)
    {
        auto iter = m_declaredVariables.find(ident.get());
        if (iter == m_declaredVariables.end())
            return false;
        VariableEnvironmentEntry entry = iter->value;
        return entry.isVar(); // The callee isn't a "var".
    }

    ALWAYS_INLINE bool hasLexicallyDeclaredVariable(const Identifier& ident)
    {
        return hasLexicallyDeclaredVariable(ident.impl());
    }

    bool hasLexicallyDeclaredVariable(const RefPtr<UniquedStringImpl>& ident) const
    {
        return m_lexicalVariables.contains(ident.get());
    }
    
    ALWAYS_INLINE bool hasDeclaredParameter(const Identifier& ident)
    {
        return hasDeclaredParameter(ident.impl());
    }

    bool hasDeclaredParameter(const RefPtr<UniquedStringImpl>& ident)
    {
        return m_declaredParameters.contains(ident.get()) || hasDeclaredVariable(ident);
    }
    
    void preventAllVariableDeclarations()
    {
        m_allowsVarDeclarations = false; 
        m_allowsLexicalDeclarations = false;
    }
    void preventVarDeclarations() { m_allowsVarDeclarations = false; }
    bool allowsVarDeclarations() const { return m_allowsVarDeclarations; }
    bool allowsLexicalDeclarations() const { return m_allowsLexicalDeclarations; }

    DeclarationResultMask declareParameter(const Identifier* ident)
    {
        ASSERT(m_allowsVarDeclarations);
        DeclarationResultMask result = DeclarationResult::Valid;
        bool isArgumentsIdent = isArguments(m_vm, ident);
        auto addResult = m_declaredVariables.add(ident->impl());
        bool isValidStrictMode = (addResult.isNewEntry || !addResult.iterator->value.isParameter())
            && m_vm->propertyNames->eval != *ident && !isArgumentsIdent;
        addResult.iterator->value.clearIsVar();
        addResult.iterator->value.setIsParameter();
        m_isValidStrictMode = m_isValidStrictMode && isValidStrictMode;
        m_declaredParameters.add(ident->impl());
        if (!isValidStrictMode)
            result |= DeclarationResult::InvalidStrictMode;
        if (isArgumentsIdent)
            m_shadowsArguments = true;
        if (!addResult.isNewEntry)
            result |= DeclarationResult::InvalidDuplicateDeclaration;

        return result;
    }
    
    bool usedVariablesContains(UniquedStringImpl* impl) const
    { 
        for (const UniquedStringImplPtrSet& set : m_usedVariables) {
            if (set.contains(impl))
                return true;
        }
        return false;
    }
    template <typename Func>
    void forEachUsedVariable(const Func& func)
    {
        for (const UniquedStringImplPtrSet& set : m_usedVariables) {
            for (UniquedStringImpl* impl : set)
                func(impl);
        }
    }
    void useVariable(const Identifier* ident, bool isEval)
    {
        useVariable(ident->impl(), isEval);
    }
    void useVariable(UniquedStringImpl* impl, bool isEval)
    {
        m_usesEval |= isEval;
        m_usedVariables.last().add(impl);
    }

    void pushUsedVariableSet() { m_usedVariables.append(UniquedStringImplPtrSet()); }
    size_t currentUsedVariablesSize() { return m_usedVariables.size(); }

    void revertToPreviousUsedVariables(size_t size) { m_usedVariables.resize(size); }

    void setNeedsFullActivation() { m_needsFullActivation = true; }
    bool needsFullActivation() const { return m_needsFullActivation; }
    bool isArrowFunctionBoundary() { return m_isArrowFunctionBoundary; }
    bool isArrowFunction() { return m_isArrowFunction; }

    bool hasDirectSuper() const { return m_hasDirectSuper; }
    bool setHasDirectSuper() { return std::exchange(m_hasDirectSuper, true); }

    bool needsSuperBinding() const { return m_needsSuperBinding; }
    bool setNeedsSuperBinding() { return std::exchange(m_needsSuperBinding, true); }
    
    void setEvalContextType(EvalContextType evalContextType) { m_evalContextType = evalContextType; }
    EvalContextType evalContextType() { return m_evalContextType; }
    
    InnerArrowFunctionCodeFeatures innerArrowFunctionFeatures() { return m_innerArrowFunctionFeatures; }
    
    void setExpectedSuperBinding(SuperBinding superBinding) { m_expectedSuperBinding = static_cast<unsigned>(superBinding); }
    SuperBinding expectedSuperBinding() const { return static_cast<SuperBinding>(m_expectedSuperBinding); }
    void setConstructorKind(ConstructorKind constructorKind) { m_constructorKind = static_cast<unsigned>(constructorKind); }
    ConstructorKind constructorKind() const { return static_cast<ConstructorKind>(m_constructorKind); }

    void setInnerArrowFunctionUsesSuperCall() { m_innerArrowFunctionFeatures |= SuperCallInnerArrowFunctionFeature; }
    void setInnerArrowFunctionUsesSuperProperty() { m_innerArrowFunctionFeatures |= SuperPropertyInnerArrowFunctionFeature; }
    void setInnerArrowFunctionUsesEval() { m_innerArrowFunctionFeatures |= EvalInnerArrowFunctionFeature; }
    void setInnerArrowFunctionUsesThis() { m_innerArrowFunctionFeatures |= ThisInnerArrowFunctionFeature; }
    void setInnerArrowFunctionUsesNewTarget() { m_innerArrowFunctionFeatures |= NewTargetInnerArrowFunctionFeature; }
    void setInnerArrowFunctionUsesArguments() { m_innerArrowFunctionFeatures |= ArgumentsInnerArrowFunctionFeature; }
    
    bool isEvalContext() const { return m_isEvalContext; }
    void setIsEvalContext(bool isEvalContext) { m_isEvalContext = isEvalContext; }

    void setInnerArrowFunctionUsesEvalAndUseArgumentsIfNeeded()
    {
        ASSERT(m_isArrowFunction);

        if (m_usesEval)
            setInnerArrowFunctionUsesEval();
        
        if (usedVariablesContains(m_vm->propertyNames->arguments.impl()))
            setInnerArrowFunctionUsesArguments();
    }

    void addClosedVariableCandidateUnconditionally(UniquedStringImpl* impl)
    {
        m_closedVariableCandidates.add(impl);
    }
    
    void collectFreeVariables(Scope* nestedScope, bool shouldTrackClosedVariables)
    {
        if (nestedScope->m_usesEval)
            m_usesEval = true;

        {
            UniquedStringImplPtrSet& destinationSet = m_usedVariables.last();
            for (const UniquedStringImplPtrSet& usedVariablesSet : nestedScope->m_usedVariables) {
                for (UniquedStringImpl* impl : usedVariablesSet) {
                    if (nestedScope->m_declaredVariables.contains(impl) || nestedScope->m_lexicalVariables.contains(impl))
                        continue;

                    // "arguments" reference should be resolved at function boudary.
                    if (nestedScope->isFunctionBoundary() && nestedScope->hasArguments() && impl == m_vm->propertyNames->arguments.impl() && !nestedScope->isArrowFunctionBoundary())
                        continue;

                    destinationSet.add(impl);
                    // We don't want a declared variable that is used in an inner scope to be thought of as captured if
                    // that inner scope is both a lexical scope and not a function. Only inner functions and "catch" 
                    // statements can cause variables to be captured.
                    if (shouldTrackClosedVariables && (nestedScope->m_isFunctionBoundary || !nestedScope->m_isLexicalScope))
                        m_closedVariableCandidates.add(impl);
                }
            }
        }
        // Propagate closed variable candidates downwards within the same function.
        // Cross function captures will be realized via m_usedVariables propagation.
        if (shouldTrackClosedVariables && !nestedScope->m_isFunctionBoundary && nestedScope->m_closedVariableCandidates.size()) {
            auto end = nestedScope->m_closedVariableCandidates.end();
            auto begin = nestedScope->m_closedVariableCandidates.begin();
            m_closedVariableCandidates.add(begin, end);
        }
    }
    
    void mergeInnerArrowFunctionFeatures(InnerArrowFunctionCodeFeatures arrowFunctionCodeFeatures)
    {
        m_innerArrowFunctionFeatures = m_innerArrowFunctionFeatures | arrowFunctionCodeFeatures;
    }
    
    void getSloppyModeHoistedFunctions(UniquedStringImplPtrSet& sloppyModeHoistedFunctions)
    {
        for (UniquedStringImpl* function : m_sloppyModeHoistableFunctionCandidates) {
            // ES6 Annex B.3.3. The only time we can't hoist a function is if a syntax error would
            // be caused by declaring a var with that function's name or if we have a parameter with
            // that function's name. Note that we would only cause a syntax error if we had a let/const/class
            // variable with the same name.
            if (!m_lexicalVariables.contains(function)) {
                auto iter = m_declaredVariables.find(function);
                bool isParameter = iter != m_declaredVariables.end() && iter->value.isParameter();
                if (!isParameter) {
                    auto addResult = m_declaredVariables.add(function);
                    addResult.iterator->value.setIsVar();
                    addResult.iterator->value.setIsSloppyModeHoistingCandidate();
                    sloppyModeHoistedFunctions.add(function);
                }
            }
        }
    }

    void getCapturedVars(IdentifierSet& capturedVariables)
    {
        if (m_needsFullActivation || m_usesEval) {
            for (auto& entry : m_declaredVariables)
                capturedVariables.add(entry.key);
            return;
        }
        for (UniquedStringImpl* impl : m_closedVariableCandidates) {
            // We refer to m_declaredVariables here directly instead of a hasDeclaredVariable because we want to mark the callee as captured.
            if (!m_declaredVariables.contains(impl)) 
                continue;
            capturedVariables.add(impl);
        }
    }
    void setStrictMode() { m_strictMode = true; }
    bool strictMode() const { return m_strictMode; }
    bool isValidStrictMode() const { return m_isValidStrictMode; }
    bool shadowsArguments() const { return m_shadowsArguments; }
    void setHasNonSimpleParameterList()
    {
        m_isValidStrictMode = false;
        m_hasNonSimpleParameterList = true;
    }
    bool hasNonSimpleParameterList() const { return m_hasNonSimpleParameterList; }

    void copyCapturedVariablesToVector(const UniquedStringImplPtrSet& usedVariables, Vector<UniquedStringImpl*, 8>& vector)
    {
        for (UniquedStringImpl* impl : usedVariables) {
            if (m_declaredVariables.contains(impl) || m_lexicalVariables.contains(impl))
                continue;
            vector.append(impl);
        }
    }

    void fillParametersForSourceProviderCache(SourceProviderCacheItemCreationParameters& parameters, const UniquedStringImplPtrSet& capturesFromParameterExpressions)
    {
        ASSERT(m_isFunction);
        parameters.usesEval = m_usesEval;
        parameters.strictMode = m_strictMode;
        parameters.needsFullActivation = m_needsFullActivation;
        parameters.innerArrowFunctionFeatures = m_innerArrowFunctionFeatures;
        parameters.needsSuperBinding = m_needsSuperBinding;
        for (const UniquedStringImplPtrSet& set : m_usedVariables)
            copyCapturedVariablesToVector(set, parameters.usedVariables);

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=156962
        // We add these unconditionally because we currently don't keep a separate
        // declaration scope for a function's parameters and its var/let/const declarations.
        // This is somewhat unfortunate and we should refactor to do this at some point
        // because parameters logically form a parent scope to var/let/const variables.
        // But because we don't do this, we must grab capture candidates from a parameter
        // list before we parse the body of a function because the body's declarations
        // might make us believe something isn't actually a capture candidate when it really
        // is.
        for (UniquedStringImpl* impl : capturesFromParameterExpressions)
            parameters.usedVariables.append(impl);
    }

    void restoreFromSourceProviderCache(const SourceProviderCacheItem* info)
    {
        ASSERT(m_isFunction);
        m_usesEval = info->usesEval;
        m_strictMode = info->strictMode;
        m_innerArrowFunctionFeatures = info->innerArrowFunctionFeatures;
        m_needsFullActivation = info->needsFullActivation;
        m_needsSuperBinding = info->needsSuperBinding;
        UniquedStringImplPtrSet& destSet = m_usedVariables.last();
        for (unsigned i = 0; i < info->usedVariablesCount; ++i)
            destSet.add(info->usedVariables()[i]);
    }

    class MaybeParseAsGeneratorForScope;

private:
    void setIsFunction()
    {
        m_isFunction = true;
        m_isFunctionBoundary = true;
        m_hasArguments = true;
        setIsLexicalScope();
        m_isGenerator = false;
        m_isGeneratorBoundary = false;
        m_isArrowFunctionBoundary = false;
        m_isArrowFunction = false;
        m_isAsyncFunction = false;
        m_isAsyncFunctionBoundary = false;
    }

    void setIsGeneratorFunction()
    {
        setIsFunction();
        m_isGenerator = true;
    }

    void setIsGenerator()
    {
        setIsFunction();
        m_isGenerator = true;
        m_isGeneratorBoundary = true;
        m_hasArguments = false;
    }
    
    void setIsArrowFunction()
    {
        setIsFunction();
        m_isArrowFunctionBoundary = true;
        m_isArrowFunction = true;
    }

    void setIsAsyncArrowFunction()
    {
        setIsArrowFunction();
        m_isAsyncFunction = true;
    }

    void setIsAsyncFunction()
    {
        setIsFunction();
        m_isAsyncFunction = true;
    }

    void setIsAsyncGeneratorFunction()
    {
        setIsFunction();
        m_isAsyncFunction = true;
        m_isGenerator = true;
    }

    void setIsAsyncGeneratorFunctionBody()
    {
        setIsFunction();
        m_hasArguments = false;
        m_isGenerator = true;
        m_isGeneratorBoundary = true;
        m_isAsyncFunction = true;
        m_isAsyncFunctionBoundary = true;
    }

    void setIsAsyncFunctionBody()
    {
        setIsFunction();
        m_hasArguments = false;
        m_isAsyncFunction = true;
        m_isAsyncFunctionBoundary = true;
    }

    void setIsAsyncArrowFunctionBody()
    {
        setIsArrowFunction();
        m_hasArguments = false;
        m_isAsyncFunction = true;
        m_isAsyncFunctionBoundary = true;
    }

    const VM* m_vm;
    bool m_shadowsArguments;
    bool m_usesEval;
    bool m_needsFullActivation;
    bool m_hasDirectSuper;
    bool m_needsSuperBinding;
    bool m_allowsVarDeclarations;
    bool m_allowsLexicalDeclarations;
    bool m_strictMode;
    bool m_isFunction;
    bool m_isGenerator;
    bool m_isGeneratorBoundary;
    bool m_isArrowFunction;
    bool m_isArrowFunctionBoundary;
    bool m_isAsyncFunction;
    bool m_isAsyncFunctionBoundary;
    bool m_isLexicalScope;
    bool m_isGlobalCodeScope;
    bool m_isSimpleCatchParameterScope;
    bool m_isFunctionBoundary;
    bool m_isValidStrictMode;
    bool m_hasArguments;
    bool m_isEvalContext;
    bool m_hasNonSimpleParameterList;
    EvalContextType m_evalContextType;
    unsigned m_constructorKind;
    unsigned m_expectedSuperBinding;
    int m_loopDepth;
    int m_switchDepth;
    InnerArrowFunctionCodeFeatures m_innerArrowFunctionFeatures;

    typedef Vector<ScopeLabelInfo, 2> LabelStack;
    std::unique_ptr<LabelStack> m_labels;
    UniquedStringImplPtrSet m_declaredParameters;
    VariableEnvironment m_declaredVariables;
    VariableEnvironment m_lexicalVariables;
    Vector<UniquedStringImplPtrSet, 6> m_usedVariables;
    UniquedStringImplPtrSet m_variablesBeingHoisted;
    UniquedStringImplPtrSet m_sloppyModeHoistableFunctionCandidates;
    HashSet<UniquedStringImpl*> m_closedVariableCandidates;
    DeclarationStacks::FunctionStack m_functionDeclarations;
};

typedef Vector<Scope, 10> ScopeStack;

struct ScopeRef {
    ScopeRef(ScopeStack* scopeStack, unsigned index)
        : m_scopeStack(scopeStack)
        , m_index(index)
    {
    }
    Scope* operator->() { return &m_scopeStack->at(m_index); }
    unsigned index() const { return m_index; }

    bool hasContainingScope()
    {
        return m_index && !m_scopeStack->at(m_index).isFunctionBoundary();
    }

    ScopeRef containingScope()
    {
        ASSERT(hasContainingScope());
        return ScopeRef(m_scopeStack, m_index - 1);
    }

    bool operator==(const ScopeRef& other)
    {
        ASSERT(other.m_scopeStack == m_scopeStack);
        return m_index == other.m_index;
    }

    bool operator!=(const ScopeRef& other)
    {
        return !(*this == other);
    }

private:
    ScopeStack* m_scopeStack;
    unsigned m_index;
};

enum class ArgumentType { Normal, Spread };
enum class ParsingContext { Program, FunctionConstructor, Eval };

template <typename LexerType>
class Parser {
    WTF_MAKE_NONCOPYABLE(Parser);
    WTF_MAKE_FAST_ALLOCATED;

public:
    Parser(VM*, const SourceCode&, JSParserBuiltinMode, JSParserStrictMode, JSParserScriptMode, SourceParseMode, SuperBinding, ConstructorKind defaultConstructorKind = ConstructorKind::None, DerivedContextType = DerivedContextType::None, bool isEvalContext = false, EvalContextType = EvalContextType::None, DebuggerParseData* = nullptr);
    ~Parser();

    template <class ParsedNode>
    std::unique_ptr<ParsedNode> parse(ParserError&, const Identifier&, SourceParseMode, ParsingContext, Optional<int> functionConstructorParametersEndPosition = WTF::nullopt);

    JSTextPosition positionBeforeLastNewline() const { return m_lexer->positionBeforeLastNewline(); }
    JSTokenLocation locationBeforeLastToken() const { return m_lexer->lastTokenLocation(); }

    struct CallOrApplyDepthScope {
        CallOrApplyDepthScope(Parser* parser)
            : m_parser(parser)
            , m_parent(parser->m_callOrApplyDepthScope)
            , m_depth(m_parent ? m_parent->m_depth + 1 : 0)
            , m_depthOfInnermostChild(m_depth)
        {
            parser->m_callOrApplyDepthScope = this;
        }

        size_t distanceToInnermostChild() const
        {
            ASSERT(m_depthOfInnermostChild >= m_depth);
            return m_depthOfInnermostChild - m_depth;
        }

        ~CallOrApplyDepthScope()
        {
            if (m_parent)
                m_parent->m_depthOfInnermostChild = std::max(m_depthOfInnermostChild, m_parent->m_depthOfInnermostChild);
            m_parser->m_callOrApplyDepthScope = m_parent;
        }

    private:

        Parser* m_parser;
        CallOrApplyDepthScope* m_parent;
        size_t m_depth;
        size_t m_depthOfInnermostChild;
    };

private:
    struct AllowInOverride {
        AllowInOverride(Parser* parser)
            : m_parser(parser)
            , m_oldAllowsIn(parser->m_allowsIn)
        {
            parser->m_allowsIn = true;
        }
        ~AllowInOverride()
        {
            m_parser->m_allowsIn = m_oldAllowsIn;
        }
        Parser* m_parser;
        bool m_oldAllowsIn;
    };

    struct AutoPopScopeRef : public ScopeRef {
        AutoPopScopeRef(Parser* parser, ScopeRef scope)
        : ScopeRef(scope)
        , m_parser(parser)
        {
        }
        
        ~AutoPopScopeRef()
        {
            if (m_parser)
                m_parser->popScope(*this, false);
        }
        
        void setPopped()
        {
            m_parser = 0;
        }
        
    private:
        Parser* m_parser;
    };

    struct AutoCleanupLexicalScope {
        // We can allocate this object on the stack without actually knowing beforehand if we're 
        // going to create a new lexical scope. If we decide to create a new lexical scope, we
        // can pass the scope into this obejct and it will take care of the cleanup for us if the parse fails.
        // This is helpful if we may fail from syntax errors after creating a lexical scope conditionally.
        AutoCleanupLexicalScope()
            : m_scope(nullptr, UINT_MAX)
            , m_parser(nullptr)
        {
        }

        ~AutoCleanupLexicalScope()
        {
            // This should only ever be called if we fail from a syntax error. Otherwise
            // it's the intention that a user of this class pops this scope manually on a 
            // successful parse. 
            if (isValid())
                m_parser->popScope(*this, false);
        }

        void setIsValid(ScopeRef& scope, Parser* parser)
        {
            RELEASE_ASSERT(scope->isLexicalScope());
            m_scope = scope;
            m_parser = parser;
        }

        bool isValid() const { return !!m_parser; }

        void setPopped()
        {
            m_parser = nullptr;
        }

        ScopeRef& scope() { return m_scope; }

    private:
        ScopeRef m_scope;
        Parser* m_parser;
    };

    enum ExpressionErrorClass {
        ErrorIndicatesNothing = 0,
        ErrorIndicatesPattern,
        ErrorIndicatesAsyncArrowFunction
    };

    struct ExpressionErrorClassifier {
        ExpressionErrorClassifier(Parser* parser)
            : m_class(ErrorIndicatesNothing)
            , m_previous(parser->m_expressionErrorClassifier)
            , m_parser(parser)
        {
            m_parser->m_expressionErrorClassifier = this;
        }

        ~ExpressionErrorClassifier()
        {
            m_parser->m_expressionErrorClassifier = m_previous;
        }

        void classifyExpressionError(ExpressionErrorClass classification)
        {
            if (m_class != ErrorIndicatesNothing)
                return;
            m_class = classification;
        }

        void forceClassifyExpressionError(ExpressionErrorClass classification)
        {
            m_class = classification;
        }

        void reclassifyExpressionError(ExpressionErrorClass oldClassification, ExpressionErrorClass classification)
        {
            if (m_class != oldClassification)
                return;
            m_class = classification;
        }

        void propagateExpressionErrorClass()
        {
            if (m_previous)
                m_previous->m_class = m_class;
        }

        bool indicatesPossiblePattern() const { return m_class == ErrorIndicatesPattern; }
        bool indicatesPossibleAsyncArrowFunction() const { return m_class == ErrorIndicatesAsyncArrowFunction; }

    private:
        ExpressionErrorClass m_class;
        ExpressionErrorClassifier* m_previous;
        Parser* m_parser;
    };

    ALWAYS_INLINE void classifyExpressionError(ExpressionErrorClass classification)
    {
        if (m_expressionErrorClassifier)
            m_expressionErrorClassifier->classifyExpressionError(classification);
    }

    ALWAYS_INLINE void forceClassifyExpressionError(ExpressionErrorClass classification)
    {
        if (m_expressionErrorClassifier)
            m_expressionErrorClassifier->forceClassifyExpressionError(classification);
    }

    ALWAYS_INLINE void reclassifyExpressionError(ExpressionErrorClass oldClassification, ExpressionErrorClass classification)
    {
        if (m_expressionErrorClassifier)
            m_expressionErrorClassifier->reclassifyExpressionError(oldClassification, classification);
    }

    ALWAYS_INLINE DestructuringKind destructuringKindFromDeclarationType(DeclarationType type)
    {
        switch (type) {
        case DeclarationType::VarDeclaration:
            return DestructuringKind::DestructureToVariables;
        case DeclarationType::LetDeclaration:
            return DestructuringKind::DestructureToLet;
        case DeclarationType::ConstDeclaration:
            return DestructuringKind::DestructureToConst;
        }

        RELEASE_ASSERT_NOT_REACHED();
        return DestructuringKind::DestructureToVariables;
    }

    ALWAYS_INLINE const char* declarationTypeToVariableKind(DeclarationType type)
    {
        switch (type) {
        case DeclarationType::VarDeclaration:
            return "variable name";
        case DeclarationType::LetDeclaration:
        case DeclarationType::ConstDeclaration:
            return "lexical variable name";
        }
        RELEASE_ASSERT_NOT_REACHED();
        return "invalid";
    }

    ALWAYS_INLINE AssignmentContext assignmentContextFromDeclarationType(DeclarationType type)
    {
        switch (type) {
        case DeclarationType::ConstDeclaration:
            return AssignmentContext::ConstDeclarationStatement;
        default:
            return AssignmentContext::DeclarationStatement;
        }
    }

    ALWAYS_INLINE bool isEvalOrArguments(const Identifier* ident) { return isEvalOrArgumentsIdentifier(m_vm, ident); }

    ScopeRef upperScope(int n)
    {
        ASSERT(m_scopeStack.size() >= size_t(1 + n));
        return ScopeRef(&m_scopeStack, m_scopeStack.size() - 1 - n);
    }

    ScopeRef currentScope()
    {
        return ScopeRef(&m_scopeStack, m_scopeStack.size() - 1);
    }

    ScopeRef currentVariableScope()
    {
        unsigned i = m_scopeStack.size() - 1;
        ASSERT(i < m_scopeStack.size());
        while (!m_scopeStack[i].allowsVarDeclarations()) {
            i--;
            ASSERT(i < m_scopeStack.size());
        }
        return ScopeRef(&m_scopeStack, i);
    }

    ScopeRef currentLexicalDeclarationScope()
    {
        unsigned i = m_scopeStack.size() - 1;
        ASSERT(i < m_scopeStack.size());
        while (!m_scopeStack[i].allowsLexicalDeclarations()) {
            i--;
            ASSERT(i < m_scopeStack.size());
        }

        return ScopeRef(&m_scopeStack, i);
    }

    ScopeRef currentFunctionScope()
    {
        unsigned i = m_scopeStack.size() - 1;
        ASSERT(i < m_scopeStack.size());
        while (i && !m_scopeStack[i].isFunctionBoundary()) {
            i--;
            ASSERT(i < m_scopeStack.size());
        }
        // When reaching the top level scope (it can be non function scope), we return it.
        return ScopeRef(&m_scopeStack, i);
    }

    ScopeRef closestParentOrdinaryFunctionNonLexicalScope()
    {
        unsigned i = m_scopeStack.size() - 1;
        ASSERT(i < m_scopeStack.size() && m_scopeStack.size());
        while (i && (!m_scopeStack[i].isFunctionBoundary() || m_scopeStack[i].isGeneratorBoundary() || m_scopeStack[i].isAsyncFunctionBoundary() || m_scopeStack[i].isArrowFunctionBoundary()))
            i--;
        // When reaching the top level scope (it can be non ordinary function scope), we return it.
        return ScopeRef(&m_scopeStack, i);
    }
    
    ScopeRef pushScope()
    {
        bool isFunction = false;
        bool isStrict = false;
        bool isGenerator = false;
        bool isArrowFunction = false;
        bool isAsyncFunction = false;
        if (!m_scopeStack.isEmpty()) {
            isStrict = m_scopeStack.last().strictMode();
            isFunction = m_scopeStack.last().isFunction();
            isGenerator = m_scopeStack.last().isGenerator();
            isArrowFunction = m_scopeStack.last().isArrowFunction();
            isAsyncFunction = m_scopeStack.last().isAsyncFunction();
        }
        m_scopeStack.constructAndAppend(m_vm, isFunction, isGenerator, isStrict, isArrowFunction, isAsyncFunction);
        return currentScope();
    }

    void popScopeInternal(ScopeRef& scope, bool shouldTrackClosedVariables)
    {
        EXCEPTION_ASSERT_UNUSED(scope, scope.index() == m_scopeStack.size() - 1);
        ASSERT(m_scopeStack.size() > 1);
        m_scopeStack[m_scopeStack.size() - 2].collectFreeVariables(&m_scopeStack.last(), shouldTrackClosedVariables);
        
        if (m_scopeStack.last().isArrowFunction())
            m_scopeStack.last().setInnerArrowFunctionUsesEvalAndUseArgumentsIfNeeded();
        
        if (!(m_scopeStack.last().isFunctionBoundary() && !m_scopeStack.last().isArrowFunctionBoundary()))
            m_scopeStack[m_scopeStack.size() - 2].mergeInnerArrowFunctionFeatures(m_scopeStack.last().innerArrowFunctionFeatures());

        if (!m_scopeStack.last().isFunctionBoundary() && m_scopeStack.last().needsFullActivation())
            m_scopeStack[m_scopeStack.size() - 2].setNeedsFullActivation();
        m_scopeStack.removeLast();
    }
    
    ALWAYS_INLINE void popScope(ScopeRef& scope, bool shouldTrackClosedVariables)
    {
        popScopeInternal(scope, shouldTrackClosedVariables);
    }
    
    ALWAYS_INLINE void popScope(AutoPopScopeRef& scope, bool shouldTrackClosedVariables)
    {
        scope.setPopped();
        popScopeInternal(scope, shouldTrackClosedVariables);
    }

    ALWAYS_INLINE void popScope(AutoCleanupLexicalScope& cleanupScope, bool shouldTrackClosedVariables)
    {
        RELEASE_ASSERT(cleanupScope.isValid());
        ScopeRef& scope = cleanupScope.scope();
        cleanupScope.setPopped();
        popScopeInternal(scope, shouldTrackClosedVariables);
    }

    NEVER_INLINE DeclarationResultMask declareHoistedVariable(const Identifier* ident)
    {
        unsigned i = m_scopeStack.size() - 1;
        ASSERT(i < m_scopeStack.size());
        while (true) {
            // Annex B.3.5 exempts `try {} catch (e) { var e; }` from being a syntax error.
            if (m_scopeStack[i].hasLexicallyDeclaredVariable(*ident) && !m_scopeStack[i].isSimpleCatchParameterScope())
                return DeclarationResult::InvalidDuplicateDeclaration;

            if (m_scopeStack[i].allowsVarDeclarations())
                return m_scopeStack[i].declareVariable(ident);

            m_scopeStack[i].addVariableBeingHoisted(ident);

            i--;
            ASSERT(i < m_scopeStack.size());
        }
    }
    
    DeclarationResultMask declareVariable(const Identifier* ident, DeclarationType type = DeclarationType::VarDeclaration, DeclarationImportType importType = DeclarationImportType::NotImported)
    {
        if (type == DeclarationType::VarDeclaration)
            return declareHoistedVariable(ident);

        ASSERT(type == DeclarationType::LetDeclaration || type == DeclarationType::ConstDeclaration);
        // Lexical variables declared at a top level scope that shadow arguments or vars are not allowed.
        if (!m_lexer->isReparsingFunction() && m_statementDepth == 1 && (hasDeclaredParameter(*ident) || hasDeclaredVariable(*ident)))
            return DeclarationResult::InvalidDuplicateDeclaration;

        return currentLexicalDeclarationScope()->declareLexicalVariable(ident, type == DeclarationType::ConstDeclaration, importType);
    }

    std::pair<DeclarationResultMask, ScopeRef> declareFunction(const Identifier* ident)
    {
        if ((m_statementDepth == 1) || (!strictMode() && !currentScope()->isFunction() && !closestParentOrdinaryFunctionNonLexicalScope()->isEvalContext())) {
            // Functions declared at the top-most scope (both in sloppy and strict mode) are declared as vars
            // for backwards compatibility. This allows us to declare functions with the same name more than once.
            // In sloppy mode, we always declare functions as vars.
            bool declareAsVar = true;
            bool isSloppyModeHoistingCandidate = false;
            ScopeRef variableScope = currentVariableScope();
            return std::make_pair(variableScope->declareFunction(ident, declareAsVar, isSloppyModeHoistingCandidate), variableScope);
        }

        if (!strictMode()) {
            ASSERT(currentScope()->isFunction() || closestParentOrdinaryFunctionNonLexicalScope()->isEvalContext());

            // Functions declared inside a function inside a nested block scope in sloppy mode are subject to this
            // crazy rule defined inside Annex B.3.3 in the ES6 spec. It basically states that we will create
            // the function as a local block scoped variable, but when we evaluate the block that the function is
            // contained in, we will assign the function to a "var" variable only if declaring such a "var" wouldn't
            // be a syntax error and if there isn't a parameter with the same name. (It would only be a syntax error if
            // there are is a let/class/const with the same name). Note that this mean we only do the "var" hoisting 
            // binding if the block evaluates. For example, this means we wont won't perform the binding if it's inside
            // the untaken branch of an if statement.
            bool declareAsVar = false;
            bool isSloppyModeHoistingCandidate = true;
            ScopeRef lexicalVariableScope = currentLexicalDeclarationScope();
            ScopeRef varScope = currentVariableScope();
            varScope->addSloppyModeHoistableFunctionCandidate(ident);
            ASSERT(varScope != lexicalVariableScope);
            return std::make_pair(lexicalVariableScope->declareFunction(ident, declareAsVar, isSloppyModeHoistingCandidate), lexicalVariableScope);
        }

        bool declareAsVar = false;
        bool isSloppyModeHoistingCandidate = false;
        ScopeRef lexicalVariableScope = currentLexicalDeclarationScope();
        return std::make_pair(lexicalVariableScope->declareFunction(ident, declareAsVar, isSloppyModeHoistingCandidate), lexicalVariableScope);
    }

    NEVER_INLINE bool hasDeclaredVariable(const Identifier& ident)
    {
        unsigned i = m_scopeStack.size() - 1;
        ASSERT(i < m_scopeStack.size());
        while (!m_scopeStack[i].allowsVarDeclarations()) {
            i--;
            ASSERT(i < m_scopeStack.size());
        }
        return m_scopeStack[i].hasDeclaredVariable(ident);
    }

    NEVER_INLINE bool hasDeclaredParameter(const Identifier& ident)
    {
        // FIXME: hasDeclaredParameter() is not valid during reparsing of generator or async function bodies, because their formal
        // parameters are declared in a scope unavailable during reparsing. Note that it is redundant to call this function during
        // reparsing anyways, as the function is already guaranteed to be valid by the original parsing.
        // https://bugs.webkit.org/show_bug.cgi?id=164087
        ASSERT(!m_lexer->isReparsingFunction());

        unsigned i = m_scopeStack.size() - 1;
        ASSERT(i < m_scopeStack.size());
        while (!m_scopeStack[i].allowsVarDeclarations()) {
            i--;
            ASSERT(i < m_scopeStack.size());
        }

        if (m_scopeStack[i].isGeneratorBoundary() || m_scopeStack[i].isAsyncFunctionBoundary()) {
            // The formal parameters which need to be verified for Generators and Async Function bodies occur
            // in the outer wrapper function, so pick the outer scope here.
            i--;
            ASSERT(i < m_scopeStack.size());
        }
        return m_scopeStack[i].hasDeclaredParameter(ident);
    }
    
    bool exportName(const Identifier& ident)
    {
        ASSERT(currentScope().index() == 0);
        ASSERT(m_moduleScopeData);
        return m_moduleScopeData->exportName(ident);
    }

    ScopeStack m_scopeStack;
    
    const SourceProviderCacheItem* findCachedFunctionInfo(int openBracePos) 
    {
        return m_functionCache ? m_functionCache->get(openBracePos) : 0;
    }

    Parser();

    String parseInner(const Identifier&, SourceParseMode, ParsingContext, Optional<int> functionConstructorParametersEndPosition = WTF::nullopt);

    void didFinishParsing(SourceElements*, DeclarationStacks::FunctionStack&&, VariableEnvironment&, UniquedStringImplPtrSet&&, CodeFeatures, int);

    // Used to determine type of error to report.
    bool isFunctionMetadataNode(ScopeNode*) { return false; }
    bool isFunctionMetadataNode(FunctionMetadataNode*) { return true; }

    ALWAYS_INLINE void next(unsigned lexerFlags = 0)
    {
        int lastLine = m_token.m_location.line;
        int lastTokenEnd = m_token.m_location.endOffset;
        int lastTokenLineStart = m_token.m_location.lineStartOffset;
        m_lastTokenEndPosition = JSTextPosition(lastLine, lastTokenEnd, lastTokenLineStart);
        m_lexer->setLastLineNumber(lastLine);
        m_token.m_type = m_lexer->lex(&m_token, lexerFlags, strictMode());
    }

    ALWAYS_INLINE void nextWithoutClearingLineTerminator(unsigned lexerFlags = 0)
    {
        int lastLine = m_token.m_location.line;
        int lastTokenEnd = m_token.m_location.endOffset;
        int lastTokenLineStart = m_token.m_location.lineStartOffset;
        m_lastTokenEndPosition = JSTextPosition(lastLine, lastTokenEnd, lastTokenLineStart);
        m_lexer->setLastLineNumber(lastLine);
        m_token.m_type = m_lexer->lexWithoutClearingLineTerminator(&m_token, lexerFlags, strictMode());
    }

    ALWAYS_INLINE void nextExpectIdentifier(unsigned lexerFlags = 0)
    {
        int lastLine = m_token.m_location.line;
        int lastTokenEnd = m_token.m_location.endOffset;
        int lastTokenLineStart = m_token.m_location.lineStartOffset;
        m_lastTokenEndPosition = JSTextPosition(lastLine, lastTokenEnd, lastTokenLineStart);
        m_lexer->setLastLineNumber(lastLine);
        m_token.m_type = m_lexer->lexExpectIdentifier(&m_token, lexerFlags, strictMode());
    }

    ALWAYS_INLINE void lexCurrentTokenAgainUnderCurrentContext()
    {
        auto savePoint = createSavePoint();
        restoreSavePoint(savePoint);
    }

    ALWAYS_INLINE bool nextTokenIsColon()
    {
        return m_lexer->nextTokenIsColon();
    }

    ALWAYS_INLINE bool consume(JSTokenType expected, unsigned flags = 0)
    {
        bool result = m_token.m_type == expected;
        if (result)
            next(flags);
        return result;
    }

    void printUnexpectedTokenText(WTF::PrintStream&);
    ALWAYS_INLINE StringView getToken()
    {
        return m_lexer->getToken(m_token);
    }

    ALWAYS_INLINE StringView getToken(const JSToken& token)
    {
        return m_lexer->getToken(token);
    }

    ALWAYS_INLINE bool match(JSTokenType expected)
    {
        return m_token.m_type == expected;
    }
    
    ALWAYS_INLINE bool matchContextualKeyword(const Identifier& identifier)
    {
        return m_token.m_type == IDENT && *m_token.m_data.ident == identifier && !m_token.m_data.escaped;
    }

    ALWAYS_INLINE bool matchIdentifierOrKeyword()
    {
        return isIdentifierOrKeyword(m_token);
    }
    
    ALWAYS_INLINE unsigned tokenStart()
    {
        return m_token.m_location.startOffset;
    }
    
    ALWAYS_INLINE const JSTextPosition& tokenStartPosition()
    {
        return m_token.m_startPosition;
    }

    ALWAYS_INLINE int tokenLine()
    {
        return m_token.m_location.line;
    }
    
    ALWAYS_INLINE int tokenColumn()
    {
        return tokenStart() - tokenLineStart();
    }

    ALWAYS_INLINE const JSTextPosition& tokenEndPosition()
    {
        return m_token.m_endPosition;
    }
    
    ALWAYS_INLINE unsigned tokenLineStart()
    {
        return m_token.m_location.lineStartOffset;
    }
    
    ALWAYS_INLINE const JSTokenLocation& tokenLocation()
    {
        return m_token.m_location;
    }

    void setErrorMessage(const String& message)
    {
        ASSERT_WITH_MESSAGE(!message.isEmpty(), "Attempted to set the empty string as an error message. Likely caused by invalid UTF8 used when creating the message.");
        m_errorMessage = message;
        if (m_errorMessage.isEmpty())
            m_errorMessage = "Unparseable script"_s;
    }
    
    NEVER_INLINE void logError(bool);
    template <typename... Args>
    NEVER_INLINE void logError(bool, Args&&...);
    
    NEVER_INLINE void updateErrorWithNameAndMessage(const char* beforeMessage, const String& name, const char* afterMessage)
    {
        m_errorMessage = makeString(beforeMessage, " '", name, "' ", afterMessage);
    }
    
    NEVER_INLINE void updateErrorMessage(const char* msg)
    {
        ASSERT(msg);
        m_errorMessage = String(msg);
        ASSERT(!m_errorMessage.isNull());
    }

    ALWAYS_INLINE void recordPauseLocation(const JSTextPosition&);
    ALWAYS_INLINE void recordFunctionEntryLocation(const JSTextPosition&);
    ALWAYS_INLINE void recordFunctionLeaveLocation(const JSTextPosition&);

    void startLoop() { currentScope()->startLoop(); }
    void endLoop() { currentScope()->endLoop(); }
    void startSwitch() { currentScope()->startSwitch(); }
    void endSwitch() { currentScope()->endSwitch(); }
    void setStrictMode() { currentScope()->setStrictMode(); }
    bool strictMode() { return currentScope()->strictMode(); }
    bool isValidStrictMode()
    {
        int i = m_scopeStack.size() - 1;
        if (!m_scopeStack[i].isValidStrictMode())
            return false;

        // In the case of Generator or Async function bodies, also check the wrapper function, whose name or
        // arguments may be invalid.
        if (UNLIKELY((m_scopeStack[i].isGeneratorBoundary() || m_scopeStack[i].isAsyncFunctionBoundary()) && i))
            return m_scopeStack[i - 1].isValidStrictMode();
        return true;
    }
    DeclarationResultMask declareParameter(const Identifier* ident) { return currentScope()->declareParameter(ident); }
    bool declareRestOrNormalParameter(const Identifier&, const Identifier**);

    bool breakIsValid()
    {
        ScopeRef current = currentScope();
        while (!current->breakIsValid()) {
            if (!current.hasContainingScope())
                return false;
            current = current.containingScope();
        }
        return true;
    }
    bool continueIsValid()
    {
        ScopeRef current = currentScope();
        while (!current->continueIsValid()) {
            if (!current.hasContainingScope())
                return false;
            current = current.containingScope();
        }
        return true;
    }
    void pushLabel(const Identifier* label, bool isLoop) { currentScope()->pushLabel(label, isLoop); }
    void popLabel(ScopeRef scope) { scope->popLabel(); }
    ScopeLabelInfo* getLabel(const Identifier* label)
    {
        ScopeRef current = currentScope();
        ScopeLabelInfo* result = 0;
        while (!(result = current->getLabel(label))) {
            if (!current.hasContainingScope())
                return 0;
            current = current.containingScope();
        }
        return result;
    }

    // http://ecma-international.org/ecma-262/6.0/#sec-identifiers-static-semantics-early-errors
    ALWAYS_INLINE bool isLETMaskedAsIDENT()
    {
        return match(LET) && !strictMode();
    }

    // http://ecma-international.org/ecma-262/6.0/#sec-identifiers-static-semantics-early-errors
    ALWAYS_INLINE bool isYIELDMaskedAsIDENT(bool inGenerator)
    {
        return match(YIELD) && !strictMode() && !inGenerator;
    }

    // http://ecma-international.org/ecma-262/6.0/#sec-generator-function-definitions-static-semantics-early-errors
    ALWAYS_INLINE bool matchSpecIdentifier(bool inGenerator)
    {
        return match(IDENT) || isLETMaskedAsIDENT() || isYIELDMaskedAsIDENT(inGenerator) || isSafeContextualKeyword(m_token);
    }

    ALWAYS_INLINE bool matchSpecIdentifier()
    {
        return match(IDENT) || isLETMaskedAsIDENT() || isYIELDMaskedAsIDENT(currentScope()->isGenerator()) || isSafeContextualKeyword(m_token);
    }

    template <class TreeBuilder> TreeSourceElements parseSourceElements(TreeBuilder&, SourceElementsMode);
    template <class TreeBuilder> TreeSourceElements parseGeneratorFunctionSourceElements(TreeBuilder&, const Identifier& name, SourceElementsMode);
    template <class TreeBuilder> TreeSourceElements parseAsyncFunctionSourceElements(TreeBuilder&, SourceParseMode, bool isArrowFunctionBodyExpression, SourceElementsMode);
    template <class TreeBuilder> TreeSourceElements parseAsyncGeneratorFunctionSourceElements(TreeBuilder&, SourceParseMode, bool isArrowFunctionBodyExpression, SourceElementsMode);
    template <class TreeBuilder> TreeSourceElements parseSingleFunction(TreeBuilder&, Optional<int> functionConstructorParametersEndPosition);
    template <class TreeBuilder> TreeStatement parseStatementListItem(TreeBuilder&, const Identifier*& directive, unsigned* directiveLiteralLength);
    template <class TreeBuilder> TreeStatement parseStatement(TreeBuilder&, const Identifier*& directive, unsigned* directiveLiteralLength = 0);
    enum class ExportType { Exported, NotExported };
    template <class TreeBuilder> TreeStatement parseClassDeclaration(TreeBuilder&, ExportType = ExportType::NotExported, DeclarationDefaultContext = DeclarationDefaultContext::Standard);
    template <class TreeBuilder> TreeStatement parseFunctionDeclaration(TreeBuilder&, ExportType = ExportType::NotExported, DeclarationDefaultContext = DeclarationDefaultContext::Standard, Optional<int> functionConstructorParametersEndPosition = WTF::nullopt);
    template <class TreeBuilder> TreeStatement parseFunctionDeclarationStatement(TreeBuilder&, bool isAsync, bool parentAllowsFunctionDeclarationAsStatement);
    template <class TreeBuilder> TreeStatement parseAsyncFunctionDeclaration(TreeBuilder&, ExportType = ExportType::NotExported, DeclarationDefaultContext = DeclarationDefaultContext::Standard, Optional<int> functionConstructorParametersEndPosition = WTF::nullopt);
    template <class TreeBuilder> NEVER_INLINE bool maybeParseAsyncFunctionDeclarationStatement(TreeBuilder& context, TreeStatement& result, bool parentAllowsFunctionDeclarationAsStatement);
    template <class TreeBuilder> TreeStatement parseVariableDeclaration(TreeBuilder&, DeclarationType, ExportType = ExportType::NotExported);
    template <class TreeBuilder> TreeStatement parseDoWhileStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseWhileStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseForStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseBreakStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseContinueStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseReturnStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseThrowStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseWithStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseSwitchStatement(TreeBuilder&);
    template <class TreeBuilder> TreeClauseList parseSwitchClauses(TreeBuilder&);
    template <class TreeBuilder> TreeClause parseSwitchDefaultClause(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseTryStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseDebuggerStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseExpressionStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseExpressionOrLabelStatement(TreeBuilder&, bool allowFunctionDeclarationAsStatement);
    template <class TreeBuilder> TreeStatement parseIfStatement(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseBlockStatement(TreeBuilder&);
    template <class TreeBuilder> TreeExpression parseExpression(TreeBuilder&);
    template <class TreeBuilder> TreeExpression parseAssignmentExpression(TreeBuilder&, ExpressionErrorClassifier&);
    template <class TreeBuilder> TreeExpression parseAssignmentExpression(TreeBuilder&);
    template <class TreeBuilder> TreeExpression parseAssignmentExpressionOrPropagateErrorClass(TreeBuilder&);
    template <class TreeBuilder> TreeExpression parseYieldExpression(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parseConditionalExpression(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parseBinaryExpression(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parseUnaryExpression(TreeBuilder&);
    template <class TreeBuilder> NEVER_INLINE TreeExpression parseAwaitExpression(TreeBuilder&);
    template <class TreeBuilder> TreeExpression parseMemberExpression(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parsePrimaryExpression(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parseArrayLiteral(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parseObjectLiteral(TreeBuilder&);
    template <class TreeBuilder> NEVER_INLINE TreeExpression parseStrictObjectLiteral(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeClassExpression parseClassExpression(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parseFunctionExpression(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parseAsyncFunctionExpression(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeArguments parseArguments(TreeBuilder&);
    template <class TreeBuilder> ALWAYS_INLINE TreeExpression parseArgument(TreeBuilder&, ArgumentType&);
    template <class TreeBuilder> TreeProperty parseProperty(TreeBuilder&, bool strict);
    template <class TreeBuilder> TreeExpression parsePropertyMethod(TreeBuilder& context, const Identifier* methodName, SourceParseMode);
    template <class TreeBuilder> TreeProperty parseGetterSetter(TreeBuilder&, bool strict, PropertyNode::Type, unsigned getterOrSetterStartOffset, ConstructorKind, ClassElementTag);
    template <class TreeBuilder> ALWAYS_INLINE TreeFunctionBody parseFunctionBody(TreeBuilder&, SyntaxChecker&, const JSTokenLocation&, int, int functionKeywordStart, int functionNameStart, int parametersStart, ConstructorKind, SuperBinding, FunctionBodyType, unsigned, SourceParseMode);
    template <class TreeBuilder> ALWAYS_INLINE bool parseFormalParameters(TreeBuilder&, TreeFormalParameterList, bool isArrowFunction, bool isMethod, unsigned&);
    enum VarDeclarationListContext { ForLoopContext, VarDeclarationContext };
    template <class TreeBuilder> TreeExpression parseVariableDeclarationList(TreeBuilder&, int& declarations, TreeDestructuringPattern& lastPattern, TreeExpression& lastInitializer, JSTextPosition& identStart, JSTextPosition& initStart, JSTextPosition& initEnd, VarDeclarationListContext, DeclarationType, ExportType, bool& forLoopConstDoesNotHaveInitializer);
    template <class TreeBuilder> TreeSourceElements parseArrowFunctionSingleExpressionBodySourceElements(TreeBuilder&);
    template <class TreeBuilder> TreeExpression parseArrowFunctionExpression(TreeBuilder&, bool isAsync);
    template <class TreeBuilder> NEVER_INLINE TreeDestructuringPattern createBindingPattern(TreeBuilder&, DestructuringKind, ExportType, const Identifier&, JSToken, AssignmentContext, const Identifier** duplicateIdentifier);
    template <class TreeBuilder> NEVER_INLINE TreeDestructuringPattern createAssignmentElement(TreeBuilder&, TreeExpression&, const JSTextPosition&, const JSTextPosition&);
    template <class TreeBuilder> NEVER_INLINE TreeDestructuringPattern parseObjectRestBindingOrAssignmentElement(TreeBuilder& context, DestructuringKind, ExportType, const Identifier** duplicateIdentifier, AssignmentContext bindingContext);
    template <class TreeBuilder> NEVER_INLINE TreeDestructuringPattern parseBindingOrAssignmentElement(TreeBuilder& context, DestructuringKind, ExportType, const Identifier** duplicateIdentifier, bool* hasDestructuringPattern, AssignmentContext bindingContext, int depth);
    template <class TreeBuilder> NEVER_INLINE TreeDestructuringPattern parseObjectRestAssignmentElement(TreeBuilder& context);
    template <class TreeBuilder> NEVER_INLINE TreeDestructuringPattern parseAssignmentElement(TreeBuilder& context, DestructuringKind, ExportType, const Identifier** duplicateIdentifier, bool* hasDestructuringPattern, AssignmentContext bindingContext, int depth);
    template <class TreeBuilder> NEVER_INLINE TreeDestructuringPattern parseObjectRestElement(TreeBuilder&, DestructuringKind, ExportType, const Identifier** duplicateIdentifier = nullptr, AssignmentContext = AssignmentContext::DeclarationStatement);
    template <class TreeBuilder> NEVER_INLINE TreeDestructuringPattern parseDestructuringPattern(TreeBuilder&, DestructuringKind, ExportType, const Identifier** duplicateIdentifier = nullptr, bool* hasDestructuringPattern = nullptr, AssignmentContext = AssignmentContext::DeclarationStatement, int depth = 0);
    template <class TreeBuilder> NEVER_INLINE TreeDestructuringPattern tryParseDestructuringPatternExpression(TreeBuilder&, AssignmentContext);
    template <class TreeBuilder> NEVER_INLINE TreeExpression parseDefaultValueForDestructuringPattern(TreeBuilder&);
    template <class TreeBuilder> TreeSourceElements parseModuleSourceElements(TreeBuilder&, SourceParseMode);
    enum class ImportSpecifierType { NamespaceImport, NamedImport, DefaultImport };
    template <class TreeBuilder> typename TreeBuilder::ImportSpecifier parseImportClauseItem(TreeBuilder&, ImportSpecifierType);
    template <class TreeBuilder> typename TreeBuilder::ModuleName parseModuleName(TreeBuilder&);
    template <class TreeBuilder> TreeStatement parseImportDeclaration(TreeBuilder&);
    template <class TreeBuilder> typename TreeBuilder::ExportSpecifier parseExportSpecifier(TreeBuilder& context, Vector<std::pair<const Identifier*, const Identifier*>>& maybeExportedLocalNames, bool& hasKeywordForLocalBindings);
    template <class TreeBuilder> TreeStatement parseExportDeclaration(TreeBuilder&);

    template <class TreeBuilder> ALWAYS_INLINE TreeExpression createResolveAndUseVariable(TreeBuilder&, const Identifier*, bool isEval, const JSTextPosition&, const JSTokenLocation&);

    enum class FunctionDefinitionType { Expression, Declaration, Method };
    template <class TreeBuilder> NEVER_INLINE bool parseFunctionInfo(TreeBuilder&, FunctionNameRequirements, SourceParseMode, bool nameIsInContainingScope, ConstructorKind, SuperBinding, int functionKeywordStart, ParserFunctionInfo<TreeBuilder>&, FunctionDefinitionType, Optional<int> functionConstructorParametersEndPosition = WTF::nullopt);
    
    ALWAYS_INLINE bool isArrowFunctionParameters();
    
    template <class TreeBuilder, class FunctionInfoType> NEVER_INLINE typename TreeBuilder::FormalParameterList parseFunctionParameters(TreeBuilder&, SourceParseMode, FunctionInfoType&);
    template <class TreeBuilder> NEVER_INLINE typename TreeBuilder::FormalParameterList createGeneratorParameters(TreeBuilder&, unsigned& parameterCount);

    template <class TreeBuilder> NEVER_INLINE TreeClassExpression parseClass(TreeBuilder&, FunctionNameRequirements, ParserClassInfo<TreeBuilder>&);

    template <class TreeBuilder> NEVER_INLINE typename TreeBuilder::TemplateString parseTemplateString(TreeBuilder& context, bool isTemplateHead, typename LexerType::RawStringsBuildMode, bool& elementIsTail);
    template <class TreeBuilder> NEVER_INLINE typename TreeBuilder::TemplateLiteral parseTemplateLiteral(TreeBuilder&, typename LexerType::RawStringsBuildMode);

    template <class TreeBuilder> ALWAYS_INLINE bool shouldCheckPropertyForUnderscoreProtoDuplicate(TreeBuilder&, const TreeProperty&);

    template <class TreeBuilder> NEVER_INLINE const char* metaPropertyName(TreeBuilder&, TreeExpression);

    template <class TreeBuilder> ALWAYS_INLINE bool isSimpleAssignmentTarget(TreeBuilder&, TreeExpression);

    ALWAYS_INLINE int isBinaryOperator(JSTokenType);
    bool allowAutomaticSemicolon();
    
    bool autoSemiColon()
    {
        if (m_token.m_type == SEMICOLON) {
            next();
            return true;
        }
        return allowAutomaticSemicolon();
    }
    
    bool canRecurse()
    {
        return m_vm->isSafeToRecurse();
    }
    
    const JSTextPosition& lastTokenEndPosition() const
    {
        return m_lastTokenEndPosition;
    }

    bool hasError() const
    {
        return !m_errorMessage.isNull();
    }

    bool isDisallowedIdentifierLet(const JSToken& token)
    {
        return token.m_type == LET && strictMode();
    }

    bool isDisallowedIdentifierAwait(const JSToken& token)
    {
        return token.m_type == AWAIT && (!m_parserState.allowAwait || currentScope()->isAsyncFunctionBoundary() || m_scriptMode == JSParserScriptMode::Module);
    }

    bool isDisallowedIdentifierYield(const JSToken& token)
    {
        return token.m_type == YIELD && (strictMode() || currentScope()->isGenerator());
    }
    
    ALWAYS_INLINE SuperBinding adjustSuperBindingForBaseConstructor(ConstructorKind constructorKind, SuperBinding superBinding, ScopeRef functionScope)
    {
        return adjustSuperBindingForBaseConstructor(constructorKind, superBinding, functionScope->needsSuperBinding(), functionScope->usesEval(), functionScope->innerArrowFunctionFeatures());
    }
    
    ALWAYS_INLINE SuperBinding adjustSuperBindingForBaseConstructor(ConstructorKind constructorKind, SuperBinding superBinding, bool scopeNeedsSuperBinding, bool currentScopeUsesEval, InnerArrowFunctionCodeFeatures innerArrowFunctionFeatures)
    {
        SuperBinding methodSuperBinding = superBinding;
        
        if (constructorKind == ConstructorKind::Base) {
            bool isSuperUsedInInnerArrowFunction = innerArrowFunctionFeatures & SuperPropertyInnerArrowFunctionFeature;
            methodSuperBinding = (scopeNeedsSuperBinding || isSuperUsedInInnerArrowFunction || currentScopeUsesEval) ? SuperBinding::Needed : SuperBinding::NotNeeded;
        }
        
        return methodSuperBinding;
    }

    const char* disallowedIdentifierLetReason()
    {
        ASSERT(strictMode());
        return "in strict mode";
    }

    const char* disallowedIdentifierAwaitReason()
    {
        if (!m_parserState.allowAwait || currentScope()->isAsyncFunctionBoundary())
            return "in an async function";
        if (m_scriptMode == JSParserScriptMode::Module)
            return "in a module";
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }

    const char* disallowedIdentifierYieldReason()
    {
        if (strictMode())
            return "in strict mode";
        if (currentScope()->isGenerator())
            return "in a generator function";
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }

    enum class FunctionParsePhase { Parameters, Body };
    struct ParserState {
        int assignmentCount { 0 };
        int nonLHSCount { 0 };
        int nonTrivialExpressionCount { 0 };
        FunctionParsePhase functionParsePhase { FunctionParsePhase::Body };
        const Identifier* lastIdentifier { nullptr };
        const Identifier* lastFunctionName { nullptr };
        bool allowAwait { true };
    };

    // If you're using this directly, you probably should be using
    // createSavePoint() instead.
    ALWAYS_INLINE ParserState internalSaveParserState()
    {
        return m_parserState;
    }

    ALWAYS_INLINE void restoreParserState(const ParserState& state)
    {
        m_parserState = state;
    }

    struct LexerState {
        int startOffset;
        unsigned oldLineStartOffset;
        unsigned oldLastLineNumber;
        unsigned oldLineNumber;
        bool hasLineTerminatorBeforeToken;
    };

    // If you're using this directly, you probably should be using
    // createSavePoint() instead.
    // i.e, if you parse any kind of AssignmentExpression between
    // saving/restoring, you should definitely not be using this directly.
    ALWAYS_INLINE LexerState internalSaveLexerState()
    {
        LexerState result;
        result.startOffset = m_token.m_location.startOffset;
        result.oldLineStartOffset = m_token.m_location.lineStartOffset;
        result.oldLastLineNumber = m_lexer->lastLineNumber();
        result.oldLineNumber = m_lexer->lineNumber();
        result.hasLineTerminatorBeforeToken = m_lexer->hasLineTerminatorBeforeToken();
        ASSERT(static_cast<unsigned>(result.startOffset) >= result.oldLineStartOffset);
        return result;
    }

    ALWAYS_INLINE void restoreLexerState(const LexerState& lexerState)
    {
        // setOffset clears lexer errors.
        m_lexer->setOffset(lexerState.startOffset, lexerState.oldLineStartOffset);
        m_lexer->setLineNumber(lexerState.oldLineNumber);
        m_lexer->setHasLineTerminatorBeforeToken(lexerState.hasLineTerminatorBeforeToken);
        nextWithoutClearingLineTerminator();
        m_lexer->setLastLineNumber(lexerState.oldLastLineNumber);
    }

    struct SavePoint {
        ParserState parserState;
        LexerState lexerState;
    };

    struct SavePointWithError : public SavePoint {
        bool lexerError;
        String lexerErrorMessage;
        String parserErrorMessage;
    };

    ALWAYS_INLINE void internalSaveState(SavePoint& savePoint)
    {
        savePoint.parserState = internalSaveParserState();
        savePoint.lexerState = internalSaveLexerState();
    }
    
    ALWAYS_INLINE SavePointWithError createSavePointForError()
    {
        SavePointWithError savePoint;
        internalSaveState(savePoint);
        savePoint.lexerError = m_lexer->sawError();
        savePoint.lexerErrorMessage = m_lexer->getErrorMessage();
        savePoint.parserErrorMessage = m_errorMessage;
        return savePoint;
    }
    
    ALWAYS_INLINE SavePoint createSavePoint()
    {
        ASSERT(!hasError());
        SavePoint savePoint;
        internalSaveState(savePoint);
        return savePoint;
    }

    ALWAYS_INLINE void internalRestoreState(const SavePoint& savePoint)
    {
        restoreLexerState(savePoint.lexerState);
        restoreParserState(savePoint.parserState);
    }

    ALWAYS_INLINE void restoreSavePointWithError(const SavePointWithError& savePoint)
    {
        internalRestoreState(savePoint);
        m_lexer->setSawError(savePoint.lexerError);
        m_lexer->setErrorMessage(savePoint.lexerErrorMessage);
        m_errorMessage = savePoint.parserErrorMessage;
    }

    ALWAYS_INLINE void restoreSavePoint(const SavePoint& savePoint)
    {
        internalRestoreState(savePoint);
        m_errorMessage = String();
    }

    VM* m_vm;
    const SourceCode* m_source;
    ParserArena m_parserArena;
    std::unique_ptr<LexerType> m_lexer;
    FunctionParameters* m_parameters { nullptr };

    ParserState m_parserState;
    
    bool m_hasStackOverflow;
    String m_errorMessage;
    JSToken m_token;
    bool m_allowsIn;
    JSTextPosition m_lastTokenEndPosition;
    int m_statementDepth;
    RefPtr<SourceProviderCache> m_functionCache;
    SourceElements* m_sourceElements;
    bool m_parsingBuiltin;
    JSParserScriptMode m_scriptMode;
    SuperBinding m_superBinding;
    ConstructorKind m_defaultConstructorKind;
    VariableEnvironment m_varDeclarations;
    DeclarationStacks::FunctionStack m_funcDeclarations;
    UniquedStringImplPtrSet m_sloppyModeHoistedFunctions;
    CodeFeatures m_features;
    int m_numConstants;
    ExpressionErrorClassifier* m_expressionErrorClassifier;
    bool m_isEvalContext;
    bool m_immediateParentAllowsFunctionDeclarationInStatement;
    RefPtr<ModuleScopeData> m_moduleScopeData;
    DebuggerParseData* m_debuggerParseData;
    CallOrApplyDepthScope* m_callOrApplyDepthScope { nullptr };
    bool m_seenTaggedTemplate { false };
};


template <typename LexerType>
template <class ParsedNode>
std::unique_ptr<ParsedNode> Parser<LexerType>::parse(ParserError& error, const Identifier& calleeName, SourceParseMode parseMode, ParsingContext parsingContext, Optional<int> functionConstructorParametersEndPosition)
{
    int errLine;
    String errMsg;

    if (ParsedNode::scopeIsFunction)
        m_lexer->setIsReparsingFunction();

    m_sourceElements = 0;

    errLine = -1;
    errMsg = String();

    JSTokenLocation startLocation(tokenLocation());
    ASSERT(m_source->startColumn() > OrdinalNumber::beforeFirst());
    unsigned startColumn = m_source->startColumn().zeroBasedInt();

    String parseError = parseInner(calleeName, parseMode, parsingContext, functionConstructorParametersEndPosition);

    int lineNumber = m_lexer->lineNumber();
    bool lexError = m_lexer->sawError();
    String lexErrorMessage = lexError ? m_lexer->getErrorMessage() : String();
    ASSERT(lexErrorMessage.isNull() != lexError);
    m_lexer->clear();

    if (!parseError.isNull() || lexError) {
        errLine = lineNumber;
        errMsg = !lexErrorMessage.isNull() ? lexErrorMessage : parseError;
        m_sourceElements = 0;
    }

    std::unique_ptr<ParsedNode> result;
    if (m_sourceElements) {
        JSTokenLocation endLocation;
        endLocation.line = m_lexer->lineNumber();
        endLocation.lineStartOffset = m_lexer->currentLineStartOffset();
        endLocation.startOffset = m_lexer->currentOffset();
        unsigned endColumn = endLocation.startOffset - endLocation.lineStartOffset;
        result = makeUnique<ParsedNode>(m_parserArena,
                                    startLocation,
                                    endLocation,
                                    startColumn,
                                    endColumn,
                                    m_sourceElements,
                                    m_varDeclarations,
                                    WTFMove(m_funcDeclarations),
                                    currentScope()->finalizeLexicalEnvironment(),
                                    WTFMove(m_sloppyModeHoistedFunctions),
                                    m_parameters,
                                    *m_source,
                                    m_features,
                                    currentScope()->innerArrowFunctionFeatures(),
                                    m_numConstants,
                                    WTFMove(m_moduleScopeData));
        result->setLoc(m_source->firstLine().oneBasedInt(), m_lexer->lineNumber(), m_lexer->currentOffset(), m_lexer->currentLineStartOffset());
        result->setEndOffset(m_lexer->currentOffset());

        if (!isFunctionParseMode(parseMode)) {
            m_source->provider()->setSourceURLDirective(m_lexer->sourceURLDirective());
            m_source->provider()->setSourceMappingURLDirective(m_lexer->sourceMappingURLDirective());
        }
    } else {
        // We can never see a syntax error when reparsing a function, since we should have
        // reported the error when parsing the containing program or eval code. So if we're
        // parsing a function body node, we assume that what actually happened here is that
        // we ran out of stack while parsing. If we see an error while parsing eval or program
        // code we assume that it was a syntax error since running out of stack is much less
        // likely, and we are currently unable to distinguish between the two cases.
        if (isFunctionMetadataNode(static_cast<ParsedNode*>(0)) || m_hasStackOverflow)
            error = ParserError(ParserError::StackOverflow, ParserError::SyntaxErrorNone, m_token);
        else {
            ParserError::SyntaxErrorType errorType = ParserError::SyntaxErrorIrrecoverable;
            if (m_token.m_type == EOFTOK)
                errorType = ParserError::SyntaxErrorRecoverable;
            else if (m_token.m_type & UnterminatedErrorTokenFlag) {
                // Treat multiline capable unterminated literals as recoverable.
                if (m_token.m_type == UNTERMINATED_MULTILINE_COMMENT_ERRORTOK || m_token.m_type == UNTERMINATED_TEMPLATE_LITERAL_ERRORTOK)
                    errorType = ParserError::SyntaxErrorRecoverable;
                else
                    errorType = ParserError::SyntaxErrorUnterminatedLiteral;
            }
            
            if (isEvalNode<ParsedNode>())
                error = ParserError(ParserError::EvalError, errorType, m_token, errMsg, errLine);
            else
                error = ParserError(ParserError::SyntaxError, errorType, m_token, errMsg, errLine);
        }
    }

    return result;
}

template <class ParsedNode>
std::unique_ptr<ParsedNode> parse(
    VM* vm, const SourceCode& source,
    const Identifier& name, JSParserBuiltinMode builtinMode,
    JSParserStrictMode strictMode, JSParserScriptMode scriptMode, SourceParseMode parseMode, SuperBinding superBinding,
    ParserError& error, JSTextPosition* positionBeforeLastNewline = nullptr,
    ConstructorKind defaultConstructorKind = ConstructorKind::None,
    DerivedContextType derivedContextType = DerivedContextType::None,
    EvalContextType evalContextType = EvalContextType::None,
    DebuggerParseData* debuggerParseData = nullptr)
{
    ASSERT(!source.provider()->source().isNull());

    MonotonicTime before;
    if (UNLIKELY(Options::reportParseTimes()))
        before = MonotonicTime::now();

    std::unique_ptr<ParsedNode> result;
    if (source.provider()->source().is8Bit()) {
        Parser<Lexer<LChar>> parser(vm, source, builtinMode, strictMode, scriptMode, parseMode, superBinding, defaultConstructorKind, derivedContextType, isEvalNode<ParsedNode>(), evalContextType, debuggerParseData);
        result = parser.parse<ParsedNode>(error, name, parseMode, isEvalNode<ParsedNode>() ? ParsingContext::Eval : ParsingContext::Program);
        if (positionBeforeLastNewline)
            *positionBeforeLastNewline = parser.positionBeforeLastNewline();
        if (builtinMode == JSParserBuiltinMode::Builtin) {
            if (!result) {
                ASSERT(error.isValid());
                if (error.type() != ParserError::StackOverflow)
                    dataLogLn("Unexpected error compiling builtin: ", error.message());
            }
        }
    } else {
        ASSERT_WITH_MESSAGE(defaultConstructorKind == ConstructorKind::None, "BuiltinExecutables::createDefaultConstructor should always use a 8-bit string");
        Parser<Lexer<UChar>> parser(vm, source, builtinMode, strictMode, scriptMode, parseMode, superBinding, defaultConstructorKind, derivedContextType, isEvalNode<ParsedNode>(), evalContextType, debuggerParseData);
        result = parser.parse<ParsedNode>(error, name, parseMode, isEvalNode<ParsedNode>() ? ParsingContext::Eval : ParsingContext::Program);
        if (positionBeforeLastNewline)
            *positionBeforeLastNewline = parser.positionBeforeLastNewline();
    }

    if (UNLIKELY(Options::countParseTimes()))
        globalParseCount++;

    if (UNLIKELY(Options::reportParseTimes())) {
        MonotonicTime after = MonotonicTime::now();
        ParseHash hash(source);
        dataLogLn(result ? "Parsed #" : "Failed to parse #", hash.hashForCall(), "/#", hash.hashForConstruct(), " in ", (after - before).milliseconds(), " ms.");
    }

    return result;
}

inline std::unique_ptr<ProgramNode> parseFunctionForFunctionConstructor(VM& vm, const SourceCode& source, ParserError& error, JSTextPosition* positionBeforeLastNewline, Optional<int> functionConstructorParametersEndPosition)
{
    ASSERT(!source.provider()->source().isNull());

    MonotonicTime before;
    if (UNLIKELY(Options::reportParseTimes()))
        before = MonotonicTime::now();

    Identifier name;
    bool isEvalNode = false;
    std::unique_ptr<ProgramNode> result;
    if (source.provider()->source().is8Bit()) {
        Parser<Lexer<LChar>> parser(&vm, source, JSParserBuiltinMode::NotBuiltin, JSParserStrictMode::NotStrict, JSParserScriptMode::Classic, SourceParseMode::ProgramMode, SuperBinding::NotNeeded, ConstructorKind::None, DerivedContextType::None, isEvalNode, EvalContextType::None, nullptr);
        result = parser.parse<ProgramNode>(error, name, SourceParseMode::ProgramMode, ParsingContext::FunctionConstructor, functionConstructorParametersEndPosition);
        if (positionBeforeLastNewline)
            *positionBeforeLastNewline = parser.positionBeforeLastNewline();
    } else {
        Parser<Lexer<UChar>> parser(&vm, source, JSParserBuiltinMode::NotBuiltin, JSParserStrictMode::NotStrict, JSParserScriptMode::Classic, SourceParseMode::ProgramMode, SuperBinding::NotNeeded, ConstructorKind::None, DerivedContextType::None, isEvalNode, EvalContextType::None, nullptr);
        result = parser.parse<ProgramNode>(error, name, SourceParseMode::ProgramMode, ParsingContext::FunctionConstructor, functionConstructorParametersEndPosition);
        if (positionBeforeLastNewline)
            *positionBeforeLastNewline = parser.positionBeforeLastNewline();
    }

    if (UNLIKELY(Options::countParseTimes()))
        globalParseCount++;

    if (UNLIKELY(Options::reportParseTimes())) {
        MonotonicTime after = MonotonicTime::now();
        ParseHash hash(source);
        dataLogLn(result ? "Parsed #" : "Failed to parse #", hash.hashForCall(), "/#", hash.hashForConstruct(), " in ", (after - before).milliseconds(), " ms.");
    }

    return result;
}


} // namespace
