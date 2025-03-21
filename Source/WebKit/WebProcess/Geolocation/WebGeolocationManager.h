/*
 * Copyright (C) 2011, 2012, 2013 Apple Inc. All rights reserved.
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

#include "MessageReceiver.h"
#include "WebGeolocationPosition.h"
#include "WebProcessSupplement.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>

namespace WebCore {
class Geolocation;
class GeolocationPosition;
}

namespace WebKit {

class WebProcess;
class WebPage;

class WebGeolocationManager : public WebProcessSupplement, public IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(WebGeolocationManager);
public:
    explicit WebGeolocationManager(WebProcess&);

    static const char* supplementName();

    void registerWebPage(WebPage&);
    void unregisterWebPage(WebPage&);
    void setEnableHighAccuracyForPage(WebPage&, bool);

    void requestPermission(WebCore::Geolocation&);

private:
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    bool isUpdating() const { return !m_pageSet.isEmpty(); }
    bool isHighAccuracyEnabled() const { return !m_highAccuracyPageSet.isEmpty(); }

    void didChangePosition(const WebCore::GeolocationPosition&);
    void didFailToDeterminePosition(const String& errorMessage);
#if PLATFORM(IOS_FAMILY)
    void resetPermissions();
#endif // PLATFORM(IOS_FAMILY)

    WebProcess& m_process;
    HashSet<WebPage*> m_pageSet;
    HashSet<WebPage*> m_highAccuracyPageSet;
};

} // namespace WebKit
