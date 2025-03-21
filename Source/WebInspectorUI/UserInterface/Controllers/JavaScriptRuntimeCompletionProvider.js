/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

Object.defineProperty(WI, "javaScriptRuntimeCompletionProvider",
{
    get: function()
    {
        if (!WI.JavaScriptRuntimeCompletionProvider._instance)
            WI.JavaScriptRuntimeCompletionProvider._instance = new WI.JavaScriptRuntimeCompletionProvider;
        return WI.JavaScriptRuntimeCompletionProvider._instance;
    }
});

WI.JavaScriptRuntimeCompletionProvider = class JavaScriptRuntimeCompletionProvider extends WI.Object
{
    constructor()
    {
        super();

        console.assert(!WI.JavaScriptRuntimeCompletionProvider._instance);

        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.ActiveCallFrameDidChange, this._clearLastProperties, this);
    }

    // Protected

    completionControllerCompletionsNeeded(completionController, defaultCompletions, base, prefix, suffix, forced)
    {
        // Don't allow non-forced empty prefix completions unless the base is that start of property access.
        if (!forced && !prefix && !/[.[]$/.test(base)) {
            completionController.updateCompletions(null);
            return;
        }

        // If the base ends with an open parentheses or open curly bracket then treat it like there is
        // no base so we get global object completions.
        if (/[({]$/.test(base))
            base = "";

        var lastBaseIndex = base.length - 1;
        var dotNotation = base[lastBaseIndex] === ".";
        var bracketNotation = base[lastBaseIndex] === "[";

        if (dotNotation || bracketNotation) {
            base = base.substring(0, lastBaseIndex);

            // Don't suggest anything for an empty base that is using dot notation.
            // Bracket notation with an empty base will be treated as an array.
            if (!base && dotNotation) {
                completionController.updateCompletions(defaultCompletions);
                return;
            }

            // Don't allow non-forced empty prefix completions if the user is entering a number, since it might be a float.
            // But allow number completions if the base already has a decimal, so "10.0." will suggest Number properties.
            if (!forced && !prefix && dotNotation && base.indexOf(".") === -1 && parseInt(base, 10) == base) {
                completionController.updateCompletions(null);
                return;
            }

            // An empty base with bracket notation is not property access, it is an array.
            // Clear the bracketNotation flag so completions are not quoted.
            if (!base && bracketNotation)
                bracketNotation = false;
        }

        // If the base is the same as the last time, we can reuse the property names we have already gathered.
        // Doing this eliminates delay caused by the async nature of the code below and it only calls getters
        // and functions once instead of repetitively. Sure, there can be difference each time the base is evaluated,
        // but this optimization gives us more of a win. We clear the cache after 30 seconds or when stepping in the
        // debugger to make sure we don't use stale properties in most cases.
        if (this._lastBase === base && this._lastPropertyNames) {
            receivedPropertyNames.call(this, this._lastPropertyNames);
            return;
        }

        this._lastBase = base;
        this._lastPropertyNames = null;

        var activeCallFrame = WI.debuggerManager.activeCallFrame;
        if (!base && activeCallFrame && !this._alwaysEvaluateInWindowContext)
            activeCallFrame.collectScopeChainVariableNames(receivedPropertyNames.bind(this));
        else {
            let options = {objectGroup: "completion", includeCommandLineAPI: true, doNotPauseOnExceptionsAndMuteConsole: true, returnByValue: false, generatePreview: false, saveResult: false};
            WI.runtimeManager.evaluateInInspectedWindow(base, options, evaluated.bind(this));
        }

        function updateLastPropertyNames(propertyNames)
        {
            if (this._clearLastPropertiesTimeout)
                clearTimeout(this._clearLastPropertiesTimeout);
            this._clearLastPropertiesTimeout = setTimeout(this._clearLastProperties.bind(this), WI.JavaScriptLogViewController.CachedPropertiesDuration);

            this._lastPropertyNames = propertyNames || {};
        }

        function evaluated(result, wasThrown)
        {
            if (wasThrown || !result || result.type === "undefined" || (result.type === "object" && result.subtype === "null")) {
                WI.runtimeManager.activeExecutionContext.target.RuntimeAgent.releaseObjectGroup("completion");

                updateLastPropertyNames.call(this, {});
                completionController.updateCompletions(defaultCompletions);

                return;
            }

            function inspectedPage_evalResult_getArrayCompletions(primitiveType)
            {
                var array = this;
                var arrayLength;

                var resultSet = {};
                for (var o = array; o; o = o.__proto__) {
                    try {
                        if (o === array && o.length) {
                            // If the array type has a length, don't include a list of all the indexes.
                            // Include it at the end and the frontend can build the list.
                            arrayLength = o.length;
                        } else {
                            var names = Object.getOwnPropertyNames(o);
                            for (var i = 0; i < names.length; ++i)
                                resultSet[names[i]] = true;
                        }
                    } catch { }
                }

                if (arrayLength)
                    resultSet["length"] = arrayLength;

                return resultSet;
            }

            function inspectedPage_evalResult_getCompletions(primitiveType)
            {
                var object;
                if (primitiveType === "string")
                    object = new String("");
                else if (primitiveType === "number")
                    object = new Number(0);
                else if (primitiveType === "boolean")
                    object = new Boolean(false);
                else if (primitiveType === "symbol")
                    object = Symbol();
                else
                    object = this;

                var resultSet = {};
                for (var o = object; o; o = o.__proto__) {
                    try {
                        var names = Object.getOwnPropertyNames(o);
                        for (var i = 0; i < names.length; ++i)
                            resultSet[names[i]] = true;
                    } catch (e) { }
                }

                return resultSet;
            }

            if (result.subtype === "array")
                result.callFunctionJSON(inspectedPage_evalResult_getArrayCompletions, undefined, receivedArrayPropertyNames.bind(this));
            else if (result.type === "object" || result.type === "function")
                result.callFunctionJSON(inspectedPage_evalResult_getCompletions, undefined, receivedPropertyNames.bind(this));
            else if (result.type === "string" || result.type === "number" || result.type === "boolean" || result.type === "symbol") {
                let options = {objectGroup: "completion", includeCommandLineAPI: false, doNotPauseOnExceptionsAndMuteConsole: true, returnByValue: true, generatePreview: false, saveResult: false};
                WI.runtimeManager.evaluateInInspectedWindow("(" + inspectedPage_evalResult_getCompletions + ")(\"" + result.type + "\")", options, receivedPropertyNamesFromEvaluate.bind(this));
            } else
                console.error("Unknown result type: " + result.type);
        }

        function receivedPropertyNamesFromEvaluate(object, wasThrown, result)
        {
            receivedPropertyNames.call(this, result && !wasThrown ? result.value : null);
        }

        function receivedArrayPropertyNames(propertyNames)
        {
            // FIXME: <https://webkit.org/b/143589> Web Inspector: Better handling for large collections in Object Trees
            // If there was an array like object, we generate autocompletion up to 1000 indexes, but this should
            // handle a list with arbitrary length.
            if (propertyNames && typeof propertyNames.length === "number") {
                var max = Math.min(propertyNames.length, 1000);
                for (var i = 0; i < max; ++i)
                    propertyNames[i] = true;
            }

            receivedPropertyNames.call(this, propertyNames);
        }

        function receivedPropertyNames(propertyNames)
        {
            propertyNames = propertyNames || {};

            updateLastPropertyNames.call(this, propertyNames);

            WI.runtimeManager.activeExecutionContext.target.RuntimeAgent.releaseObjectGroup("completion");

            if (!base) {
                let commandLineAPI = WI.JavaScriptRuntimeCompletionProvider._commandLineAPI.slice(0);
                if (WI.debuggerManager.paused) {
                    let targetData = WI.debuggerManager.dataForTarget(WI.runtimeManager.activeExecutionContext.target);
                    if (targetData.pauseReason === WI.DebuggerManager.PauseReason.Listener || targetData.pauseReason === WI.DebuggerManager.PauseReason.EventListener)
                        commandLineAPI.push("$event");
                    else if (targetData.pauseReason === WI.DebuggerManager.PauseReason.Exception)
                        commandLineAPI.push("$exception");
                }
                for (let name of commandLineAPI)
                    propertyNames[name] = true;

                let savedResultAlias = WI.settings.consoleSavedResultAlias.value;
                if (savedResultAlias) {
                    propertyNames[savedResultAlias + "0"] = true;
                    propertyNames[savedResultAlias + "_"] = true;
                }

                // FIXME: Due to caching, sometimes old $n values show up as completion results even though they are not available. We should clear that proactively.
                for (var i = 1; i <= WI.ConsoleCommandResultMessage.maximumSavedResultIndex; ++i) {
                    propertyNames["$" + i] = true;

                    if (savedResultAlias)
                        propertyNames[savedResultAlias + i] = true;
                }
            }

            propertyNames = Object.keys(propertyNames);

            var implicitSuffix = "";
            if (bracketNotation) {
                var quoteUsed = prefix[0] === "'" ? "'" : "\"";
                if (suffix !== "]" && suffix !== quoteUsed)
                    implicitSuffix = "]";
            }

            var completions = defaultCompletions;
            var knownCompletions = completions.keySet();

            for (var i = 0; i < propertyNames.length; ++i) {
                var property = propertyNames[i];

                if (dotNotation && !/^[a-zA-Z_$][a-zA-Z0-9_$]*$/.test(property))
                    continue;

                if (bracketNotation) {
                    if (parseInt(property) != property)
                        property = quoteUsed + property.escapeCharacters(quoteUsed + "\\") + (suffix !== quoteUsed ? quoteUsed : "");
                }

                if (!property.startsWith(prefix) || property in knownCompletions)
                    continue;

                completions.push(property);
                knownCompletions[property] = true;
            }

            function compare(a, b)
            {
                // Try to sort in numerical order first.
                let numericCompareResult = a - b;
                if (!isNaN(numericCompareResult))
                    return numericCompareResult;

                // Sort __defineGetter__, __lookupGetter__, and friends last.
                let aRareProperty = a.startsWith("__") && a.endsWith("__");
                let bRareProperty = b.startsWith("__") && b.endsWith("__");
                if (aRareProperty && !bRareProperty)
                    return 1;
                if (!aRareProperty && bRareProperty)
                    return -1;

                // Not numbers, sort as strings.
                return a.extendedLocaleCompare(b);
            }

            completions.sort(compare);

            completionController.updateCompletions(completions, implicitSuffix);
        }
    }

    // Private

    _clearLastProperties()
    {
        if (this._clearLastPropertiesTimeout) {
            clearTimeout(this._clearLastPropertiesTimeout);
            delete this._clearLastPropertiesTimeout;
        }

        // Clear the cache of property names so any changes while stepping or sitting idle get picked up if the same
        // expression is evaluated again.
        this._lastPropertyNames = null;
    }
};

WI.JavaScriptRuntimeCompletionProvider._commandLineAPI = [
    "$",
    "$$",
    "$0",
    "$_",
    "$x",
    "clear",
    "copy",
    "dir",
    "dirxml",
    "getEventListeners",
    "inspect",
    "keys",
    "monitorEvents",
    "profile",
    "profileEnd",
    "queryInstances",
    "queryObjects",
    "screenshot",
    "table",
    "unmonitorEvents",
    "values",
];
