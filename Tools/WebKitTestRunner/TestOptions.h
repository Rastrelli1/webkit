/*
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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

#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WTR {

struct TestOptions {
    struct ContextOptions {
        Vector<String> overrideLanguages;
        bool ignoreSynchronousMessagingTimeouts { false };
        bool enableProcessSwapOnNavigation { true };
        bool enableProcessSwapOnWindowOpen { false };

        bool hasSameInitializationOptions(const ContextOptions& options) const
        {
            if (ignoreSynchronousMessagingTimeouts != options.ignoreSynchronousMessagingTimeouts
                || overrideLanguages != options.overrideLanguages
                || enableProcessSwapOnNavigation != options.enableProcessSwapOnNavigation
                || enableProcessSwapOnWindowOpen != options.enableProcessSwapOnWindowOpen)
                return false;
            return true;
        }

        bool shouldEnableProcessSwapOnNavigation() const
        {
            return enableProcessSwapOnNavigation || enableProcessSwapOnWindowOpen;
        }
    };

    bool useThreadedScrolling { false };
    bool useAcceleratedDrawing { false };
    bool useRemoteLayerTree { false };
    bool shouldShowWebView { false };
    bool useFlexibleViewport { false };
    bool useFixedLayout { false };
    bool isSVGTest { false };
    bool useDataDetection { false };
    bool useMockScrollbars { true };
    bool needsSiteSpecificQuirks { false };
    bool ignoresViewportScaleLimits { false };
    bool useCharacterSelectionGranularity { false };
    bool enableAttachmentElement { false };
    bool enableIntersectionObserver { false };
    bool enableMenuItemElement { false };
    bool enableModernMediaControls { true };
    bool enablePointerLock { false };
    bool enableWebAuthentication { true };
    bool enableWebAuthenticationLocalAuthenticator { true };
    bool enableIsSecureContextAttribute { true };
    bool enableInspectorAdditions { false };
    bool shouldShowTouches { false };
    bool dumpJSConsoleLogInStdErr { false };
    bool allowCrossOriginSubresourcesToAskForCredentials { false };
    bool domPasteAllowed { true };
    bool enableColorFilter { false };
    bool punchOutWhiteBackgroundsInDarkMode { false };
    bool runSingly { false };
    bool checkForWorldLeaks { false };
    bool shouldIgnoreMetaViewport { false };
    bool shouldShowSpellCheckingDots { false };
    bool enableEditableImages { false };
    bool editable { false };
    bool enableUndoManagerAPI { false };
    bool shouldHandleRunOpenPanel { true };
    bool shouldPresentPopovers { true };
    bool enableAppNap { false };
    bool enablePageCache { false };
    bool enableLazyImageLoading { false };

    double contentInsetTop { 0 };

    float deviceScaleFactor { 1 };
    std::string applicationManifest;
    std::string jscOptions;
    std::string additionalSupportedImageTypes;
    HashMap<String, bool> experimentalFeatures;
    HashMap<String, bool> internalDebugFeatures;
    String contentMode;

    ContextOptions contextOptions;

    TestOptions(const std::string& pathOrURL);

    // Add here options that can only be set upon PlatformWebView
    // initialization and make sure it's up to date when adding new
    // options to this struct. Otherwise, tests using those options
    // might fail if WTR is reusing an existing PlatformWebView.
    bool hasSameInitializationOptions(const TestOptions& options) const
    {
        if (useThreadedScrolling != options.useThreadedScrolling
            || useAcceleratedDrawing != options.useAcceleratedDrawing
            || useMockScrollbars != options.useMockScrollbars
            || needsSiteSpecificQuirks != options.needsSiteSpecificQuirks
            || useCharacterSelectionGranularity != options.useCharacterSelectionGranularity
            || enableAttachmentElement != options.enableAttachmentElement
            || enableIntersectionObserver != options.enableIntersectionObserver
            || enableMenuItemElement != options.enableMenuItemElement
            || enableModernMediaControls != options.enableModernMediaControls
            || enablePointerLock != options.enablePointerLock
            || enableWebAuthentication != options.enableWebAuthentication
            || enableWebAuthenticationLocalAuthenticator != options.enableWebAuthenticationLocalAuthenticator
            || enableIsSecureContextAttribute != options.enableIsSecureContextAttribute
            || enableInspectorAdditions != options.enableInspectorAdditions
            || dumpJSConsoleLogInStdErr != options.dumpJSConsoleLogInStdErr
            || applicationManifest != options.applicationManifest
            || allowCrossOriginSubresourcesToAskForCredentials != options.allowCrossOriginSubresourcesToAskForCredentials
            || domPasteAllowed != options.domPasteAllowed
            || enableColorFilter != options.enableColorFilter
            || punchOutWhiteBackgroundsInDarkMode != options.punchOutWhiteBackgroundsInDarkMode
            || jscOptions != options.jscOptions
            || additionalSupportedImageTypes != options.additionalSupportedImageTypes
            || runSingly != options.runSingly
            || checkForWorldLeaks != options.checkForWorldLeaks
            || shouldShowSpellCheckingDots != options.shouldShowSpellCheckingDots
            || shouldIgnoreMetaViewport != options.shouldIgnoreMetaViewport
            || enableEditableImages != options.enableEditableImages
            || editable != options.editable
            || enableUndoManagerAPI != options.enableUndoManagerAPI
            || shouldHandleRunOpenPanel != options.shouldHandleRunOpenPanel
            || shouldPresentPopovers != options.shouldPresentPopovers
            || contentInsetTop != options.contentInsetTop
            || contentMode != options.contentMode
            || enableAppNap != options.enableAppNap
            || enablePageCache != options.enablePageCache)
            return false;

        if (!contextOptions.hasSameInitializationOptions(options.contextOptions))
            return false;

        if (experimentalFeatures != options.experimentalFeatures)
            return false;

        if (internalDebugFeatures != options.internalDebugFeatures)
            return false;

        return true;
    }
};

}
