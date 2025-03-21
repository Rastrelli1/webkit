/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebPageInspectorTargetController.h"

#include "WebPage.h"
#include "WebPageProxyMessages.h"

namespace WebKit {

WebPageInspectorTargetController::WebPageInspectorTargetController(WebPage& page)
    : m_page(page)
    , m_pageTarget(page)
{
    // Do not send the page target to the UIProcess, the WebPageProxy will manager this for us.
    m_targets.set(m_pageTarget.identifier(), &m_pageTarget);
}

WebPageInspectorTargetController::~WebPageInspectorTargetController()
{
}

void WebPageInspectorTargetController::addTarget(Inspector::InspectorTarget& target)
{
    auto addResult = m_targets.set(target.identifier(), &target);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);

    m_page.send(Messages::WebPageProxy::CreateInspectorTarget(target.identifier(), target.type()));
}

void WebPageInspectorTargetController::removeTarget(Inspector::InspectorTarget& target)
{
    ASSERT_WITH_MESSAGE(target.identifier() != m_pageTarget.identifier(), "Should never remove the main target.");

    m_page.send(Messages::WebPageProxy::DestroyInspectorTarget(target.identifier()));

    m_targets.remove(target.identifier());
    m_targetFrontendChannels.remove(target.identifier());
}

void WebPageInspectorTargetController::connectInspector(const String& targetId, Inspector::FrontendChannel::ConnectionType connectionType)
{
    auto target = m_targets.get(targetId);
    if (!target)
        return;

    RefPtr<WebPageInspectorTargetFrontendChannel> channel = m_targetFrontendChannels.get(targetId);
    if (!channel) {
        channel = WebPageInspectorTargetFrontendChannel::create(*this, targetId, connectionType);
        m_targetFrontendChannels.set(target->identifier(), channel);
    }

    target->connect(*channel.get());
}

void WebPageInspectorTargetController::disconnectInspector(const String& targetId)
{
    auto target = m_targets.get(targetId);
    if (!target)
        return;

    RefPtr<WebPageInspectorTargetFrontendChannel> channel = m_targetFrontendChannels.take(targetId);
    if (!channel)
        return;

    target->disconnect(*channel.get());
}

void WebPageInspectorTargetController::sendMessageToTargetBackend(const String& targetId, const String& message)
{
    auto target = m_targets.get(targetId);
    if (!target)
        return;

    target->sendMessageToTargetBackend(message);
}

void WebPageInspectorTargetController::sendMessageToTargetFrontend(const String& targetId, const String& message)
{
    m_page.send(Messages::WebPageProxy::SendMessageToInspectorFrontend(targetId, message));
}

} // namespace WebKit
