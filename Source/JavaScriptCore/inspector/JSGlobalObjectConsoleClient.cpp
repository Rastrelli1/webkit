/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "JSGlobalObjectConsoleClient.h"

#include "ConsoleMessage.h"
#include "InspectorConsoleAgent.h"
#include "InspectorDebuggerAgent.h"
#include "InspectorScriptProfilerAgent.h"
#include "ScriptArguments.h"
#include "ScriptCallStack.h"
#include "ScriptCallStackFactory.h"

using namespace JSC;

namespace Inspector {

#if !LOG_DISABLED
static bool sLogToSystemConsole = true;
#else
static bool sLogToSystemConsole = false;
#endif

bool JSGlobalObjectConsoleClient::logToSystemConsole()
{
    return sLogToSystemConsole;
}

void JSGlobalObjectConsoleClient::setLogToSystemConsole(bool shouldLog)
{
    sLogToSystemConsole = shouldLog;
}

JSGlobalObjectConsoleClient::JSGlobalObjectConsoleClient(InspectorConsoleAgent* consoleAgent)
    : ConsoleClient()
    , m_consoleAgent(consoleAgent)
{
}

void JSGlobalObjectConsoleClient::messageWithTypeAndLevel(MessageType type, MessageLevel level, JSC::ExecState* exec, Ref<ScriptArguments>&& arguments)
{
    if (JSGlobalObjectConsoleClient::logToSystemConsole())
        ConsoleClient::printConsoleMessageWithArguments(MessageSource::ConsoleAPI, type, level, exec, arguments.copyRef());

    String message;
    arguments->getFirstArgumentAsString(message);
    m_consoleAgent->addMessageToConsole(makeUnique<ConsoleMessage>(MessageSource::ConsoleAPI, type, level, message, WTFMove(arguments), exec));

    if (type == MessageType::Assert) {
        if (m_debuggerAgent)
            m_debuggerAgent->handleConsoleAssert(message);
    }
}

void JSGlobalObjectConsoleClient::count(ExecState* exec, const String& label)
{
    m_consoleAgent->count(exec, label);
}

void JSGlobalObjectConsoleClient::countReset(ExecState* exec, const String& label)
{
    m_consoleAgent->countReset(exec, label);
}

void JSGlobalObjectConsoleClient::profile(JSC::ExecState*, const String& title)
{
    if (!m_consoleAgent->enabled())
        return;

    // Allow duplicate unnamed profiles. Disallow duplicate named profiles.
    if (!title.isEmpty()) {
        for (auto& existingTitle : m_profiles) {
            if (existingTitle == title) {
                // FIXME: Send an enum to the frontend for localization?
                String warning = title.isEmpty() ? "Unnamed Profile already exists"_s : makeString("Profile \"", title, "\" already exists");
                m_consoleAgent->addMessageToConsole(makeUnique<ConsoleMessage>(MessageSource::ConsoleAPI, MessageType::Profile, MessageLevel::Warning, warning));
                return;
            }
        }
    }

    m_profiles.append(title);
    startConsoleProfile();
}

void JSGlobalObjectConsoleClient::profileEnd(JSC::ExecState*, const String& title)
{
    if (!m_consoleAgent->enabled())
        return;

    // Stop profiles in reverse order. If the title is empty, then stop the last profile.
    // Otherwise, match the title of the profile to stop.
    for (ptrdiff_t i = m_profiles.size() - 1; i >= 0; --i) {
        if (title.isEmpty() || m_profiles[i] == title) {
            m_profiles.remove(i);
            if (m_profiles.isEmpty())
                stopConsoleProfile();
            return;
        }
    }

    // FIXME: Send an enum to the frontend for localization?
    String warning = title.isEmpty() ? "No profiles exist"_s : makeString("Profile \"", title, "\" does not exist");
    m_consoleAgent->addMessageToConsole(makeUnique<ConsoleMessage>(MessageSource::ConsoleAPI, MessageType::ProfileEnd, MessageLevel::Warning, warning));
}

void JSGlobalObjectConsoleClient::startConsoleProfile()
{
    ErrorString unused;

    if (m_debuggerAgent) {
        m_profileRestoreBreakpointActiveValue = m_debuggerAgent->breakpointsActive();
        m_debuggerAgent->setBreakpointsActive(unused, false);
    }

    if (m_scriptProfilerAgent) {
        const bool includeSamples = true;
        m_scriptProfilerAgent->startTracking(unused, &includeSamples);
    }
}

void JSGlobalObjectConsoleClient::stopConsoleProfile()
{
    ErrorString unused;

    if (m_scriptProfilerAgent)
        m_scriptProfilerAgent->stopTracking(unused);

    if (m_debuggerAgent)
        m_debuggerAgent->setBreakpointsActive(unused, m_profileRestoreBreakpointActiveValue);
}

void JSGlobalObjectConsoleClient::takeHeapSnapshot(JSC::ExecState*, const String& title)
{
    m_consoleAgent->takeHeapSnapshot(title);
}

void JSGlobalObjectConsoleClient::time(ExecState* exec, const String& label)
{
    m_consoleAgent->startTiming(exec, label);
}

void JSGlobalObjectConsoleClient::timeLog(ExecState* exec, const String& label, Ref<ScriptArguments>&& arguments)
{
    m_consoleAgent->logTiming(exec, label, WTFMove(arguments));
}

void JSGlobalObjectConsoleClient::timeEnd(ExecState* exec, const String& label)
{
    m_consoleAgent->stopTiming(exec, label);
}

void JSGlobalObjectConsoleClient::timeStamp(ExecState*, Ref<ScriptArguments>&&)
{
    // FIXME: JSContext inspection needs a timeline.
    warnUnimplemented("console.timeStamp"_s);
}

void JSGlobalObjectConsoleClient::record(ExecState*, Ref<ScriptArguments>&&) { }
void JSGlobalObjectConsoleClient::recordEnd(ExecState*, Ref<ScriptArguments>&&) { }

void JSGlobalObjectConsoleClient::screenshot(ExecState*, Ref<ScriptArguments>&&)
{
    warnUnimplemented("console.screenshot"_s);
}

void JSGlobalObjectConsoleClient::warnUnimplemented(const String& method)
{
    String message = method + " is currently ignored in JavaScript context inspection.";
    m_consoleAgent->addMessageToConsole(makeUnique<ConsoleMessage>(MessageSource::ConsoleAPI, MessageType::Log, MessageLevel::Warning, message));
}

} // namespace Inspector
