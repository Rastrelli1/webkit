/*
 * Copyright (C) 2007, 2014-2015 Apple Inc.  All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//# sourceURL=__InjectedScript_InjectedScriptSource.js

(function (InjectedScriptHost, inspectedGlobalObject, injectedScriptId) {

// FIXME: <https://webkit.org/b/152294> Web Inspector: Parse InjectedScriptSource as a built-in to get guaranteed non-user-overridden built-ins

var Object = {}.constructor;

function toString(obj)
{
    return String(obj);
}

function toStringDescription(obj)
{
    if (obj === 0 && 1 / obj < 0)
        return "-0";

    if (isBigInt(obj))
        return toString(obj) + "n";

    return toString(obj);
}

function isUInt32(obj)
{
    if (typeof obj === "number")
        return obj >>> 0 === obj && (obj > 0 || 1 / obj > 0);
    return "" + (obj >>> 0) === obj;
}

function isSymbol(value)
{
    return typeof value === "symbol";
}

function isBigInt(value)
{
    return typeof value === "bigint";
}

function isEmptyObject(object)
{
    for (let key in object)
        return false;
    return true;
}

function isDefined(value)
{
    return !!value || InjectedScriptHost.isHTMLAllCollection(value);
}

function isPrimitiveValue(value)
{
    switch (typeof value) {
    case "boolean":
    case "number":
    case "string":
        return true;
    case "undefined":
        return !InjectedScriptHost.isHTMLAllCollection(value);
    default:
        return false;
    }
}

// -------

let InjectedScript = class InjectedScript
{
    constructor()
    {
        this._lastBoundObjectId = 1;
        this._idToWrappedObject = {};
        this._idToObjectGroupName = {};
        this._objectGroups = {};
        this._modules = {};
        this._nextSavedResultIndex = 1;
        this._savedResults = [];
    }

    // InjectedScript C++ API

    execute(functionString, objectGroup, includeCommandLineAPI, returnByValue, generatePreview, saveResult, args)
    {
        return this._wrapAndSaveCall(objectGroup, returnByValue, generatePreview, saveResult, () => {
            const isEvalOnCallFrame = false;
            return this._evaluateOn(InjectedScriptHost.evaluateWithScopeExtension, InjectedScriptHost, functionString, isEvalOnCallFrame, includeCommandLineAPI).apply(undefined, args);
        });
    }

    evaluate(expression, objectGroup, includeCommandLineAPI, returnByValue, generatePreview, saveResult)
    {
        const isEvalOnCallFrame = false;
        return this._evaluateAndWrap(InjectedScriptHost.evaluateWithScopeExtension, InjectedScriptHost, expression, objectGroup, isEvalOnCallFrame, includeCommandLineAPI, returnByValue, generatePreview, saveResult);
    }

    awaitPromise(promiseObjectId, returnByValue, generatePreview, saveResult, callback)
    {
        let parsedPromiseObjectId = this._parseObjectId(promiseObjectId);
        let promiseObject = this._objectForId(parsedPromiseObjectId);
        let promiseObjectGroupName = this._idToObjectGroupName[parsedPromiseObjectId.id];

        if (!isDefined(promiseObject)) {
            callback("Could not find object with given id");
            return;
        }

        if (!(promiseObject instanceof Promise)) {
            callback("Object with given id is not a Promise");
            return;
        }

        let resolve = (value) => {
            let returnObject = {
                wasThrown: false,
                result: RemoteObject.create(value, promiseObjectGroupName, returnByValue, generatePreview),
            };

            if (saveResult) {
                this._savedResultIndex = 0;
                this._saveResult(returnObject.result);
                if (this._savedResultIndex)
                    returnObject.savedResultIndex = this._savedResultIndex;
            }

            callback(returnObject);
        };
        let reject = (reason) => {
            callback(this._createThrownValue(reason, promiseObjectGroupName));
        };
        promiseObject.then(resolve, reject);
    }

    evaluateOnCallFrame(topCallFrame, callFrameId, expression, objectGroup, includeCommandLineAPI, returnByValue, generatePreview, saveResult)
    {
        let callFrame = this._callFrameForId(topCallFrame, callFrameId);
        if (!callFrame)
            return "Could not find call frame with given id";
        const isEvalOnCallFrame = true;
        return this._evaluateAndWrap(callFrame.evaluateWithScopeExtension, callFrame, expression, objectGroup, isEvalOnCallFrame, includeCommandLineAPI, returnByValue, generatePreview, saveResult);
    }

    callFunctionOn(objectId, expression, args, returnByValue, generatePreview)
    {
        let parsedObjectId = this._parseObjectId(objectId);
        let object = this._objectForId(parsedObjectId);
        let objectGroupName = this._idToObjectGroupName[parsedObjectId.id];

        if (!isDefined(object))
            return "Could not find object with given id";

        let resolvedArgs = [];
        if (args) {
            let callArgs = InjectedScriptHost.evaluate(args);
            for (let i = 0; i < callArgs.length; ++i) {
                try {
                    resolvedArgs[i] = this._resolveCallArgument(callArgs[i]);
                } catch (e) {
                    return String(e);
                }
            }
        }

        try {
            let func = InjectedScriptHost.evaluate("(" + expression + ")");
            if (typeof func !== "function")
                return "Given expression does not evaluate to a function";

            return {
                wasThrown: false,
                result: RemoteObject.create(func.apply(object, resolvedArgs), objectGroupName, returnByValue, generatePreview)
            };
        } catch (e) {
            return this._createThrownValue(e, objectGroupName);
        }
    }

    getFunctionDetails(objectId)
    {
        let parsedObjectId = this._parseObjectId(objectId);
        let object = this._objectForId(parsedObjectId);
        if (typeof object !== "function")
            return "Cannot resolve function by id.";
        return this.functionDetails(object);
    }

    functionDetails(func)
    {
        let details = InjectedScriptHost.functionDetails(func);
        if (!details)
            return "Cannot resolve function details.";
        return details;
    }

    getPreview(objectId)
    {
        let parsedObjectId = this._parseObjectId(objectId);
        let object = this._objectForId(parsedObjectId);
        return RemoteObject.createObjectPreviewForValue(object, true);
    }

    getProperties(objectId, ownProperties, generatePreview)
    {
        let nativeGettersAsValues = false;
        let collectionMode = ownProperties ? InjectedScript.CollectionMode.OwnProperties : InjectedScript.CollectionMode.AllProperties;
        return this._getProperties(objectId, collectionMode, generatePreview, nativeGettersAsValues);
    }

    getDisplayableProperties(objectId, generatePreview)
    {
        let nativeGettersAsValues = true;
        let collectionMode = InjectedScript.CollectionMode.OwnProperties | InjectedScript.CollectionMode.NativeGetterProperties;
        return this._getProperties(objectId, collectionMode, generatePreview, nativeGettersAsValues);
    }

    getInternalProperties(objectId, generatePreview)
    {
        let parsedObjectId = this._parseObjectId(objectId);
        let object = this._objectForId(parsedObjectId);
        let objectGroupName = this._idToObjectGroupName[parsedObjectId.id];

        if (!isDefined(object))
            return false;

        if (isSymbol(object))
            return false;

        let descriptors = this._internalPropertyDescriptors(object);
        if (!descriptors)
            return [];

        for (let i = 0; i < descriptors.length; ++i) {
            let descriptor = descriptors[i];
            if ("value" in descriptor)
                descriptor.value = RemoteObject.create(descriptor.value, objectGroupName, false, generatePreview);
        }

        return descriptors;
    }

    getCollectionEntries(objectId, objectGroupName, startIndex, numberToFetch)
    {
        let parsedObjectId = this._parseObjectId(objectId);
        let object = this._objectForId(parsedObjectId);
        objectGroupName = objectGroupName || this._idToObjectGroupName[parsedObjectId.id];

        if (!isDefined(object))
            return;

        if (typeof object !== "object")
            return;

        let entries = this._entries(object, InjectedScriptHost.subtype(object), startIndex, numberToFetch);
        return entries.map(function(entry) {
            entry.value = RemoteObject.create(entry.value, objectGroupName, false, true);
            if ("key" in entry)
                entry.key = RemoteObject.create(entry.key, objectGroupName, false, true);
            return entry;
        });
    }

    saveResult(callArgumentJSON)
    {
        this._savedResultIndex = 0;

        try {
            let callArgument = InjectedScriptHost.evaluate("(" + callArgumentJSON + ")");
            let value = this._resolveCallArgument(callArgument);
            this._saveResult(value);
        } catch { }

        return this._savedResultIndex;
    }

    wrapCallFrames(callFrame)
    {
        if (!callFrame)
            return false;

        let result = [];
        let depth = 0;
        do {
            result.push(new InjectedScript.CallFrameProxy(depth++, callFrame));
            callFrame = callFrame.caller;
        } while (callFrame);
        return result;
    }

    wrapObject(object, groupName, canAccessInspectedGlobalObject, generatePreview)
    {
        if (!canAccessInspectedGlobalObject)
            return this._fallbackWrapper(object);

        return RemoteObject.create(object, groupName, false, generatePreview);
    }

    wrapJSONString(jsonString, groupName, generatePreview)
    {
        try {
            return this.wrapObject(JSON.parse(jsonString), groupName, true, generatePreview);
        } catch {
            return null;
        }
    }

    wrapTable(canAccessInspectedGlobalObject, table, columns)
    {
        if (!canAccessInspectedGlobalObject)
            return this._fallbackWrapper(table);

        // FIXME: Currently columns are ignored. Instead, the frontend filters all
        // properties based on the provided column names and in the provided order.
        // We could filter here to avoid sending very large preview objects.

        let columnNames = null;
        if (typeof columns === "string")
            columns = [columns];

        if (InjectedScriptHost.subtype(columns) === "array") {
            columnNames = [];
            for (let i = 0; i < columns.length; ++i)
                columnNames.push(toString(columns[i]));
        }

        return RemoteObject.create(table, "console", false, true, columnNames);
    }

    previewValue(value)
    {
        return RemoteObject.createObjectPreviewForValue(value, true);
    }

    setEventValue(value)
    {
        this._eventValue = value;
    }

    clearEventValue()
    {
        delete this._eventValue;
    }

    setExceptionValue(value)
    {
        this._exceptionValue = value;
    }

    clearExceptionValue()
    {
        delete this._exceptionValue;
    }

    findObjectById(objectId)
    {
        let parsedObjectId = this._parseObjectId(objectId);
        return this._objectForId(parsedObjectId);
    }

    inspectObject(object)
    {
        if (this._commandLineAPIImpl)
            this._commandLineAPIImpl.inspect(object);
    }

    releaseObject(objectId)
    {
        let parsedObjectId = this._parseObjectId(objectId);
        this._releaseObject(parsedObjectId.id);
    }

    releaseObjectGroup(objectGroupName)
    {
        if (objectGroupName === "console") {
            delete this._lastResult;
            this._nextSavedResultIndex = 1;
            this._savedResults = [];
        }

        let group = this._objectGroups[objectGroupName];
        if (!group)
            return;

        for (let i = 0; i < group.length; i++)
            this._releaseObject(group[i]);

        delete this._objectGroups[objectGroupName];
    }

    // InjectedScriptModule C++ API

    module(name)
    {
        return this._modules[name];
    }

    injectModule(name, source, host)
    {
        delete this._modules[name];

        let moduleFunction = InjectedScriptHost.evaluate("(" + source + ")");
        if (typeof moduleFunction !== "function") {
            if (inspectedGlobalObject.console)
                inspectedGlobalObject.console.error("Web Inspector error: A function was expected for module %s evaluation", name);
            return null;
        }

        let module = moduleFunction.call(inspectedGlobalObject, InjectedScriptHost, inspectedGlobalObject, injectedScriptId, this, RemoteObject, host);
        this._modules[name] = module;
        return module;
    }

    // InjectedScriptModule JavaScript API

    isPrimitiveValue(value)
    {
        return isPrimitiveValue(value);
    }

    // Private

    _parseObjectId(objectId)
    {
        return InjectedScriptHost.evaluate("(" + objectId + ")");
    }

    _objectForId(objectId)
    {
        return this._idToWrappedObject[objectId.id];
    }

    _bind(object, objectGroupName)
    {
        let id = this._lastBoundObjectId++;
        let objectId = `{"injectedScriptId":${injectedScriptId},"id":${id}}`;

        this._idToWrappedObject[id] = object;

        if (objectGroupName) {
            let group = this._objectGroups[objectGroupName];
            if (!group) {
                group = [];
                this._objectGroups[objectGroupName] = group;
            }
            group.push(id);
            this._idToObjectGroupName[id] = objectGroupName;
        }

        return objectId;
    }

    _releaseObject(id)
    {
        delete this._idToWrappedObject[id];
        delete this._idToObjectGroupName[id];
    }

    _fallbackWrapper(object)
    {
        let result = {};
        result.type = typeof object;
        if (isPrimitiveValue(object))
            result.value = object;
        else
            result.description = toStringDescription(object);
        return result;
    }

    _resolveCallArgument(callArgumentJSON)
    {
        if ("value" in callArgumentJSON)
            return callArgumentJSON.value;

        let objectId = callArgumentJSON.objectId;
        if (objectId) {
            let parsedArgId = this._parseObjectId(objectId);
            if (!parsedArgId || parsedArgId["injectedScriptId"] !== injectedScriptId)
                throw "Arguments should belong to the same JavaScript world as the target object.";

            let resolvedArg = this._objectForId(parsedArgId);
            if (!isDefined(resolvedArg))
                throw "Could not find object with given id";

            return resolvedArg;
        }

        return undefined;
    }

    _createThrownValue(value, objectGroup)
    {
        let remoteObject = RemoteObject.create(value, objectGroup);
        try {
            remoteObject.description = toStringDescription(value);
        } catch { }
        return {
            wasThrown: true,
            result: remoteObject
        };
    }

    _evaluateAndWrap(evalFunction, object, expression, objectGroup, isEvalOnCallFrame, includeCommandLineAPI, returnByValue, generatePreview, saveResult)
    {
        return this._wrapAndSaveCall(objectGroup, returnByValue, generatePreview, saveResult, () => {
            return this._evaluateOn(evalFunction, object, expression, isEvalOnCallFrame, includeCommandLineAPI);
        });
    }

    _wrapAndSaveCall(objectGroup, returnByValue, generatePreview, saveResult, func)
    {
        return this._wrapCall(objectGroup, returnByValue, generatePreview, saveResult, () => {
            let result = func();
            if (saveResult)
                this._saveResult(result);
            return result;
        });
    }

    _wrapCall(objectGroup, returnByValue, generatePreview, saveResult, func)
    {
        try {
            this._savedResultIndex = 0;

            let returnObject = {
                wasThrown: false,
                result: RemoteObject.create(func(), objectGroup, returnByValue, generatePreview)
            };

            if (saveResult && this._savedResultIndex)
                returnObject.savedResultIndex = this._savedResultIndex;

            return returnObject;
        } catch (e) {
            return this._createThrownValue(e, objectGroup);
        }
    }

    _evaluateOn(evalFunction, object, expression, isEvalOnCallFrame, includeCommandLineAPI)
    {
        let commandLineAPI = null;
        if (includeCommandLineAPI) {
            if (this.CommandLineAPI)
                commandLineAPI = new this.CommandLineAPI(this._commandLineAPIImpl, isEvalOnCallFrame ? object : null);
            else
                commandLineAPI = new BasicCommandLineAPI(isEvalOnCallFrame ? object : null);
        }

        return evalFunction.call(object, expression, commandLineAPI);
    }

    _callFrameForId(topCallFrame, callFrameId)
    {
        let parsedCallFrameId = InjectedScriptHost.evaluate("(" + callFrameId + ")");
        let ordinal = parsedCallFrameId["ordinal"];
        let callFrame = topCallFrame;
        while (--ordinal >= 0 && callFrame)
            callFrame = callFrame.caller;
        return callFrame;
    }

    _getProperties(objectId, collectionMode, generatePreview, nativeGettersAsValues)
    {
        let parsedObjectId = this._parseObjectId(objectId);
        let object = this._objectForId(parsedObjectId);
        let objectGroupName = this._idToObjectGroupName[parsedObjectId.id];

        if (!isDefined(object))
            return false;

        if (isSymbol(object))
            return false;

        let descriptors = this._propertyDescriptors(object, collectionMode, nativeGettersAsValues);

        for (let i = 0; i < descriptors.length; ++i) {
            let descriptor = descriptors[i];
            if ("get" in descriptor)
                descriptor.get = RemoteObject.create(descriptor.get, objectGroupName);
            if ("set" in descriptor)
                descriptor.set = RemoteObject.create(descriptor.set, objectGroupName);
            if ("value" in descriptor)
                descriptor.value = RemoteObject.create(descriptor.value, objectGroupName, false, generatePreview);
            if (!("configurable" in descriptor))
                descriptor.configurable = false;
            if (!("enumerable" in descriptor))
                descriptor.enumerable = false;
            if ("symbol" in descriptor)
                descriptor.symbol = RemoteObject.create(descriptor.symbol, objectGroupName);
        }

        return descriptors;
    }

    _internalPropertyDescriptors(object, completeDescriptor)
    {
        let internalProperties = InjectedScriptHost.getInternalProperties(object);
        if (!internalProperties)
            return null;

        let descriptors = [];
        for (let i = 0; i < internalProperties.length; i++) {
            let property = internalProperties[i];
            let descriptor = {name: property.name, value: property.value};
            if (completeDescriptor) {
                descriptor.writable = false;
                descriptor.configurable = false;
                descriptor.enumerable = false;
                descriptor.isOwn = true;
            }
            descriptors.push(descriptor);
        }
        return descriptors;
    }

    _propertyDescriptors(object, collectionMode, nativeGettersAsValues)
    {
        if (InjectedScriptHost.subtype(object) === "proxy")
            return [];

        let descriptors = [];
        let nameProcessed = new Set;

        function createFakeValueDescriptor(name, symbol, descriptor, isOwnProperty, possibleNativeBindingGetter)
        {
            try {
                let fakeDescriptor = {name, value: object[name], writable: descriptor.writable || false, configurable: descriptor.configurable || false, enumerable: descriptor.enumerable || false};
                if (possibleNativeBindingGetter)
                    fakeDescriptor.nativeGetter = true;
                if (isOwnProperty)
                    fakeDescriptor.isOwn = true;
                if (symbol)
                    fakeDescriptor.symbol = symbol;
                // Silence any possible unhandledrejection exceptions created from accessing a native accessor with a wrong this object.
                if (fakeDescriptor.value instanceof Promise && InjectedScriptHost.isPromiseRejectedWithNativeGetterTypeError(fakeDescriptor.value))
                    fakeDescriptor.value.catch(function(){});
                return fakeDescriptor;
            } catch (e) {
                let errorDescriptor = {name, value: e, wasThrown: true};
                if (isOwnProperty)
                    errorDescriptor.isOwn = true;
                if (symbol)
                    errorDescriptor.symbol = symbol;
                return errorDescriptor;
            }
        }

        function processDescriptor(descriptor, isOwnProperty, possibleNativeBindingGetter)
        {
            // All properties.
            if (collectionMode & InjectedScript.CollectionMode.AllProperties) {
                descriptors.push(descriptor);
                return;
            }

            // Own properties.
            if (collectionMode & InjectedScript.CollectionMode.OwnProperties && isOwnProperty) {
                descriptors.push(descriptor);
                return;
            }

            // Native Getter properties.
            if (collectionMode & InjectedScript.CollectionMode.NativeGetterProperties) {
                if (possibleNativeBindingGetter) {
                    descriptors.push(descriptor);
                    return;
                }
            }
        }

        function processProperties(o, properties, isOwnProperty)
        {
            for (let i = 0; i < properties.length; ++i) {
                let property = properties[i];
                if (nameProcessed.has(property) || property === "__proto__")
                    continue;

                nameProcessed.add(property);

                let name = toString(property);
                let symbol = isSymbol(property) ? property : null;

                let descriptor = Object.getOwnPropertyDescriptor(o, property);
                if (!descriptor) {
                    // FIXME: Bad descriptor. Can we get here?
                    // Fall back to very restrictive settings.
                    let fakeDescriptor = createFakeValueDescriptor(name, symbol, {writable: false, configurable: false, enumerable: false}, isOwnProperty);
                    processDescriptor(fakeDescriptor, isOwnProperty);
                    continue;
                }

                if (nativeGettersAsValues) {
                    if (String(descriptor.get).endsWith("[native code]\n}") || (!descriptor.get && descriptor.hasOwnProperty("get") && !descriptor.set && descriptor.hasOwnProperty("set"))) {
                        // Developers may create such a descriptor, so we should be resilient:
                        // let x = {}; Object.defineProperty(x, "p", {get:undefined}); Object.getOwnPropertyDescriptor(x, "p")
                        let fakeDescriptor = createFakeValueDescriptor(name, symbol, descriptor, isOwnProperty, true);
                        processDescriptor(fakeDescriptor, isOwnProperty, true);
                        continue;
                    }
                }

                descriptor.name = name;
                if (isOwnProperty)
                    descriptor.isOwn = true;
                if (symbol)
                    descriptor.symbol = symbol;
                processDescriptor(descriptor, isOwnProperty);
            }
        }

        function arrayIndexPropertyNames(o, length)
        {
            let array = [];
            for (let i = 0; i < length; ++i) {
                if (i in o)
                    array.push("" + i);
            }
            return array;
        }

        // FIXME: <https://webkit.org/b/143589> Web Inspector: Better handling for large collections in Object Trees
        // For array types with a large length we attempt to skip getOwnPropertyNames and instead just sublist of indexes.
        let isArrayLike = false;
        try {
            isArrayLike = RemoteObject.subtype(object) === "array" && isFinite(object.length) && object.length > 0;
        } catch { }

        for (let o = object; isDefined(o); o = Object.getPrototypeOf(o)) {
            let isOwnProperty = o === object;

            if (isArrayLike && isOwnProperty)
                processProperties(o, arrayIndexPropertyNames(o, Math.min(object.length, 100)), isOwnProperty);
            else {
                processProperties(o, Object.getOwnPropertyNames(o), isOwnProperty);
                if (Object.getOwnPropertySymbols)
                    processProperties(o, Object.getOwnPropertySymbols(o), isOwnProperty);
            }

            if (collectionMode === InjectedScript.CollectionMode.OwnProperties)
                break;
        }

        // Always include __proto__ at the end.
        try {
            if (object.__proto__)
                descriptors.push({name: "__proto__", value: object.__proto__, writable: true, configurable: true, enumerable: false, isOwn: true});
        } catch { }

        return descriptors;
    }

    _getSetEntries(object, skip, numberToFetch)
    {
        let entries = [];

        // FIXME: This is observable if the page overrides Set.prototype[Symbol.iterator].
        for (let value of object) {
            if (skip > 0) {
                skip--;
                continue;
            }

            entries.push({value});

            if (numberToFetch && entries.length === numberToFetch)
                break;
        }

        return entries;
    }

    _getMapEntries(object, skip, numberToFetch)
    {
        let entries = [];

        // FIXME: This is observable if the page overrides Map.prototype[Symbol.iterator].
        for (let [key, value] of object) {
            if (skip > 0) {
                skip--;
                continue;
            }

            entries.push({key, value});

            if (numberToFetch && entries.length === numberToFetch)
                break;
        }

        return entries;
    }

    _getWeakMapEntries(object, numberToFetch)
    {
        return InjectedScriptHost.weakMapEntries(object, numberToFetch);
    }

    _getWeakSetEntries(object, numberToFetch)
    {
        return InjectedScriptHost.weakSetEntries(object, numberToFetch);
    }

    _getIteratorEntries(object, numberToFetch)
    {
        return InjectedScriptHost.iteratorEntries(object, numberToFetch);
    }

    _entries(object, subtype, startIndex, numberToFetch)
    {
        if (subtype === "set")
            return this._getSetEntries(object, startIndex, numberToFetch);
        if (subtype === "map")
            return this._getMapEntries(object, startIndex, numberToFetch);
        if (subtype === "weakmap")
            return this._getWeakMapEntries(object, numberToFetch);
        if (subtype === "weakset")
            return this._getWeakSetEntries(object, numberToFetch);
        if (subtype === "iterator")
            return this._getIteratorEntries(object, numberToFetch);

        throw "unexpected type";
    }

    _saveResult(result)
    {
        this._lastResult = result;

        if (result === undefined || result === null)
            return;

        let existingIndex = this._savedResults.indexOf(result);
        if (existingIndex !== -1) {
            this._savedResultIndex = existingIndex;
            return;
        }

        this._savedResultIndex = this._nextSavedResultIndex;
        this._savedResults[this._nextSavedResultIndex++] = result;

        // $n is limited from $1-$99. $0 is special.
        if (this._nextSavedResultIndex >= 100)
            this._nextSavedResultIndex = 1;
    }

    _savedResult(index)
    {
        return this._savedResults[index];
    }
};

InjectedScript.CollectionMode = {
    OwnProperties: 1 << 0,          // own properties.
    NativeGetterProperties: 1 << 1, // native getter properties in the prototype chain.
    AllProperties: 1 << 2,          // all properties in the prototype chain.
};

var injectedScript = new InjectedScript;

// -------

let RemoteObject = class RemoteObject
{
    constructor(object, objectGroupName, forceValueType, generatePreview, columnNames)
    {
        this.type = typeof object;

        if (this.type === "undefined" && InjectedScriptHost.isHTMLAllCollection(object))
            this.type = "object";

        if (isPrimitiveValue(object) || isBigInt(object) || object === null || forceValueType) {
            // We don't send undefined values over JSON.
            // BigInt values are not JSON serializable.
            if (this.type !== "undefined" && this.type !== "bigint")
                this.value = object;

            // Null object is object with 'null' subtype.
            if (object === null)
                this.subtype = "null";

            // Provide user-friendly number values.
            if (this.type === "number" || this.type === "bigint")
                this.description = toStringDescription(object);

            return;
        }

        this.objectId = injectedScript._bind(object, objectGroupName);

        let subtype = RemoteObject.subtype(object);
        if (subtype)
            this.subtype = subtype;

        this.className = InjectedScriptHost.internalConstructorName(object);
        this.description = RemoteObject.describe(object);

        if (subtype === "array")
            this.size = typeof object.length === "number" ? object.length : 0;
        else if (subtype === "set" || subtype === "map")
            this.size = object.size;
        else if (subtype === "weakmap")
            this.size = InjectedScriptHost.weakMapSize(object);
        else if (subtype === "weakset")
            this.size = InjectedScriptHost.weakSetSize(object);
        else if (subtype === "class") {
            this.classPrototype = RemoteObject.create(object.prototype, objectGroupName);
            this.className = object.name;
        }

        if (generatePreview && this.type === "object") {
            if (subtype === "proxy") {
                this.preview = this._generatePreview(InjectedScriptHost.proxyTargetValue(object));
                this.preview.lossless = false;
            } else
                this.preview = this._generatePreview(object, undefined, columnNames);
        }
    }

    // Static

    static create(object, objectGroupName, forceValueType, generatePreview, columnNames)
    {
        try {
            return new RemoteObject(object, objectGroupName, forceValueType, generatePreview, columnNames);
        } catch (e) {
            let description;
            try {
                description = RemoteObject.describe(e);
            } catch (ex) {
                alert(ex.message);
                description = "<failed to convert exception to string>";
            }
            return new RemoteObject(description);
        }
    }

    static createObjectPreviewForValue(value, generatePreview, columnNames)
    {
        let remoteObject = new RemoteObject(value, undefined, false, generatePreview, columnNames);
        if (remoteObject.objectId)
            injectedScript.releaseObject(remoteObject.objectId);
        if (remoteObject.classPrototype && remoteObject.classPrototype.objectId)
            injectedScript.releaseObject(remoteObject.classPrototype.objectId);
        return remoteObject.preview || remoteObject._emptyPreview();
    }

    static subtype(value)
    {
        if (value === null)
            return "null";

        if (isPrimitiveValue(value) || isBigInt(value) || isSymbol(value))
            return null;

        if (InjectedScriptHost.isHTMLAllCollection(value))
            return "array";

        let preciseType = InjectedScriptHost.subtype(value);
        if (preciseType)
            return preciseType;

        // FireBug's array detection.
        try {
            if (typeof value.splice === "function" && isFinite(value.length))
                return "array";
        } catch { }

        return null;
    }

    static describe(value)
    {
        if (isPrimitiveValue(value))
            return null;

        if (isBigInt(value))
            return null;

        if (isSymbol(value))
            return toString(value);

        let subtype = RemoteObject.subtype(value);

        if (subtype === "regexp")
            return toString(value);

        if (subtype === "date")
            return toString(value);

        if (subtype === "error")
            return toString(value);

        if (subtype === "proxy")
            return "Proxy";

        if (subtype === "node")
            return RemoteObject.nodePreview(value);

        let className = InjectedScriptHost.internalConstructorName(value);
        if (subtype === "array")
            return className;

        if (subtype === "iterator" && Symbol.toStringTag in value)
            return value[Symbol.toStringTag];

        // NodeList in JSC is a function, check for array prior to this.
        if (typeof value === "function")
            return value.toString();

        // If Object, try for a better name from the constructor.
        if (className === "Object") {
            let constructorName = value.constructor && value.constructor.name;
            if (constructorName)
                return constructorName;
        }

        return className;
    }

    static nodePreview(node)
    {
        let isXMLDocument = node.ownerDocument && !!node.ownerDocument.xmlVersion;
        let nodeName = isXMLDocument ? node.nodeName : node.nodeName.toLowerCase();

        switch (node.nodeType) {
        case 1: // Node.ELEMENT_NODE
            if (node.id)
                return "<" + nodeName + " id=\"" + node.id + "\">";
            if (node.classList.length)
                return "<" + nodeName + " class=\"" + node.classList.toString().replace(/\s+/, " ") + "\">";
            if (nodeName === "input" && node.type)
                return "<" + nodeName + " type=\"" + node.type + "\">";
            return "<" + nodeName + ">";

        case 3: // Node.TEXT_NODE
            return nodeName + " \"" + node.nodeValue + "\"";

        case 8: // Node.COMMENT_NODE
            return "<!--" + node.nodeValue + "-->";

        case 10: // Node.DOCUMENT_TYPE_NODE
            return "<!DOCTYPE " + nodeName + ">";

        default:
            return nodeName;
        }
    }

    // Private

    _initialPreview()
    {
        let preview = {
            type: this.type,
            description: this.description || toString(this.value),
            lossless: true,
        };

        if (this.subtype) {
            preview.subtype = this.subtype;
            if (this.subtype !== "null") {
                preview.overflow = false;
                preview.properties = [];
            }
        }

        if ("size" in this)
            preview.size = this.size;

        return preview;
    }

    _emptyPreview()
    {
        let preview = this._initialPreview();

        if (this.subtype === "map" || this.subtype === "set" || this.subtype === "weakmap" || this.subtype === "weakset" || this.subtype === "iterator") {
            if (this.size) {
                preview.entries = [];
                preview.lossless = false;
                preview.overflow = true;
            }
        }

        return preview;
    }

    _generatePreview(object, firstLevelKeys, secondLevelKeys)
    {
        let preview = this._initialPreview();
        let isTableRowsRequest = secondLevelKeys === null || secondLevelKeys;
        let firstLevelKeysCount = firstLevelKeys ? firstLevelKeys.length : 0;

        let propertiesThreshold = {
            properties: isTableRowsRequest ? 1000 : Math.max(5, firstLevelKeysCount),
            indexes: isTableRowsRequest ? 1000 : Math.max(10, firstLevelKeysCount)
        };

        try {
            // Maps, Sets, and Iterators have entries.
            if (this.subtype === "map" || this.subtype === "set" || this.subtype === "weakmap" || this.subtype === "weakset" || this.subtype === "iterator")
                this._appendEntryPreviews(object, preview);

            preview.properties = [];

            // Internal Properties.
            let internalPropertyDescriptors = injectedScript._internalPropertyDescriptors(object, true);
            if (internalPropertyDescriptors) {
                this._appendPropertyPreviews(object, preview, internalPropertyDescriptors, true, propertiesThreshold, firstLevelKeys, secondLevelKeys);
                if (propertiesThreshold.indexes < 0 || propertiesThreshold.properties < 0)
                    return preview;
            }

            if (preview.entries)
                return preview;

            // Properties.
            let nativeGettersAsValues = true;
            let descriptors = injectedScript._propertyDescriptors(object, InjectedScript.CollectionMode.AllProperties, nativeGettersAsValues);
            this._appendPropertyPreviews(object, preview, descriptors, false, propertiesThreshold, firstLevelKeys, secondLevelKeys);
            if (propertiesThreshold.indexes < 0 || propertiesThreshold.properties < 0)
                return preview;
        } catch {
            preview.lossless = false;
        }

        return preview;
    }

    _appendPropertyPreviews(object, preview, descriptors, internal, propertiesThreshold, firstLevelKeys, secondLevelKeys)
    {
        for (let i = 0; i < descriptors.length; ++i) {
            let descriptor = descriptors[i];

            // Seen enough.
            if (propertiesThreshold.indexes < 0 || propertiesThreshold.properties < 0)
                break;

            // Error in descriptor.
            if (descriptor.wasThrown) {
                preview.lossless = false;
                continue;
            }

            // Do not show "__proto__" in preview.
            let name = descriptor.name;
            if (name === "__proto__") {
                // Non basic __proto__ objects may have interesting, non-enumerable, methods to show.
                if (descriptor.value && descriptor.value.constructor
                    && descriptor.value.constructor !== Object
                    && descriptor.value.constructor !== Array
                    && descriptor.value.constructor !== RegExp)
                    preview.lossless = false;
                continue;
            }

            // For arrays, only allow indexes.
            if (this.subtype === "array" && !isUInt32(name))
                continue;

            // Do not show non-enumerable non-own properties.
            // Special case to allow array indexes that may be on the prototype.
            // Special case to allow native getters on non-RegExp objects.
            if (!descriptor.enumerable && !descriptor.isOwn && !(this.subtype === "array" || (this.subtype !== "regexp" && descriptor.nativeGetter)))
                continue;

            // If we have a filter, only show properties in the filter.
            // FIXME: Currently these filters do nothing on the backend.
            if (firstLevelKeys && !firstLevelKeys.includes(name))
                continue;

            // Getter/setter.
            if (!("value" in descriptor)) {
                preview.lossless = false;
                this._appendPropertyPreview(preview, internal, {name, type: "accessor"}, propertiesThreshold);
                continue;
            }

            // Null value.
            let value = descriptor.value;
            if (value === null) {
                this._appendPropertyPreview(preview, internal, {name, type: "object", subtype: "null", value: "null"}, propertiesThreshold);
                continue;
            }

            // Ignore non-enumerable functions.
            let type = typeof value;
            if (!descriptor.enumerable && type === "function")
                continue;

            // Fix type of document.all.
            if (InjectedScriptHost.isHTMLAllCollection(value))
                type = "object";

            // Primitive.
            const maxLength = 100;
            if (isPrimitiveValue(value) || isBigInt(value)) {
                if (type === "string" && value.length > maxLength) {
                    value = this._abbreviateString(value, maxLength, true);
                    preview.lossless = false;
                }
                this._appendPropertyPreview(preview, internal, {name, type, value: toStringDescription(value)}, propertiesThreshold);
                continue;
            }

            // Symbol.
            if (isSymbol(value)) {
                let symbolString = toString(value);
                if (symbolString.length > maxLength) {
                    symbolString = this._abbreviateString(symbolString, maxLength, true);
                    preview.lossless = false;
                }
                this._appendPropertyPreview(preview, internal, {name, type, value: symbolString}, propertiesThreshold);
                continue;
            }

            // Object.
            let property = {name, type};
            let subtype = RemoteObject.subtype(value);
            if (subtype)
                property.subtype = subtype;

            // Second level.
            if ((secondLevelKeys === null || secondLevelKeys) || this._isPreviewableObject(value, object)) {
                // FIXME: If we want secondLevelKeys filter to continue we would need some refactoring.
                let subPreview = RemoteObject.createObjectPreviewForValue(value, value !== object, secondLevelKeys);
                property.valuePreview = subPreview;
                if (!subPreview.lossless)
                    preview.lossless = false;
                if (subPreview.overflow)
                    preview.overflow = true;
            } else {
                let description = "";
                if (type !== "function" || subtype === "class") {
                    let fullDescription;
                    if (subtype === "class")
                        fullDescription = "class " + value.name;
                    else if (subtype === "node")
                        fullDescription = RemoteObject.nodePreview(value);
                    else
                        fullDescription = RemoteObject.describe(value);
                    description = this._abbreviateString(fullDescription, maxLength, subtype === "regexp");
                }
                property.value = description;
                preview.lossless = false;
            }

            this._appendPropertyPreview(preview, internal, property, propertiesThreshold);
        }
    }

    _appendPropertyPreview(preview, internal, property, propertiesThreshold)
    {
        if (toString(property.name >>> 0) === property.name)
            propertiesThreshold.indexes--;
        else
            propertiesThreshold.properties--;

        if (propertiesThreshold.indexes < 0 || propertiesThreshold.properties < 0) {
            preview.overflow = true;
            preview.lossless = false;
            return;
        }

        if (internal)
            property.internal = true;

        preview.properties.push(property);
    }

    _appendEntryPreviews(object, preview)
    {
        // Fetch 6, but only return 5, so we can tell if we overflowed.
        let entries = injectedScript._entries(object, this.subtype, 0, 6);
        if (!entries)
            return;

        if (entries.length > 5) {
            entries.pop();
            preview.overflow = true;
            preview.lossless = false;
        }

        function updateMainPreview(subPreview) {
            if (!subPreview.lossless)
                preview.lossless = false;
        }

        preview.entries = entries.map(function(entry) {
            entry.value = RemoteObject.createObjectPreviewForValue(entry.value, entry.value !== object);
            updateMainPreview(entry.value);
            if ("key" in entry) {
                entry.key = RemoteObject.createObjectPreviewForValue(entry.key, entry.key !== object);
                updateMainPreview(entry.key);
            }
            return entry;
        });
    }

    _isPreviewableObject(value, object)
    {
        let set = new Set;
        set.add(object);

        return this._isPreviewableObjectInternal(value, set, 1);
    }

    _isPreviewableObjectInternal(object, knownObjects, depth)
    {
        // Deep object.
        if (depth > 3)
            return false;

        // Primitive.
        if (isPrimitiveValue(object) || isBigInt(object) || isSymbol(object))
            return true;

        // Null.
        if (object === null)
            return true;

        // Cyclic objects.
        if (knownObjects.has(object))
            return false;

        ++depth;
        knownObjects.add(object);

        // Arrays are simple if they have 5 or less simple objects.
        let subtype = RemoteObject.subtype(object);
        if (subtype === "array") {
            let length = object.length;
            if (length > 5)
                return false;
            for (let i = 0; i < length; ++i) {
                if (!this._isPreviewableObjectInternal(object[i], knownObjects, depth))
                    return false;
            }
            return true;
        }

        // Not a basic object.
        if (object.__proto__ && object.__proto__.__proto__)
            return false;

        // Objects are simple if they have 3 or less simple value properties.
        let ownPropertyNames = Object.getOwnPropertyNames(object);
        if (ownPropertyNames.length > 3)
            return false;
        for (let i = 0; i < ownPropertyNames.length; ++i) {
            let propertyName = ownPropertyNames[i];
            let descriptor = Object.getOwnPropertyDescriptor(object, propertyName);
            if (descriptor && !("value" in descriptor))
                return false;
            if (!this._isPreviewableObjectInternal(object[propertyName], knownObjects, depth))
                return false;
        }

        return true;
    }

    _abbreviateString(string, maxLength, middle)
    {
        if (string.length <= maxLength)
            return string;

        if (middle) {
            let leftHalf = maxLength >> 1;
            let rightHalf = maxLength - leftHalf - 1;
            return string.substr(0, leftHalf) + "\u2026" + string.substr(string.length - rightHalf, rightHalf);
        }

        return string.substr(0, maxLength) + "\u2026";
    }
};

// -------

InjectedScript.CallFrameProxy = function(ordinal, callFrame)
{
    this.callFrameId = `{"ordinal":${ordinal},"injectedScriptId":${injectedScriptId}}`;
    this.functionName = callFrame.functionName;
    this.location = {scriptId: String(callFrame.sourceID), lineNumber: callFrame.line, columnNumber: callFrame.column};
    this.scopeChain = this._wrapScopeChain(callFrame);
    this.this = RemoteObject.create(callFrame.thisObject, "backtrace");
    this.isTailDeleted = callFrame.isTailDeleted;
};

InjectedScript.CallFrameProxy.prototype = {
    _wrapScopeChain(callFrame)
    {
        let scopeChain = callFrame.scopeChain;
        let scopeDescriptions = callFrame.scopeDescriptions();

        let scopeChainProxy = [];
        for (let i = 0; i < scopeChain.length; i++)
            scopeChainProxy[i] = InjectedScript.CallFrameProxy._createScopeJson(scopeChain[i], scopeDescriptions[i], "backtrace");
        return scopeChainProxy;
    }
};

InjectedScript.CallFrameProxy._scopeTypeNames = {
    0: "global", // GLOBAL_SCOPE
    1: "with", // WITH_SCOPE
    2: "closure", // CLOSURE_SCOPE
    3: "catch", // CATCH_SCOPE
    4: "functionName", // FUNCTION_NAME_SCOPE
    5: "globalLexicalEnvironment", // GLOBAL_LEXICAL_ENVIRONMENT_SCOPE
    6: "nestedLexical", // NESTED_LEXICAL_SCOPE
};

InjectedScript.CallFrameProxy._createScopeJson = function(object, {name, type, location}, groupId)
{
    let scope = {
        object: RemoteObject.create(object, groupId),
        type: InjectedScript.CallFrameProxy._scopeTypeNames[type],
    };

    if (name)
        scope.name = name;

    if (location)
        scope.location = location;

    if (isEmptyObject(object))
        scope.empty = true;

    return scope;
}

// -------

function bind(func, thisObject, ...outerArgs)
{
    return function(...innerArgs) {
        return func.apply(thisObject, outerArgs.concat(innerArgs));
    };
}

function BasicCommandLineAPI(callFrame)
{
    let savedResultAlias = InjectedScriptHost.savedResultAlias;

    let defineGetter = (key, value) => {
        if (typeof value !== "function") {
            let originalValue = value;
            value = function() { return originalValue; };
        }

        this.__defineGetter__("$" + key, value);
        if (savedResultAlias)
            this.__defineGetter__(savedResultAlias + key, value);
    };

    if ("_lastResult" in injectedScript)
        defineGetter("_", injectedScript._lastResult);

    if ("_exceptionValue" in injectedScript)
        defineGetter("exception", injectedScript._exceptionValue);

    if ("_eventValue" in injectedScript)
        defineGetter("event", injectedScript._eventValue);

    // $1-$99
    for (let i = 1; i <= injectedScript._savedResults.length; ++i)
        defineGetter(i, bind(injectedScript._savedResult, injectedScript, i));

    // Command Line API methods.
    for (let i = 0; i < BasicCommandLineAPI.methods.length; ++i) {
        let method = BasicCommandLineAPI.methods[i];
        this[method.name] = method;
    }
}

BasicCommandLineAPI.methods = [
    function dir() { return inspectedGlobalObject.console.dir(...arguments); },
    function clear() { return inspectedGlobalObject.console.clear(...arguments); },
    function table() { return inspectedGlobalObject.console.table(...arguments); },
    function profile() { return inspectedGlobalObject.console.profile(...arguments); },
    function profileEnd() { return inspectedGlobalObject.console.profileEnd(...arguments); },

    function keys(object) { return Object.keys(object); },
    function values(object) {
        let result = [];
        for (let key in object)
            result.push(object[key]);
        return result;
    },

    function queryInstances() {
        return InjectedScriptHost.queryInstances(...arguments);
    },

    function queryObjects() {
        return InjectedScriptHost.queryInstances(...arguments);
    },
];

for (let i = 0; i < BasicCommandLineAPI.methods.length; ++i) {
    let method = BasicCommandLineAPI.methods[i];
    method.toString = function() { return "function " + method.name + "() { [Command Line API] }"; };
}

return injectedScript;
})
