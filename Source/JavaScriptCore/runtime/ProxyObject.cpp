/*
 * Copyright (C) 2016-2019 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ProxyObject.h"

#include "ArrayConstructor.h"
#include "Error.h"
#include "IdentifierInlines.h"
#include "JSCInlines.h"
#include "JSObjectInlines.h"
#include "ObjectConstructor.h"
#include "SlotVisitorInlines.h"
#include "StructureInlines.h"
#include "VMInlines.h"
#include <wtf/NoTailCalls.h>

// Note that we use NO_TAIL_CALLS() throughout this file because we rely on the machine stack
// growing larger for throwing OOM errors for when we have an effectively cyclic prototype chain.

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ProxyObject);

const ClassInfo ProxyObject::s_info = { "ProxyObject", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ProxyObject) };

ProxyObject::ProxyObject(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

String ProxyObject::toStringName(const JSObject* object, ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    const ProxyObject* proxy = jsCast<const ProxyObject*>(object);
    while (proxy) {
        const JSObject* target = proxy->target();
        bool targetIsArray = isArray(exec, target);
        if (UNLIKELY(scope.exception()))
            break;
        if (targetIsArray)
            RELEASE_AND_RETURN(scope, target->classInfo(vm)->methodTable.toStringName(target, exec));

        proxy = jsDynamicCast<const ProxyObject*>(vm, target);
    }
    return "Object"_s;
}

Structure* ProxyObject::structureForTarget(JSGlobalObject* globalObject, JSValue target)
{
    if (!target.isObject())
        return globalObject->proxyObjectStructure();

    JSObject* targetAsObject = jsCast<JSObject*>(target);
    CallData ignoredCallData;
    VM& vm = globalObject->vm();
    bool isCallable = targetAsObject->methodTable(vm)->getCallData(targetAsObject, ignoredCallData) != CallType::None;
    return isCallable ? globalObject->callableProxyObjectStructure() : globalObject->proxyObjectStructure();
}

void ProxyObject::finishCreation(VM& vm, ExecState* exec, JSValue target, JSValue handler)
{
    auto scope = DECLARE_THROW_SCOPE(vm);
    Base::finishCreation(vm);
    ASSERT(type() == ProxyObjectType);
    if (!target.isObject()) {
        throwTypeError(exec, scope, "A Proxy's 'target' should be an Object"_s);
        return;
    }
    if (ProxyObject* targetAsProxy = jsDynamicCast<ProxyObject*>(vm, target)) {
        if (targetAsProxy->isRevoked()) {
            throwTypeError(exec, scope, "A Proxy's 'target' shouldn't be a revoked Proxy"_s);
            return;
        }
    }
    if (!handler.isObject()) {
        throwTypeError(exec, scope, "A Proxy's 'handler' should be an Object"_s);
        return;
    }
    if (ProxyObject* handlerAsProxy = jsDynamicCast<ProxyObject*>(vm, handler)) {
        if (handlerAsProxy->isRevoked()) {
            throwTypeError(exec, scope, "A Proxy's 'handler' shouldn't be a revoked Proxy"_s);
            return;
        }
    }

    JSObject* targetAsObject = jsCast<JSObject*>(target);

    CallData ignoredCallData;
    m_isCallable = targetAsObject->methodTable(vm)->getCallData(targetAsObject, ignoredCallData) != CallType::None;
    if (m_isCallable) {
        TypeInfo info = structure(vm)->typeInfo();
        RELEASE_ASSERT(info.implementsHasInstance() && info.implementsDefaultHasInstance());
    }

    m_isConstructible = jsCast<JSObject*>(target)->isConstructor(vm);

    m_target.set(vm, this, targetAsObject);
    m_handler.set(vm, this, handler);
}

static const ASCIILiteral s_proxyAlreadyRevokedErrorMessage { "Proxy has already been revoked. No more operations are allowed to be performed on it"_s };

static JSValue performProxyGet(ExecState* exec, ProxyObject* proxyObject, JSValue receiver, PropertyName propertyName)
{
    NO_TAIL_CALLS();

    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(exec, scope);
        return { };
    }

    JSObject* target = proxyObject->target();

    auto performDefaultGet = [&] {
        scope.release();
        PropertySlot slot(receiver, PropertySlot::InternalMethodType::Get);
        bool hasProperty = target->getPropertySlot(exec, propertyName, slot);
        EXCEPTION_ASSERT(!scope.exception() || !hasProperty);
        if (hasProperty)
            RELEASE_AND_RETURN(scope, slot.getValue(exec, propertyName));

        return jsUndefined();
    };

    if (propertyName.isPrivateName())
        return jsUndefined();

    JSValue handlerValue = proxyObject->handler();
    if (handlerValue.isNull())
        return throwTypeError(exec, scope, s_proxyAlreadyRevokedErrorMessage);

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    CallType callType;
    JSValue getHandler = handler->getMethod(exec, callData, callType, vm.propertyNames->get, "'get' property of a Proxy's handler object should be callable"_s);
    RETURN_IF_EXCEPTION(scope, { });

    if (getHandler.isUndefined())
        return performDefaultGet();

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(identifierToSafePublicJSValue(vm, Identifier::fromUid(&vm, propertyName.uid())));
    arguments.append(receiver);
    ASSERT(!arguments.hasOverflowed());
    JSValue trapResult = call(exec, getHandler, callType, callData, handler, arguments);
    RETURN_IF_EXCEPTION(scope, { });

    PropertyDescriptor descriptor;
    bool result = target->getOwnPropertyDescriptor(exec, propertyName, descriptor);
    EXCEPTION_ASSERT(!scope.exception() || !result);
    if (result) {
        if (descriptor.isDataDescriptor() && !descriptor.configurable() && !descriptor.writable()) {
            if (!sameValue(exec, descriptor.value(), trapResult))
                return throwTypeError(exec, scope, "Proxy handler's 'get' result of a non-configurable and non-writable property should be the same value as the target's property"_s);
        } else if (descriptor.isAccessorDescriptor() && !descriptor.configurable() && descriptor.getter().isUndefined()) {
            if (!trapResult.isUndefined())
                return throwTypeError(exec, scope, "Proxy handler's 'get' result of a non-configurable accessor property without a getter should be undefined"_s);
        }
    }

    RETURN_IF_EXCEPTION(scope, { });

    return trapResult;
}

bool ProxyObject::performGet(ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    NO_TAIL_CALLS();

    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue result = performProxyGet(exec, this, slot.thisValue(), propertyName);
    RETURN_IF_EXCEPTION(scope, false);
    unsigned ignoredAttributes = 0;
    slot.setValue(this, ignoredAttributes, result);
    return true;
}

bool ProxyObject::performInternalMethodGetOwnProperty(ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    NO_TAIL_CALLS();

    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(exec, scope);
        return false;
    }
    JSObject* target = this->target();

    auto performDefaultGetOwnProperty = [&] {
        return target->methodTable(vm)->getOwnPropertySlot(target, exec, propertyName, slot);
    };

    if (propertyName.isPrivateName())
        return false;

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwVMTypeError(exec, scope, s_proxyAlreadyRevokedErrorMessage);
        return false;
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    CallType callType;
    JSValue getOwnPropertyDescriptorMethod = handler->getMethod(exec, callData, callType, makeIdentifier(vm, "getOwnPropertyDescriptor"), "'getOwnPropertyDescriptor' property of a Proxy's handler should be callable"_s);
    RETURN_IF_EXCEPTION(scope, false);
    if (getOwnPropertyDescriptorMethod.isUndefined())
        RELEASE_AND_RETURN(scope, performDefaultGetOwnProperty());

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(identifierToSafePublicJSValue(vm, Identifier::fromUid(&vm, propertyName.uid())));
    ASSERT(!arguments.hasOverflowed());
    JSValue trapResult = call(exec, getOwnPropertyDescriptorMethod, callType, callData, handler, arguments);
    RETURN_IF_EXCEPTION(scope, false);

    if (!trapResult.isUndefined() && !trapResult.isObject()) {
        throwVMTypeError(exec, scope, "result of 'getOwnPropertyDescriptor' call should either be an Object or undefined"_s);
        return false;
    }

    PropertyDescriptor targetPropertyDescriptor;
    bool isTargetPropertyDescriptorDefined = target->getOwnPropertyDescriptor(exec, propertyName, targetPropertyDescriptor);
    RETURN_IF_EXCEPTION(scope, false);

    if (trapResult.isUndefined()) {
        if (!isTargetPropertyDescriptorDefined)
            return false;
        if (!targetPropertyDescriptor.configurable()) {
            throwVMTypeError(exec, scope, "When the result of 'getOwnPropertyDescriptor' is undefined the target must be configurable"_s);
            return false;
        }
        // FIXME: this doesn't work if 'target' is another Proxy. We don't have isExtensible implemented in a way that fits w/ Proxys.
        // https://bugs.webkit.org/show_bug.cgi?id=154375
        bool isExtensible = target->isExtensible(exec);
        RETURN_IF_EXCEPTION(scope, false);
        if (!isExtensible) {
            // FIXME: Come up with a test for this error. I'm not sure how to because
            // Object.seal(o) will make all fields [[Configurable]] false.
            // https://bugs.webkit.org/show_bug.cgi?id=154376
            throwVMTypeError(exec, scope, "When 'getOwnPropertyDescriptor' returns undefined, the 'target' of a Proxy should be extensible"_s);
            return false;
        }

        return false;
    }

    bool isExtensible = target->isExtensible(exec);
    RETURN_IF_EXCEPTION(scope, false);
    PropertyDescriptor trapResultAsDescriptor;
    toPropertyDescriptor(exec, trapResult, trapResultAsDescriptor);
    RETURN_IF_EXCEPTION(scope, false);
    bool throwException = false;
    bool valid = validateAndApplyPropertyDescriptor(exec, nullptr, propertyName, isExtensible,
        trapResultAsDescriptor, isTargetPropertyDescriptorDefined, targetPropertyDescriptor, throwException);
    RETURN_IF_EXCEPTION(scope, false);
    if (!valid) {
        throwVMTypeError(exec, scope, "Result from 'getOwnPropertyDescriptor' fails the IsCompatiblePropertyDescriptor test"_s);
        return false;
    }

    if (!trapResultAsDescriptor.configurable()) {
        if (!isTargetPropertyDescriptorDefined || targetPropertyDescriptor.configurable()) {
            throwVMTypeError(exec, scope, "Result from 'getOwnPropertyDescriptor' can't be non-configurable when the 'target' doesn't have it as an own property or if it is a configurable own property on 'target'"_s);
            return false;
        }
        if (trapResultAsDescriptor.writablePresent() && !trapResultAsDescriptor.writable() && targetPropertyDescriptor.writable()) {
            throwVMTypeError(exec, scope, "Result from 'getOwnPropertyDescriptor' can't be non-configurable and non-writable when the target's property is writable"_s);
            return false;
        }
    }

    if (trapResultAsDescriptor.isAccessorDescriptor()) {
        GetterSetter* getterSetter = trapResultAsDescriptor.slowGetterSetter(exec);
        RETURN_IF_EXCEPTION(scope, false);
        slot.setGetterSlot(this, trapResultAsDescriptor.attributes(), getterSetter);
    } else if (trapResultAsDescriptor.isDataDescriptor() && !trapResultAsDescriptor.value().isEmpty())
        slot.setValue(this, trapResultAsDescriptor.attributes(), trapResultAsDescriptor.value());
    else
        slot.setValue(this, trapResultAsDescriptor.attributes(), jsUndefined()); // We use undefined because it's the default value in object properties.

    return true;
}

bool ProxyObject::performHasProperty(ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    NO_TAIL_CALLS();

    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(exec, scope);
        return false;
    }
    JSObject* target = this->target();
    slot.setValue(this, static_cast<unsigned>(PropertyAttribute::None), jsUndefined()); // Nobody should rely on our value, but be safe and protect against any bad actors reading our value.

    auto performDefaultHasProperty = [&] {
        return target->methodTable(vm)->getOwnPropertySlot(target, exec, propertyName, slot);
    };

    if (propertyName.isPrivateName())
        return false;

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwVMTypeError(exec, scope, s_proxyAlreadyRevokedErrorMessage);
        return false;
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    CallType callType;
    JSValue hasMethod = handler->getMethod(exec, callData, callType, vm.propertyNames->has, "'has' property of a Proxy's handler should be callable"_s);
    RETURN_IF_EXCEPTION(scope, false);
    if (hasMethod.isUndefined())
        RELEASE_AND_RETURN(scope, performDefaultHasProperty());

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(identifierToSafePublicJSValue(vm, Identifier::fromUid(&vm, propertyName.uid())));
    ASSERT(!arguments.hasOverflowed());
    JSValue trapResult = call(exec, hasMethod, callType, callData, handler, arguments);
    RETURN_IF_EXCEPTION(scope, false);

    bool trapResultAsBool = trapResult.toBoolean(exec);
    RETURN_IF_EXCEPTION(scope, false);

    if (!trapResultAsBool) {
        PropertyDescriptor descriptor;
        bool isPropertyDescriptorDefined = target->getOwnPropertyDescriptor(exec, propertyName, descriptor); 
        RETURN_IF_EXCEPTION(scope, false);
        if (isPropertyDescriptorDefined) {
            if (!descriptor.configurable()) {
                throwVMTypeError(exec, scope, "Proxy 'has' must return 'true' for non-configurable properties"_s);
                return false;
            }
            bool isExtensible = target->isExtensible(exec);
            RETURN_IF_EXCEPTION(scope, false);
            if (!isExtensible) {
                throwVMTypeError(exec, scope, "Proxy 'has' must return 'true' for a non-extensible 'target' object with a configurable property"_s);
                return false;
            }
        }
    }

    return trapResultAsBool;
}

bool ProxyObject::getOwnPropertySlotCommon(ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    slot.disableCaching();
    slot.setIsTaintedByOpaqueObject();

    if (slot.internalMethodType() == PropertySlot::InternalMethodType::VMInquiry)
        return false;

    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(exec, scope);
        return false;
    }
    switch (slot.internalMethodType()) {
    case PropertySlot::InternalMethodType::Get:
        RELEASE_AND_RETURN(scope, performGet(exec, propertyName, slot));
    case PropertySlot::InternalMethodType::GetOwnProperty:
        RELEASE_AND_RETURN(scope, performInternalMethodGetOwnProperty(exec, propertyName, slot));
    case PropertySlot::InternalMethodType::HasProperty:
        RELEASE_AND_RETURN(scope, performHasProperty(exec, propertyName, slot));
    default:
        return false;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

bool ProxyObject::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    ProxyObject* thisObject = jsCast<ProxyObject*>(object);
    return thisObject->getOwnPropertySlotCommon(exec, propertyName, slot);
}

bool ProxyObject::getOwnPropertySlotByIndex(JSObject* object, ExecState* exec, unsigned propertyName, PropertySlot& slot)
{
    ProxyObject* thisObject = jsCast<ProxyObject*>(object);
    Identifier ident = Identifier::from(exec, propertyName); 
    return thisObject->getOwnPropertySlotCommon(exec, ident.impl(), slot);
}

template <typename PerformDefaultPutFunction>
bool ProxyObject::performPut(ExecState* exec, JSValue putValue, JSValue thisValue, PropertyName propertyName, PerformDefaultPutFunction performDefaultPut, bool shouldThrow)
{
    NO_TAIL_CALLS();

    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(exec, scope);
        return false;
    }

    if (propertyName.isPrivateName())
        return false;

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwVMTypeError(exec, scope, s_proxyAlreadyRevokedErrorMessage);
        return false;
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    CallType callType;
    JSValue setMethod = handler->getMethod(exec, callData, callType, vm.propertyNames->set, "'set' property of a Proxy's handler should be callable"_s);
    RETURN_IF_EXCEPTION(scope, false);
    JSObject* target = this->target();
    if (setMethod.isUndefined())
        RELEASE_AND_RETURN(scope, performDefaultPut());

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(identifierToSafePublicJSValue(vm, Identifier::fromUid(&vm, propertyName.uid())));
    arguments.append(putValue);
    arguments.append(thisValue);
    ASSERT(!arguments.hasOverflowed());
    JSValue trapResult = call(exec, setMethod, callType, callData, handler, arguments);
    RETURN_IF_EXCEPTION(scope, false);
    bool trapResultAsBool = trapResult.toBoolean(exec);
    RETURN_IF_EXCEPTION(scope, false);
    if (!trapResultAsBool) {
        if (shouldThrow)
            throwVMTypeError(exec, scope, makeString("Proxy object's 'set' trap returned falsy value for property '", String(propertyName.uid()), "'"));
        return false;
    }

    PropertyDescriptor descriptor;
    bool hasProperty = target->getOwnPropertyDescriptor(exec, propertyName, descriptor);
    EXCEPTION_ASSERT(!scope.exception() || !hasProperty);
    if (hasProperty) {
        if (descriptor.isDataDescriptor() && !descriptor.configurable() && !descriptor.writable()) {
            if (!sameValue(exec, descriptor.value(), putValue)) {
                throwVMTypeError(exec, scope, "Proxy handler's 'set' on a non-configurable and non-writable property on 'target' should either return false or be the same value already on the 'target'"_s);
                return false;
            }
        } else if (descriptor.isAccessorDescriptor() && !descriptor.configurable() && descriptor.setter().isUndefined()) {
            throwVMTypeError(exec, scope, "Proxy handler's 'set' method on a non-configurable accessor property without a setter should return false"_s);
            return false;
        }
    }
    return true;
}

bool ProxyObject::put(JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = exec->vm();
    slot.disableCaching();

    ProxyObject* thisObject = jsCast<ProxyObject*>(cell);
    auto performDefaultPut = [&] () {
        JSObject* target = jsCast<JSObject*>(thisObject->target());
        return target->methodTable(vm)->put(target, exec, propertyName, value, slot);
    };
    return thisObject->performPut(exec, value, slot.thisValue(), propertyName, performDefaultPut, slot.isStrictMode());
}

bool ProxyObject::putByIndexCommon(ExecState* exec, JSValue thisValue, unsigned propertyName, JSValue putValue, bool shouldThrow)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    Identifier ident = Identifier::from(exec, propertyName);
    RETURN_IF_EXCEPTION(scope, false);
    auto performDefaultPut = [&] () {
        JSObject* target = this->target();
        bool isStrictMode = shouldThrow;
        PutPropertySlot slot(thisValue, isStrictMode); // We must preserve the "this" target of the putByIndex.
        return target->methodTable(vm)->put(target, exec, ident.impl(), putValue, slot);
    };
    RELEASE_AND_RETURN(scope, performPut(exec, putValue, thisValue, ident.impl(), performDefaultPut, shouldThrow));
}

bool ProxyObject::putByIndex(JSCell* cell, ExecState* exec, unsigned propertyName, JSValue value, bool shouldThrow)
{
    ProxyObject* thisObject = jsCast<ProxyObject*>(cell);
    return thisObject->putByIndexCommon(exec, thisObject, propertyName, value, shouldThrow);
}

static EncodedJSValue JSC_HOST_CALL performProxyCall(ExecState* exec)
{
    NO_TAIL_CALLS();

    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(exec, scope);
        return encodedJSValue();
    }
    ProxyObject* proxy = jsCast<ProxyObject*>(exec->jsCallee());
    JSValue handlerValue = proxy->handler();
    if (handlerValue.isNull())
        return throwVMTypeError(exec, scope, s_proxyAlreadyRevokedErrorMessage);

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    CallType callType;
    JSValue applyMethod = handler->getMethod(exec, callData, callType, makeIdentifier(vm, "apply"), "'apply' property of a Proxy's handler should be callable"_s);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    JSObject* target = proxy->target();
    if (applyMethod.isUndefined()) {
        CallData callData;
        CallType callType = target->methodTable(vm)->getCallData(target, callData);
        RELEASE_ASSERT(callType != CallType::None);
        RELEASE_AND_RETURN(scope, JSValue::encode(call(exec, target, callType, callData, exec->thisValue(), ArgList(exec))));
    }

    JSArray* argArray = constructArray(exec, static_cast<ArrayAllocationProfile*>(nullptr), ArgList(exec));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(exec->thisValue().toThis(exec, ECMAMode::StrictMode));
    arguments.append(argArray);
    ASSERT(!arguments.hasOverflowed());
    RELEASE_AND_RETURN(scope, JSValue::encode(call(exec, applyMethod, callType, callData, handler, arguments)));
}

CallType ProxyObject::getCallData(JSCell* cell, CallData& callData)
{
    ProxyObject* proxy = jsCast<ProxyObject*>(cell);
    if (!proxy->m_isCallable) {
        callData.js.functionExecutable = nullptr;
        callData.js.scope = nullptr;
        return CallType::None;
    }

    callData.native.function = performProxyCall;
    return CallType::Host;
}

static EncodedJSValue JSC_HOST_CALL performProxyConstruct(ExecState* exec)
{
    NO_TAIL_CALLS();

    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(exec, scope);
        return encodedJSValue();
    }
    ProxyObject* proxy = jsCast<ProxyObject*>(exec->jsCallee());
    JSValue handlerValue = proxy->handler();
    if (handlerValue.isNull())
        return throwVMTypeError(exec, scope, s_proxyAlreadyRevokedErrorMessage);

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    CallType callType;
    JSValue constructMethod = handler->getMethod(exec, callData, callType, makeIdentifier(vm, "construct"), "'construct' property of a Proxy's handler should be callable"_s);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    JSObject* target = proxy->target();
    if (constructMethod.isUndefined()) {
        ConstructData constructData;
        ConstructType constructType = target->methodTable(vm)->getConstructData(target, constructData);
        RELEASE_ASSERT(constructType != ConstructType::None);
        RELEASE_AND_RETURN(scope, JSValue::encode(construct(exec, target, constructType, constructData, ArgList(exec), exec->newTarget())));
    }

    JSArray* argArray = constructArray(exec, static_cast<ArrayAllocationProfile*>(nullptr), ArgList(exec));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(argArray);
    arguments.append(exec->newTarget());
    ASSERT(!arguments.hasOverflowed());
    JSValue result = call(exec, constructMethod, callType, callData, handler, arguments);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (!result.isObject())
        return throwVMTypeError(exec, scope, "Result from Proxy handler's 'construct' method should be an object"_s);
    return JSValue::encode(result);
}

ConstructType ProxyObject::getConstructData(JSCell* cell, ConstructData& constructData)
{
    ProxyObject* proxy = jsCast<ProxyObject*>(cell);
    if (!proxy->m_isConstructible) {
        constructData.js.functionExecutable = nullptr;
        constructData.js.scope = nullptr;
        return ConstructType::None;
    }

    constructData.native.function = performProxyConstruct;
    return ConstructType::Host;
}

template <typename DefaultDeleteFunction>
bool ProxyObject::performDelete(ExecState* exec, PropertyName propertyName, DefaultDeleteFunction performDefaultDelete)
{
    NO_TAIL_CALLS();

    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(exec, scope);
        return false;
    }

    if (propertyName.isPrivateName())
        return false;

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwVMTypeError(exec, scope, s_proxyAlreadyRevokedErrorMessage);
        return false;
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    CallType callType;
    JSValue deletePropertyMethod = handler->getMethod(exec, callData, callType, makeIdentifier(vm, "deleteProperty"), "'deleteProperty' property of a Proxy's handler should be callable"_s);
    RETURN_IF_EXCEPTION(scope, false);
    JSObject* target = this->target();
    if (deletePropertyMethod.isUndefined())
        RELEASE_AND_RETURN(scope, performDefaultDelete());

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(identifierToSafePublicJSValue(vm, Identifier::fromUid(&vm, propertyName.uid())));
    ASSERT(!arguments.hasOverflowed());
    JSValue trapResult = call(exec, deletePropertyMethod, callType, callData, handler, arguments);
    RETURN_IF_EXCEPTION(scope, false);

    bool trapResultAsBool = trapResult.toBoolean(exec);
    RETURN_IF_EXCEPTION(scope, false);

    if (!trapResultAsBool)
        return false;

    PropertyDescriptor descriptor;
    bool result = target->getOwnPropertyDescriptor(exec, propertyName, descriptor);
    EXCEPTION_ASSERT(!scope.exception() || !result);
    if (result) {
        if (!descriptor.configurable()) {
            throwVMTypeError(exec, scope, "Proxy handler's 'deleteProperty' method should return false when the target's property is not configurable"_s);
            return false;
        }
        bool targetIsExtensible = target->isExtensible(exec);
        RETURN_IF_EXCEPTION(scope, false);
        if (!targetIsExtensible) {
            throwVMTypeError(exec, scope, "Proxy handler's 'deleteProperty' method should return false when the target has property and is not extensible"_s);
            return false;
        }
    }

    RETURN_IF_EXCEPTION(scope, false);

    return true;
}

bool ProxyObject::deleteProperty(JSCell* cell, ExecState* exec, PropertyName propertyName)
{
    ProxyObject* thisObject = jsCast<ProxyObject*>(cell);
    auto performDefaultDelete = [&] () -> bool {
        JSObject* target = thisObject->target();
        return target->methodTable(exec->vm())->deleteProperty(target, exec, propertyName);
    };
    return thisObject->performDelete(exec, propertyName, performDefaultDelete);
}

bool ProxyObject::deletePropertyByIndex(JSCell* cell, ExecState* exec, unsigned propertyName)
{
    ProxyObject* thisObject = jsCast<ProxyObject*>(cell);
    Identifier ident = Identifier::from(exec, propertyName); 
    auto performDefaultDelete = [&] () -> bool {
        JSObject* target = thisObject->target();
        return target->methodTable(exec->vm())->deletePropertyByIndex(target, exec, propertyName);
    };
    return thisObject->performDelete(exec, ident.impl(), performDefaultDelete);
}

bool ProxyObject::performPreventExtensions(ExecState* exec)
{
    NO_TAIL_CALLS();

    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(exec, scope);
        return false;
    }

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwVMTypeError(exec, scope, s_proxyAlreadyRevokedErrorMessage);
        return false;
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    CallType callType;
    JSValue preventExtensionsMethod = handler->getMethod(exec, callData, callType, makeIdentifier(vm, "preventExtensions"), "'preventExtensions' property of a Proxy's handler should be callable"_s);
    RETURN_IF_EXCEPTION(scope, false);
    JSObject* target = this->target();
    if (preventExtensionsMethod.isUndefined())
        RELEASE_AND_RETURN(scope, target->methodTable(vm)->preventExtensions(target, exec));

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    ASSERT(!arguments.hasOverflowed());
    JSValue trapResult = call(exec, preventExtensionsMethod, callType, callData, handler, arguments);
    RETURN_IF_EXCEPTION(scope, false);

    bool trapResultAsBool = trapResult.toBoolean(exec);
    RETURN_IF_EXCEPTION(scope, false);

    if (trapResultAsBool) {
        bool targetIsExtensible = target->isExtensible(exec);
        RETURN_IF_EXCEPTION(scope, false);
        if (targetIsExtensible) {
            throwVMTypeError(exec, scope, "Proxy's 'preventExtensions' trap returned true even though its target is extensible. It should have returned false"_s);
            return false;
        }
    }

    return trapResultAsBool;
}

bool ProxyObject::preventExtensions(JSObject* object, ExecState* exec)
{
    return jsCast<ProxyObject*>(object)->performPreventExtensions(exec);
}

bool ProxyObject::performIsExtensible(ExecState* exec)
{
    NO_TAIL_CALLS();

    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(exec, scope);
        return false;
    }

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwVMTypeError(exec, scope, s_proxyAlreadyRevokedErrorMessage);
        return false;
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    CallType callType;
    JSValue isExtensibleMethod = handler->getMethod(exec, callData, callType, makeIdentifier(vm, "isExtensible"), "'isExtensible' property of a Proxy's handler should be callable"_s);
    RETURN_IF_EXCEPTION(scope, false);

    JSObject* target = this->target();
    if (isExtensibleMethod.isUndefined())
        RELEASE_AND_RETURN(scope, target->isExtensible(exec));

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    ASSERT(!arguments.hasOverflowed());
    JSValue trapResult = call(exec, isExtensibleMethod, callType, callData, handler, arguments);
    RETURN_IF_EXCEPTION(scope, false);

    bool trapResultAsBool = trapResult.toBoolean(exec);
    RETURN_IF_EXCEPTION(scope, false);

    bool isTargetExtensible = target->isExtensible(exec);
    RETURN_IF_EXCEPTION(scope, false);

    if (trapResultAsBool != isTargetExtensible) {
        if (isTargetExtensible) {
            ASSERT(!trapResultAsBool);
            throwVMTypeError(exec, scope, "Proxy object's 'isExtensible' trap returned false when the target is extensible. It should have returned true"_s);
        } else {
            ASSERT(!isTargetExtensible);
            ASSERT(trapResultAsBool);
            throwVMTypeError(exec, scope, "Proxy object's 'isExtensible' trap returned true when the target is non-extensible. It should have returned false"_s);
        }
    }
    
    return trapResultAsBool;
}

bool ProxyObject::isExtensible(JSObject* object, ExecState* exec)
{
    return jsCast<ProxyObject*>(object)->performIsExtensible(exec);
}

bool ProxyObject::performDefineOwnProperty(ExecState* exec, PropertyName propertyName, const PropertyDescriptor& descriptor, bool shouldThrow)
{
    NO_TAIL_CALLS();

    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(exec, scope);
        return false;
    }

    JSObject* target = this->target();
    auto performDefaultDefineOwnProperty = [&] {
        RELEASE_AND_RETURN(scope, target->methodTable(vm)->defineOwnProperty(target, exec, propertyName, descriptor, shouldThrow));
    };

    if (propertyName.isPrivateName())
        return false;

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwVMTypeError(exec, scope, s_proxyAlreadyRevokedErrorMessage);
        return false;
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    CallType callType;
    JSValue definePropertyMethod = handler->getMethod(exec, callData, callType, vm.propertyNames->defineProperty, "'defineProperty' property of a Proxy's handler should be callable"_s);
    RETURN_IF_EXCEPTION(scope, false);

    if (definePropertyMethod.isUndefined())
        return performDefaultDefineOwnProperty();

    JSObject* descriptorObject = constructObjectFromPropertyDescriptor(exec, descriptor);
    RETURN_IF_EXCEPTION(scope, false);

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(identifierToSafePublicJSValue(vm, Identifier::fromUid(&vm, propertyName.uid())));
    arguments.append(descriptorObject);
    ASSERT(!arguments.hasOverflowed());
    JSValue trapResult = call(exec, definePropertyMethod, callType, callData, handler, arguments);
    RETURN_IF_EXCEPTION(scope, false);

    bool trapResultAsBool = trapResult.toBoolean(exec);
    RETURN_IF_EXCEPTION(scope, false);

    if (!trapResultAsBool)
        return false;

    PropertyDescriptor targetDescriptor;
    bool isTargetDescriptorDefined = target->getOwnPropertyDescriptor(exec, propertyName, targetDescriptor);
    RETURN_IF_EXCEPTION(scope, false);

    bool targetIsExtensible = target->isExtensible(exec);
    RETURN_IF_EXCEPTION(scope, false);
    bool settingConfigurableToFalse = descriptor.configurablePresent() && !descriptor.configurable();

    if (!isTargetDescriptorDefined) {
        if (!targetIsExtensible) {
            throwVMTypeError(exec, scope, "Proxy's 'defineProperty' trap returned true even though getOwnPropertyDescriptor of the Proxy's target returned undefined and the target is non-extensible"_s);
            return false;
        }
        if (settingConfigurableToFalse) {
            throwVMTypeError(exec, scope, "Proxy's 'defineProperty' trap returned true for a non-configurable field even though getOwnPropertyDescriptor of the Proxy's target returned undefined"_s);
            return false;
        }

        return true;
    } 

    ASSERT(isTargetDescriptorDefined);
    bool isCurrentDefined = isTargetDescriptorDefined;
    const PropertyDescriptor& current = targetDescriptor;
    bool throwException = false;
    bool isCompatibleDescriptor = validateAndApplyPropertyDescriptor(exec, nullptr, propertyName, targetIsExtensible, descriptor, isCurrentDefined, current, throwException);
    RETURN_IF_EXCEPTION(scope, false);    
    if (!isCompatibleDescriptor) {
        throwVMTypeError(exec, scope, "Proxy's 'defineProperty' trap did not define a property on its target that is compatible with the trap's input descriptor"_s);
        return false;
    }
    if (settingConfigurableToFalse && targetDescriptor.configurable()) {
        throwVMTypeError(exec, scope, "Proxy's 'defineProperty' trap did not define a non-configurable property on its target even though the input descriptor to the trap said it must do so"_s);
        return false;
    }
    if (targetDescriptor.isDataDescriptor() && !targetDescriptor.configurable() && targetDescriptor.writable()) {
        if (descriptor.writablePresent() && !descriptor.writable()) {
            throwTypeError(exec, scope, "Proxy's 'defineProperty' trap returned true for a non-writable input descriptor when the target's property is non-configurable and writable"_s);
            return false;
        }
    }
    
    return true;
}

bool ProxyObject::defineOwnProperty(JSObject* object, ExecState* exec, PropertyName propertyName, const PropertyDescriptor& descriptor, bool shouldThrow)
{
    ProxyObject* thisObject = jsCast<ProxyObject*>(object);
    return thisObject->performDefineOwnProperty(exec, propertyName, descriptor, shouldThrow);
}

void ProxyObject::performGetOwnPropertyNames(ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode enumerationMode)
{
    NO_TAIL_CALLS();

    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(exec, scope);
        return;
    }
    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwVMTypeError(exec, scope, s_proxyAlreadyRevokedErrorMessage);
        return;
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    CallType callType;
    JSValue ownKeysMethod = handler->getMethod(exec, callData, callType, makeIdentifier(vm, "ownKeys"), "'ownKeys' property of a Proxy's handler should be callable"_s);
    RETURN_IF_EXCEPTION(scope, void());
    JSObject* target = this->target();
    if (ownKeysMethod.isUndefined()) {
        scope.release();
        target->methodTable(vm)->getOwnPropertyNames(target, exec, propertyNames, enumerationMode);
        return;
    }

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    ASSERT(!arguments.hasOverflowed());
    JSValue arrayLikeObject = call(exec, ownKeysMethod, callType, callData, handler, arguments);
    RETURN_IF_EXCEPTION(scope, void());

    PropertyNameArray trapResult(&vm, propertyNames.propertyNameMode(), propertyNames.privateSymbolMode());
    HashSet<UniquedStringImpl*> uncheckedResultKeys;
    {
        HashSet<RefPtr<UniquedStringImpl>> seenKeys;

        RuntimeTypeMask resultFilter = 0;
        switch (propertyNames.propertyNameMode()) {
        case PropertyNameMode::Symbols:
            resultFilter = TypeSymbol;
            break;
        case PropertyNameMode::Strings:
            resultFilter = TypeString;
            break;
        case PropertyNameMode::StringsAndSymbols:
            resultFilter = TypeSymbol | TypeString;
            break;
        }
        ASSERT(resultFilter);

        auto addPropName = [&] (JSValue value, RuntimeType type) -> bool {
            static const bool doExitEarly = true;
            static const bool dontExitEarly = false;

            Identifier ident = value.toPropertyKey(exec);
            RETURN_IF_EXCEPTION(scope, doExitEarly);

            // If trapResult contains any duplicate entries, throw a TypeError exception.
            //    
            // Per spec[1], filtering by type should occur _after_ [[OwnPropertyKeys]], so duplicates
            // are tracked in a separate hashtable from uncheckedResultKeys (which only contain the
            // keys filtered by type).
            //
            // [1] Per https://tc39.github.io/ecma262/#sec-proxy-object-internal-methods-and-internal-slots-ownpropertykeysmust not contain any duplicate names"_s);
            if (!seenKeys.add(ident.impl()).isNewEntry) {
                throwTypeError(exec, scope, "Proxy handler's 'ownKeys' trap result must not contain any duplicate names"_s);
                return doExitEarly;
            }

            if (!(type & resultFilter))
                return dontExitEarly;

            uncheckedResultKeys.add(ident.impl());
            trapResult.add(ident.impl());
            return dontExitEarly;
        };

        RuntimeTypeMask dontThrowAnExceptionTypeFilter = TypeString | TypeSymbol;
        createListFromArrayLike(exec, arrayLikeObject, dontThrowAnExceptionTypeFilter, "Proxy handler's 'ownKeys' method must return an object"_s, "Proxy handler's 'ownKeys' method must return an array-like object containing only Strings and Symbols"_s, addPropName);
        RETURN_IF_EXCEPTION(scope, void());
    }

    bool targetIsExensible = target->isExtensible(exec);
    RETURN_IF_EXCEPTION(scope, void());

    PropertyNameArray targetKeys(&vm, propertyNames.propertyNameMode(), propertyNames.privateSymbolMode());
    target->methodTable(vm)->getOwnPropertyNames(target, exec, targetKeys, enumerationMode);
    RETURN_IF_EXCEPTION(scope, void());
    Vector<UniquedStringImpl*> targetConfigurableKeys;
    Vector<UniquedStringImpl*> targetNonConfigurableKeys;
    for (const Identifier& ident : targetKeys) {
        PropertyDescriptor descriptor;
        bool isPropertyDefined = target->getOwnPropertyDescriptor(exec, ident.impl(), descriptor); 
        RETURN_IF_EXCEPTION(scope, void());
        if (isPropertyDefined && !descriptor.configurable())
            targetNonConfigurableKeys.append(ident.impl());
        else
            targetConfigurableKeys.append(ident.impl());
    }

    enum ContainedIn { IsContainedIn, IsNotContainedIn };
    auto removeIfContainedInUncheckedResultKeys = [&] (UniquedStringImpl* impl) -> ContainedIn {
        auto iter = uncheckedResultKeys.find(impl);
        if (iter == uncheckedResultKeys.end())
            return IsNotContainedIn;

        uncheckedResultKeys.remove(iter);
        return IsContainedIn;
    };

    for (UniquedStringImpl* impl : targetNonConfigurableKeys) {
        if (removeIfContainedInUncheckedResultKeys(impl) == IsNotContainedIn) {
            throwVMTypeError(exec, scope, makeString("Proxy object's 'target' has the non-configurable property '", String(impl), "' that was not in the result from the 'ownKeys' trap"));
            return;
        }
    }

    if (!targetIsExensible) {
        for (UniquedStringImpl* impl : targetConfigurableKeys) {
            if (removeIfContainedInUncheckedResultKeys(impl) == IsNotContainedIn) {
                throwVMTypeError(exec, scope, makeString("Proxy object's non-extensible 'target' has configurable property '", String(impl), "' that was not in the result from the 'ownKeys' trap"));
                return;
            }
        }

        if (uncheckedResultKeys.size()) {
            throwVMTypeError(exec, scope, "Proxy handler's 'ownKeys' method returned a key that was not present in its non-extensible target"_s);
            return;
        }
    }

    if (!enumerationMode.includeDontEnumProperties()) {
        // Filtering DontEnum properties is observable in proxies and must occur following the invariant checks above.
        for (auto propertyName : trapResult) {
            PropertySlot slot(this, PropertySlot::InternalMethodType::GetOwnProperty);
            auto result = getOwnPropertySlotCommon(exec, propertyName, slot);
            RETURN_IF_EXCEPTION(scope, void());
            if (!result)
                continue;
            if (slot.attributes() & PropertyAttribute::DontEnum)
                continue;
            propertyNames.add(propertyName.impl());
        }
    } else {
        for (auto propertyName : trapResult)
            propertyNames.add(propertyName.impl());
    }
}

void ProxyObject::getOwnPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNameArray, EnumerationMode enumerationMode)
{
    ProxyObject* thisObject = jsCast<ProxyObject*>(object);
    thisObject->performGetOwnPropertyNames(exec, propertyNameArray, enumerationMode);
}

void ProxyObject::getPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNameArray, EnumerationMode enumerationMode)
{
    NO_TAIL_CALLS();
    JSObject::getPropertyNames(object, exec, propertyNameArray, enumerationMode);
}

void ProxyObject::getOwnNonIndexPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void ProxyObject::getStructurePropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode)
{
    // We should always go down the getOwnPropertyNames path.
    RELEASE_ASSERT_NOT_REACHED();
}

void ProxyObject::getGenericPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode)
{
    RELEASE_ASSERT_NOT_REACHED();
}

bool ProxyObject::performSetPrototype(ExecState* exec, JSValue prototype, bool shouldThrowIfCantSet)
{
    NO_TAIL_CALLS();

    ASSERT(prototype.isObject() || prototype.isNull());

    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(exec, scope);
        return false;
    }

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwVMTypeError(exec, scope, s_proxyAlreadyRevokedErrorMessage);
        return false;
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    CallType callType;
    JSValue setPrototypeOfMethod = handler->getMethod(exec, callData, callType, makeIdentifier(vm, "setPrototypeOf"), "'setPrototypeOf' property of a Proxy's handler should be callable"_s);
    RETURN_IF_EXCEPTION(scope, false);

    JSObject* target = this->target();
    if (setPrototypeOfMethod.isUndefined())
        RELEASE_AND_RETURN(scope, target->setPrototype(vm, exec, prototype, shouldThrowIfCantSet));

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    arguments.append(prototype);
    ASSERT(!arguments.hasOverflowed());
    JSValue trapResult = call(exec, setPrototypeOfMethod, callType, callData, handler, arguments);
    RETURN_IF_EXCEPTION(scope, false);

    bool trapResultAsBool = trapResult.toBoolean(exec);
    RETURN_IF_EXCEPTION(scope, false);
    
    if (!trapResultAsBool) {
        if (shouldThrowIfCantSet)
            throwVMTypeError(exec, scope, "Proxy 'setPrototypeOf' returned false indicating it could not set the prototype value. The operation was expected to succeed"_s);
        return false;
    }

    bool targetIsExtensible = target->isExtensible(exec);
    RETURN_IF_EXCEPTION(scope, false);
    if (targetIsExtensible)
        return true;

    JSValue targetPrototype = target->getPrototype(vm, exec);
    RETURN_IF_EXCEPTION(scope, false);
    if (!sameValue(exec, prototype, targetPrototype)) {
        throwVMTypeError(exec, scope, "Proxy 'setPrototypeOf' trap returned true when its target is non-extensible and the new prototype value is not the same as the current prototype value. It should have returned false"_s);
        return false;
    }

    return true;
}

bool ProxyObject::setPrototype(JSObject* object, ExecState* exec, JSValue prototype, bool shouldThrowIfCantSet)
{
    return jsCast<ProxyObject*>(object)->performSetPrototype(exec, prototype, shouldThrowIfCantSet);
}

JSValue ProxyObject::performGetPrototype(ExecState* exec)
{
    NO_TAIL_CALLS();

    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (UNLIKELY(!vm.isSafeToRecurseSoft())) {
        throwStackOverflowError(exec, scope);
        return { };
    }

    JSValue handlerValue = this->handler();
    if (handlerValue.isNull()) {
        throwVMTypeError(exec, scope, s_proxyAlreadyRevokedErrorMessage);
        return { };
    }

    JSObject* handler = jsCast<JSObject*>(handlerValue);
    CallData callData;
    CallType callType;
    JSValue getPrototypeOfMethod = handler->getMethod(exec, callData, callType, makeIdentifier(vm, "getPrototypeOf"), "'getPrototypeOf' property of a Proxy's handler should be callable"_s);
    RETURN_IF_EXCEPTION(scope, { });

    JSObject* target = this->target();
    if (getPrototypeOfMethod.isUndefined()) 
        RELEASE_AND_RETURN(scope, target->getPrototype(vm, exec));

    MarkedArgumentBuffer arguments;
    arguments.append(target);
    ASSERT(!arguments.hasOverflowed());
    JSValue trapResult = call(exec, getPrototypeOfMethod, callType, callData, handler, arguments);
    RETURN_IF_EXCEPTION(scope, { });

    if (!trapResult.isObject() && !trapResult.isNull()) {
        throwVMTypeError(exec, scope, "Proxy handler's 'getPrototypeOf' trap should either return an object or null"_s);
        return { };
    }

    bool targetIsExtensible = target->isExtensible(exec);
    RETURN_IF_EXCEPTION(scope, { });
    if (targetIsExtensible)
        return trapResult;

    JSValue targetPrototype = target->getPrototype(vm, exec);
    RETURN_IF_EXCEPTION(scope, { });
    if (!sameValue(exec, targetPrototype, trapResult)) {
        throwVMTypeError(exec, scope, "Proxy's 'getPrototypeOf' trap for a non-extensible target should return the same value as the target's prototype"_s);
        return { };
    }

    return trapResult;
}

JSValue ProxyObject::getPrototype(JSObject* object, ExecState* exec)
{
    return jsCast<ProxyObject*>(object)->performGetPrototype(exec);
}

void ProxyObject::revoke(VM& vm)
{ 
    // This should only ever be called once and we should strictly transition from Object to null.
    RELEASE_ASSERT(!m_handler.get().isNull() && m_handler.get().isObject());
    m_handler.set(vm, this, jsNull());
}

bool ProxyObject::isRevoked() const
{
    return handler().isNull();
}

void ProxyObject::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    ProxyObject* thisObject = jsCast<ProxyObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    visitor.append(thisObject->m_target);
    visitor.append(thisObject->m_handler);
}

} // namespace JSC
