/*
 * Copyright (C) 2010, 2013, 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Lexer.h"
#include "ParserFunctionInfo.h"
#include "YarrSyntaxChecker.h"

namespace JSC {

class SyntaxChecker {
public:
    struct BinaryExprContext {
        BinaryExprContext(SyntaxChecker& context)
            : m_context(&context)
        {
            m_token = m_context->m_topBinaryExpr;
            m_context->m_topBinaryExpr = 0;
        }
        ~BinaryExprContext()
        {
            m_context->m_topBinaryExpr = m_token;
        }
    private:
        int m_token;
        SyntaxChecker* m_context;
    };
    struct UnaryExprContext {
        UnaryExprContext(SyntaxChecker& context)
            : m_context(&context)
        {
            m_token = m_context->m_topUnaryToken;
            m_context->m_topUnaryToken = 0;
        }
        ~UnaryExprContext()
        {
            m_context->m_topUnaryToken = m_token;
        }
    private:
        int m_token;
        SyntaxChecker* m_context;
    };
    
    SyntaxChecker(VM* , void*)
    {
    }

    static const constexpr int MetaPropertyBit = 0x80000000;
    enum : int { NoneExpr = 0,
        ResolveEvalExpr, ResolveExpr, IntegerExpr, DoubleExpr, StringExpr, BigIntExpr,
        ThisExpr, NullExpr, BoolExpr, RegExpExpr, ObjectLiteralExpr,
        FunctionExpr, ClassExpr, SuperExpr, ImportExpr, BracketExpr, DotExpr, CallExpr,
        NewExpr, PreExpr, PostExpr, UnaryExpr, BinaryExpr, OptionalChain,
        ConditionalExpr, AssignmentExpr, TypeofExpr,
        DeleteExpr, ArrayLiteralExpr, BindingDestructuring, RestParameter,
        ArrayDestructuring, ObjectDestructuring, SourceElementsResult,
        FunctionBodyResult, SpreadExpr, ObjectSpreadExpr, ArgumentsResult,
        PropertyListResult, ArgumentsListResult, ElementsListResult,
        StatementResult, FormalParameterListResult, ClauseResult,
        ClauseListResult, CommaExpr, DestructuringAssignment,
        TemplateStringResult, TemplateStringListResult,
        TemplateExpressionListResult, TemplateExpr,
        TaggedTemplateExpr, YieldExpr, AwaitExpr,
        ModuleNameResult,
        ImportSpecifierResult, ImportSpecifierListResult,
        ExportSpecifierResult, ExportSpecifierListResult,

        NewTargetExpr = MetaPropertyBit | 0,
        ImportMetaExpr = MetaPropertyBit | 1,
    };
    typedef int ExpressionType;

    typedef ExpressionType Expression;
    typedef int SourceElements;
    typedef int Arguments;
    typedef ExpressionType Comma;
    struct Property {
        ALWAYS_INLINE Property(void* = 0)
            : type((PropertyNode::Type)0)
        {
        }
        ALWAYS_INLINE Property(const Identifier* ident, PropertyNode::Type ty)
            : name(ident)
            , type(ty)
        {
        }
        ALWAYS_INLINE Property(PropertyNode::Type ty)
            : name(0)
            , type(ty)
        {
        }
        ALWAYS_INLINE bool operator!() { return !type; }
        const Identifier* name;
        PropertyNode::Type type;
    };
    typedef int PropertyList;
    typedef int ElementList;
    typedef int ArgumentsList;
    typedef int TemplateExpressionList;
    typedef int TemplateString;
    typedef int TemplateStringList;
    typedef int TemplateLiteral;
    typedef int FormalParameterList;
    typedef int FunctionBody;
    typedef int ClassExpression;
    typedef int ModuleName;
    typedef int ImportSpecifier;
    typedef int ImportSpecifierList;
    typedef int ExportSpecifier;
    typedef int ExportSpecifierList;
    typedef int Statement;
    typedef int ClauseList;
    typedef int Clause;
    typedef int BinaryOperand;
    typedef int DestructuringPattern;
    typedef DestructuringPattern ArrayPattern;
    typedef DestructuringPattern ObjectPattern;
    typedef DestructuringPattern RestPattern;

    static const bool CreatesAST = false;
    static const bool NeedsFreeVariableInfo = false;
    static const bool CanUseFunctionCache = true;
    static const unsigned DontBuildKeywords = LexexFlagsDontBuildKeywords;
    static const unsigned DontBuildStrings = LexerFlagsDontBuildStrings;

    int createSourceElements() { return SourceElementsResult; }
    ExpressionType makeFunctionCallNode(const JSTokenLocation&, ExpressionType, bool, int, int, int, int, size_t, bool) { return CallExpr; }
    ExpressionType createCommaExpr(const JSTokenLocation&, ExpressionType expr) { return expr; }
    ExpressionType appendToCommaExpr(const JSTokenLocation&, ExpressionType& head, ExpressionType, ExpressionType next) { head = next; return next; }
    ExpressionType makeAssignNode(const JSTokenLocation&, ExpressionType, Operator, ExpressionType, bool, bool, int, int, int) { return AssignmentExpr; }
    ExpressionType makePrefixNode(const JSTokenLocation&, ExpressionType, Operator, int, int, int) { return PreExpr; }
    ExpressionType makePostfixNode(const JSTokenLocation&, ExpressionType, Operator, int, int, int) { return PostExpr; }
    ExpressionType makeTypeOfNode(const JSTokenLocation&, ExpressionType) { return TypeofExpr; }
    ExpressionType makeDeleteNode(const JSTokenLocation&, ExpressionType, int, int, int) { return DeleteExpr; }
    ExpressionType makeNegateNode(const JSTokenLocation&, ExpressionType) { return UnaryExpr; }
    ExpressionType makeBitwiseNotNode(const JSTokenLocation&, ExpressionType) { return UnaryExpr; }
    ExpressionType createLogicalNot(const JSTokenLocation&, ExpressionType) { return UnaryExpr; }
    ExpressionType createUnaryPlus(const JSTokenLocation&, ExpressionType) { return UnaryExpr; }
    ExpressionType createVoid(const JSTokenLocation&, ExpressionType) { return UnaryExpr; }
    ExpressionType createImportExpr(const JSTokenLocation&, ExpressionType, int, int, int) { return ImportExpr; }
    ExpressionType createThisExpr(const JSTokenLocation&) { return ThisExpr; }
    ExpressionType createSuperExpr(const JSTokenLocation&) { return SuperExpr; }
    ExpressionType createNewTargetExpr(const JSTokenLocation&) { return NewTargetExpr; }
    ExpressionType createImportMetaExpr(const JSTokenLocation&, ExpressionType) { return ImportMetaExpr; }
    ALWAYS_INLINE bool isMetaProperty(ExpressionType type) { return type & MetaPropertyBit; }
    ALWAYS_INLINE bool isNewTarget(ExpressionType type) { return type == NewTargetExpr; }
    ALWAYS_INLINE bool isImportMeta(ExpressionType type) { return type == ImportMetaExpr; }
    ExpressionType createResolve(const JSTokenLocation&, const Identifier&, int, int) { return ResolveExpr; }
    ExpressionType createObjectLiteral(const JSTokenLocation&) { return ObjectLiteralExpr; }
    ExpressionType createObjectLiteral(const JSTokenLocation&, int) { return ObjectLiteralExpr; }
    ExpressionType createArray(const JSTokenLocation&, int) { return ArrayLiteralExpr; }
    ExpressionType createArray(const JSTokenLocation&, int, int) { return ArrayLiteralExpr; }
    ExpressionType createDoubleExpr(const JSTokenLocation&, double) { return DoubleExpr; }
    ExpressionType createIntegerExpr(const JSTokenLocation&, double) { return IntegerExpr; }
    ExpressionType createBigInt(const JSTokenLocation&, const Identifier*, int) { return BigIntExpr; }
    ExpressionType createString(const JSTokenLocation&, const Identifier*) { return StringExpr; }
    ExpressionType createBoolean(const JSTokenLocation&, bool) { return BoolExpr; }
    ExpressionType createNull(const JSTokenLocation&) { return NullExpr; }
    ExpressionType createBracketAccess(const JSTokenLocation&, ExpressionType, ExpressionType, bool, int, int, int) { return BracketExpr; }
    ExpressionType createDotAccess(const JSTokenLocation&, ExpressionType, const Identifier*, int, int, int) { return DotExpr; }
    ExpressionType createRegExp(const JSTokenLocation&, const Identifier& pattern, const Identifier& flags, int) { return Yarr::hasError(Yarr::checkSyntax(pattern.string(), flags.string())) ? 0 : RegExpExpr; }
    ExpressionType createNewExpr(const JSTokenLocation&, ExpressionType, int, int, int, int) { return NewExpr; }
    ExpressionType createNewExpr(const JSTokenLocation&, ExpressionType, int, int) { return NewExpr; }
    ExpressionType createOptionalChain(const JSTokenLocation&, ExpressionType, ExpressionType, bool) { return OptionalChain; }
    ExpressionType createConditionalExpr(const JSTokenLocation&, ExpressionType, ExpressionType, ExpressionType) { return ConditionalExpr; }
    ExpressionType createAssignResolve(const JSTokenLocation&, const Identifier&, ExpressionType, int, int, int, AssignmentContext) { return AssignmentExpr; }
    ExpressionType createEmptyVarExpression(const JSTokenLocation&, const Identifier&) { return AssignmentExpr; }
    ExpressionType createEmptyLetExpression(const JSTokenLocation&, const Identifier&) { return AssignmentExpr; }
    ExpressionType createYield(const JSTokenLocation&) { return YieldExpr; }
    ExpressionType createYield(const JSTokenLocation&, ExpressionType, bool, int, int, int) { return YieldExpr; }
    ExpressionType createAwait(const JSTokenLocation&, ExpressionType, int, int, int) { return AwaitExpr; }
    ClassExpression createClassExpr(const JSTokenLocation&, const ParserClassInfo<SyntaxChecker>&, VariableEnvironment&, ExpressionType, ExpressionType, PropertyList) { return ClassExpr; }
    ExpressionType createFunctionExpr(const JSTokenLocation&, const ParserFunctionInfo<SyntaxChecker>&) { return FunctionExpr; }
    ExpressionType createGeneratorFunctionBody(const JSTokenLocation&, const ParserFunctionInfo<SyntaxChecker>&, const Identifier&) { return FunctionExpr; }
    ExpressionType createAsyncFunctionBody(const JSTokenLocation&, const ParserFunctionInfo<SyntaxChecker>&) { return FunctionExpr; }
    int createFunctionMetadata(const JSTokenLocation&, const JSTokenLocation&, int, int, bool, int, int, int, ConstructorKind, SuperBinding, unsigned, SourceParseMode, bool, InnerArrowFunctionCodeFeatures = NoInnerArrowFunctionFeatures) { return FunctionBodyResult; }
    ExpressionType createArrowFunctionExpr(const JSTokenLocation&, const ParserFunctionInfo<SyntaxChecker>&) { return FunctionExpr; }
    ExpressionType createMethodDefinition(const JSTokenLocation&, const ParserFunctionInfo<SyntaxChecker>&) { return FunctionExpr; }
    void setFunctionNameStart(int, int) { }
    int createArguments() { return ArgumentsResult; }
    int createArguments(int) { return ArgumentsResult; }
    ExpressionType createSpreadExpression(const JSTokenLocation&, ExpressionType, int, int, int) { return SpreadExpr; }
    ExpressionType createObjectSpreadExpression(const JSTokenLocation&, ExpressionType, int, int, int) { return ObjectSpreadExpr; }
    TemplateString createTemplateString(const JSTokenLocation&, const Identifier*, const Identifier*) { return TemplateStringResult; }
    TemplateStringList createTemplateStringList(TemplateString) { return TemplateStringListResult; }
    TemplateStringList createTemplateStringList(TemplateStringList, TemplateString) { return TemplateStringListResult; }
    TemplateExpressionList createTemplateExpressionList(Expression) { return TemplateExpressionListResult; }
    TemplateExpressionList createTemplateExpressionList(TemplateExpressionList, Expression) { return TemplateExpressionListResult; }
    TemplateLiteral createTemplateLiteral(const JSTokenLocation&, TemplateStringList) { return TemplateExpr; }
    TemplateLiteral createTemplateLiteral(const JSTokenLocation&, TemplateStringList, TemplateExpressionList) { return TemplateExpr; }
    ExpressionType createTaggedTemplate(const JSTokenLocation&, ExpressionType, TemplateLiteral, int, int, int) { return TaggedTemplateExpr; }

    int createArgumentsList(const JSTokenLocation&, int) { return ArgumentsListResult; }
    int createArgumentsList(const JSTokenLocation&, int, int) { return ArgumentsListResult; }
    Property createProperty(const Identifier* name, int, PropertyNode::Type type, PropertyNode::PutType, bool complete, SuperBinding, InferName, ClassElementTag)
    {
        if (!complete)
            return Property(type);
        ASSERT(name);
        return Property(name, type);
    }
    Property createProperty(int, PropertyNode::Type type, PropertyNode::PutType, bool, SuperBinding, ClassElementTag)
    {
        return Property(type);
    }
    Property createProperty(VM* vm, ParserArena& parserArena, double name, int, PropertyNode::Type type, PropertyNode::PutType, bool complete, SuperBinding, ClassElementTag)
    {
        if (!complete)
            return Property(type);
        return Property(&parserArena.identifierArena().makeNumericIdentifier(vm, name), type);
    }
    Property createProperty(int, int, PropertyNode::Type type, PropertyNode::PutType, bool, SuperBinding, ClassElementTag)
    {
        return Property(type);
    }
    int createPropertyList(const JSTokenLocation&, Property) { return PropertyListResult; }
    int createPropertyList(const JSTokenLocation&, Property, int) { return PropertyListResult; }
    int createElementList(int, int) { return ElementsListResult; }
    int createElementList(int, int, int) { return ElementsListResult; }
    int createElementList(int) { return ElementsListResult; }
    int createFormalParameterList() { return FormalParameterListResult; }
    void appendParameter(int, DestructuringPattern, int) { }
    int createClause(int, int) { return ClauseResult; }
    int createClauseList(int) { return ClauseListResult; }
    int createClauseList(int, int) { return ClauseListResult; }
    int createFuncDeclStatement(const JSTokenLocation&, const ParserFunctionInfo<SyntaxChecker>&) { return StatementResult; }
    int createClassDeclStatement(const JSTokenLocation&, ClassExpression,
        const JSTextPosition&, const JSTextPosition&, int, int) { return StatementResult; }
    int createBlockStatement(const JSTokenLocation&, int, int, int, VariableEnvironment&, DeclarationStacks::FunctionStack&&) { return StatementResult; }
    int createExprStatement(const JSTokenLocation&, int, int, int) { return StatementResult; }
    int createIfStatement(const JSTokenLocation&, int, int, int, int) { return StatementResult; }
    int createIfStatement(const JSTokenLocation&, int, int, int, int, int) { return StatementResult; }
    int createForLoop(const JSTokenLocation&, int, int, int, int, int, int, VariableEnvironment&) { return StatementResult; }
    int createForInLoop(const JSTokenLocation&, int, int, int, const JSTokenLocation&, int, int, int, int, int, VariableEnvironment&) { return StatementResult; }
    int createForOfLoop(bool, const JSTokenLocation&, int, int, int, const JSTokenLocation&, int, int, int, int, int, VariableEnvironment&) { return StatementResult; }
    int createEmptyStatement(const JSTokenLocation&) { return StatementResult; }
    int createDeclarationStatement(const JSTokenLocation&, int, int, int) { return StatementResult; }
    int createReturnStatement(const JSTokenLocation&, int, int, int) { return StatementResult; }
    int createBreakStatement(const JSTokenLocation&, int, int) { return StatementResult; }
    int createBreakStatement(const JSTokenLocation&, const Identifier*, int, int) { return StatementResult; }
    int createContinueStatement(const JSTokenLocation&, int, int) { return StatementResult; }
    int createContinueStatement(const JSTokenLocation&, const Identifier*, int, int) { return StatementResult; }
    int createTryStatement(const JSTokenLocation&, int, int, int, int, int, int, VariableEnvironment&) { return StatementResult; }
    int createSwitchStatement(const JSTokenLocation&, int, int, int, int, int, int, VariableEnvironment&, DeclarationStacks::FunctionStack&&) { return StatementResult; }
    int createWhileStatement(const JSTokenLocation&, int, int, int, int) { return StatementResult; }
    int createWithStatement(const JSTokenLocation&, int, int, int, int, int, int) { return StatementResult; }
    int createDoWhileStatement(const JSTokenLocation&, int, int, int, int) { return StatementResult; }
    int createLabelStatement(const JSTokenLocation&, const Identifier*, int, int, int) { return StatementResult; }
    int createThrowStatement(const JSTokenLocation&, int, int, int) { return StatementResult; }
    int createDebugger(const JSTokenLocation&, int, int) { return StatementResult; }
    int createConstStatement(const JSTokenLocation&, int, int, int) { return StatementResult; }
    int createModuleName(const JSTokenLocation&, const Identifier&) { return ModuleNameResult; }
    ImportSpecifier createImportSpecifier(const JSTokenLocation&, const Identifier&, const Identifier&) { return ImportSpecifierResult; }
    ImportSpecifierList createImportSpecifierList() { return ImportSpecifierListResult; }
    void appendImportSpecifier(ImportSpecifierList, ImportSpecifier) { }
    int createImportDeclaration(const JSTokenLocation&, ImportSpecifierList, ModuleName) { return StatementResult; }
    int createExportAllDeclaration(const JSTokenLocation&, ModuleName) { return StatementResult; }
    int createExportDefaultDeclaration(const JSTokenLocation&, int, const Identifier&) { return StatementResult; }
    int createExportLocalDeclaration(const JSTokenLocation&, int) { return StatementResult; }
    int createExportNamedDeclaration(const JSTokenLocation&, ExportSpecifierList, ModuleName) { return StatementResult; }
    ExportSpecifier createExportSpecifier(const JSTokenLocation&, const Identifier&, const Identifier&) { return ExportSpecifierResult; }
    ExportSpecifierList createExportSpecifierList() { return ExportSpecifierListResult; }
    void appendExportSpecifier(ExportSpecifierList, ExportSpecifier) { }

    int appendConstDecl(const JSTokenLocation&, int, const Identifier*, int) { return StatementResult; }
    Property createGetterOrSetterProperty(const JSTokenLocation&, PropertyNode::Type type, bool strict, const Identifier* name, const ParserFunctionInfo<SyntaxChecker>&, ClassElementTag)
    {
        ASSERT(name);
        if (!strict)
            return Property(type);
        return Property(name, type);
    }
    Property createGetterOrSetterProperty(const JSTokenLocation&, PropertyNode::Type type, bool, int, const ParserFunctionInfo<SyntaxChecker>&, ClassElementTag)
    {
        return Property(type);
    }
    Property createGetterOrSetterProperty(VM* vm, ParserArena& parserArena, const JSTokenLocation&, PropertyNode::Type type, bool strict, double name, const ParserFunctionInfo<SyntaxChecker>&, ClassElementTag)
    {
        if (!strict)
            return Property(type);
        return Property(&parserArena.identifierArena().makeNumericIdentifier(vm, name), type);
    }

    void appendStatement(int, int) { }
    int combineCommaNodes(const JSTokenLocation&, int, int) { return CommaExpr; }
    int evalCount() const { return 0; }
    void appendBinaryExpressionInfo(int& operandStackDepth, int expr, int, int, int, bool)
    {
        if (!m_topBinaryExpr)
            m_topBinaryExpr = expr;
        else
            m_topBinaryExpr = BinaryExpr;
        operandStackDepth++;
    }
    
    // Logic to handle datastructures used during parsing of binary expressions
    void operatorStackPop(int& operatorStackDepth) { operatorStackDepth--; }
    bool operatorStackShouldReduce(int) { return true; }
    BinaryOperand getFromOperandStack(int) { return m_topBinaryExpr; }
    void shrinkOperandStackBy(int& operandStackDepth, int amount) { operandStackDepth -= amount; }
    void appendBinaryOperation(const JSTokenLocation&, int& operandStackDepth, int&, BinaryOperand, BinaryOperand) { operandStackDepth++; }
    void operatorStackAppend(int& operatorStackDepth, int, int) { operatorStackDepth++; }
    int popOperandStack(int&) { int res = m_topBinaryExpr; m_topBinaryExpr = 0; return res; }
    
    void appendUnaryToken(int& stackDepth, int tok, int) { stackDepth = 1; m_topUnaryToken = tok; }
    int unaryTokenStackLastType(int&) { return m_topUnaryToken; }
    JSTextPosition unaryTokenStackLastStart(int&) { return JSTextPosition(0, 0, 0); }
    void unaryTokenStackRemoveLast(int& stackDepth) { stackDepth = 0; }
    
    void assignmentStackAppend(int, int, int, int, int, Operator) { }
    int createAssignment(const JSTokenLocation&, int, int, int, int, int) { RELEASE_ASSERT_NOT_REACHED(); return AssignmentExpr; }
    const Identifier* getName(const Property& property) const { return property.name; }
    PropertyNode::Type getType(const Property& property) const { return property.type; }
    bool isResolve(ExpressionType expr) const { return expr == ResolveExpr || expr == ResolveEvalExpr; }
    ExpressionType createDestructuringAssignment(const JSTokenLocation&, int, ExpressionType)
    {
        return DestructuringAssignment;
    }
    
    ArrayPattern createArrayPattern(const JSTokenLocation&)
    {
        return ArrayDestructuring;
    }
    void appendArrayPatternSkipEntry(ArrayPattern, const JSTokenLocation&)
    {
    }
    void appendArrayPatternEntry(ArrayPattern, const JSTokenLocation&, DestructuringPattern, int)
    {
    }
    void appendArrayPatternRestEntry(ArrayPattern, const JSTokenLocation&, DestructuringPattern)
    {
    }
    void finishArrayPattern(ArrayPattern, const JSTextPosition&, const JSTextPosition&, const JSTextPosition&)
    {
    }
    ObjectPattern createObjectPattern(const JSTokenLocation&)
    {
        return ObjectDestructuring;
    }
    void appendObjectPatternEntry(ObjectPattern, const JSTokenLocation&, bool, const Identifier&, DestructuringPattern, int)
    {
    }
    void appendObjectPatternEntry(VM&, ObjectPattern, const JSTokenLocation&, Expression, DestructuringPattern, Expression)
    {
    }
    void appendObjectPatternRestEntry(VM&, ObjectPattern, const JSTokenLocation&, DestructuringPattern)
    {
    }
    void setContainsObjectRestElement(ObjectPattern, bool)
    {
    }
    void setContainsComputedProperty(ObjectPattern, bool)
    {
    }

    DestructuringPattern createBindingLocation(const JSTokenLocation&, const Identifier&, const JSTextPosition&, const JSTextPosition&, AssignmentContext)
    {
        return BindingDestructuring;
    }
    RestPattern createRestParameter(DestructuringPattern, size_t)
    { 
        return RestParameter;
    }
    DestructuringPattern createAssignmentElement(const Expression&, const JSTextPosition&, const JSTextPosition&)
    {
        return BindingDestructuring;
    }

    bool isBindingNode(DestructuringPattern pattern)
    {
        return pattern == BindingDestructuring;
    }

    bool isLocation(ExpressionType type)
    {
        return type == ResolveExpr || type == DotExpr || type == BracketExpr;
    }

    bool isAssignmentLocation(ExpressionType type)
    {
        return isLocation(type) || type == DestructuringAssignment;
    }

    bool isObjectLiteral(ExpressionType type)
    {
        return type == ObjectLiteralExpr;
    }

    bool isArrayLiteral(ExpressionType type)
    {
        return type == ArrayLiteralExpr;
    }

    bool isObjectOrArrayLiteral(ExpressionType type)
    {
        return isObjectLiteral(type) || isArrayLiteral(type);
    }

    bool isFunctionCall(ExpressionType type)
    {
        return type == CallExpr;
    }

    bool shouldSkipPauseLocation(int) const { return true; }

    void setEndOffset(int, int) { }
    int endOffset(int) { return 0; }
    void setStartOffset(int, int) { }

    JSTextPosition breakpointLocation(int) { return JSTextPosition(-1, 0, 0); }

    void propagateArgumentsUse() { }

private:
    int m_topBinaryExpr;
    int m_topUnaryToken;
};

} // namespace JSC
