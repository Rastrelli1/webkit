/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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

WI.ContentViewCookieType = {
    ApplicationCache: "application-cache",
    CookieStorage: "cookie-storage",
    Database: "database",
    DatabaseTable: "database-table",
    DOMStorage: "dom-storage",
    Resource: "resource", // includes Frame too.
    Timelines: "timelines"
};

WI.SelectedSidebarPanelCookieKey = "selected-sidebar-panel";
WI.TypeIdentifierCookieKey = "represented-object-type";

WI.StateRestorationType = {
    Load: "state-restoration-load",
    Navigation: "state-restoration-navigation",
    Delayed: "state-restoration-delayed",
};

WI.LayoutDirection = {
    System: "system",
    LTR: "ltr",
    RTL: "rtl",
};

WI.loaded = function()
{
    // Register observers for events from the InspectorBackend.
    if (InspectorBackend.registerTargetDispatcher)
        InspectorBackend.registerTargetDispatcher(new WI.TargetObserver);
    if (InspectorBackend.registerInspectorDispatcher)
        InspectorBackend.registerInspectorDispatcher(new WI.InspectorObserver);
    if (InspectorBackend.registerPageDispatcher)
        InspectorBackend.registerPageDispatcher(new WI.PageObserver);
    if (InspectorBackend.registerConsoleDispatcher)
        InspectorBackend.registerConsoleDispatcher(new WI.ConsoleObserver);
    if (InspectorBackend.registerNetworkDispatcher)
        InspectorBackend.registerNetworkDispatcher(new WI.NetworkObserver);
    if (InspectorBackend.registerDOMDispatcher)
        InspectorBackend.registerDOMDispatcher(new WI.DOMObserver);
    if (InspectorBackend.registerDebuggerDispatcher)
        InspectorBackend.registerDebuggerDispatcher(new WI.DebuggerObserver);
    if (InspectorBackend.registerHeapDispatcher)
        InspectorBackend.registerHeapDispatcher(new WI.HeapObserver);
    if (InspectorBackend.registerMemoryDispatcher)
        InspectorBackend.registerMemoryDispatcher(new WI.MemoryObserver);
    if (InspectorBackend.registerDatabaseDispatcher)
        InspectorBackend.registerDatabaseDispatcher(new WI.DatabaseObserver);
    if (InspectorBackend.registerDOMStorageDispatcher)
        InspectorBackend.registerDOMStorageDispatcher(new WI.DOMStorageObserver);
    if (InspectorBackend.registerApplicationCacheDispatcher)
        InspectorBackend.registerApplicationCacheDispatcher(new WI.ApplicationCacheObserver);
    if (InspectorBackend.registerCPUProfilerDispatcher)
        InspectorBackend.registerCPUProfilerDispatcher(new WI.CPUProfilerObserver);
    if (InspectorBackend.registerScriptProfilerDispatcher)
        InspectorBackend.registerScriptProfilerDispatcher(new WI.ScriptProfilerObserver);
    if (InspectorBackend.registerTimelineDispatcher)
        InspectorBackend.registerTimelineDispatcher(new WI.TimelineObserver);
    if (InspectorBackend.registerCSSDispatcher)
        InspectorBackend.registerCSSDispatcher(new WI.CSSObserver);
    if (InspectorBackend.registerLayerTreeDispatcher)
        InspectorBackend.registerLayerTreeDispatcher(new WI.LayerTreeObserver);
    if (InspectorBackend.registerRuntimeDispatcher)
        InspectorBackend.registerRuntimeDispatcher(new WI.RuntimeObserver);
    if (InspectorBackend.registerWorkerDispatcher)
        InspectorBackend.registerWorkerDispatcher(new WI.WorkerObserver);
    if (InspectorBackend.registerCanvasDispatcher)
        InspectorBackend.registerCanvasDispatcher(new WI.CanvasObserver);

    // Listen for the ProvisionalLoadStarted event before registering for events so our code gets called before any managers or sidebars.
    // This lets us save a state cookie before any managers or sidebars do any resets that would affect state (namely TimelineManager).
    WI.Frame.addEventListener(WI.Frame.Event.ProvisionalLoadStarted, WI._provisionalLoadStarted,);

    // Populate any UIStrings that must be done early after localized strings have loaded.
    WI.KeyboardShortcut.Key.Space._displayName = WI.UIString("Space");

    // Create the singleton managers next, before the user interface elements, so the user interface can register
    // as event listeners on these managers.
    WI.managers = [
        WI.targetManager = new WI.TargetManager,
        WI.branchManager = new WI.BranchManager,
        WI.networkManager = new WI.NetworkManager,
        WI.domStorageManager = new WI.DOMStorageManager,
        WI.databaseManager = new WI.DatabaseManager,
        WI.indexedDBManager = new WI.IndexedDBManager,
        WI.domManager = new WI.DOMManager,
        WI.cssManager = new WI.CSSManager,
        WI.consoleManager = new WI.ConsoleManager,
        WI.runtimeManager = new WI.RuntimeManager,
        WI.heapManager = new WI.HeapManager,
        WI.memoryManager = new WI.MemoryManager,
        WI.applicationCacheManager = new WI.ApplicationCacheManager,
        WI.timelineManager = new WI.TimelineManager,
        WI.auditManager = new WI.AuditManager,
        WI.debuggerManager = new WI.DebuggerManager,
        WI.layerTreeManager = new WI.LayerTreeManager,
        WI.workerManager = new WI.WorkerManager,
        WI.domDebuggerManager = new WI.DOMDebuggerManager,
        WI.canvasManager = new WI.CanvasManager,
    ];

    // Register for events.
    WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.Paused, WI._debuggerDidPause);
    WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.Resumed, WI._debuggerDidResume);
    WI.domManager.addEventListener(WI.DOMManager.Event.InspectModeStateChanged, WI._inspectModeStateChanged);
    WI.domManager.addEventListener(WI.DOMManager.Event.DOMNodeWasInspected, WI._domNodeWasInspected);
    WI.domStorageManager.addEventListener(WI.DOMStorageManager.Event.DOMStorageObjectWasInspected, WI._domStorageWasInspected);
    WI.databaseManager.addEventListener(WI.DatabaseManager.Event.DatabaseWasInspected, WI._databaseWasInspected);
    WI.networkManager.addEventListener(WI.NetworkManager.Event.MainFrameDidChange, WI._mainFrameDidChange);
    WI.networkManager.addEventListener(WI.NetworkManager.Event.FrameWasAdded, WI._frameWasAdded);

    WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, WI._mainResourceDidChange);

    document.addEventListener("DOMContentLoaded", WI.contentLoaded);

    // Create settings.
    WI._showingSplitConsoleSetting = new WI.Setting("showing-split-console", false);
    WI._openTabsSetting = new WI.Setting("open-tab-types", [
        WI.ElementsTabContentView.Type,
        WI.NetworkTabContentView.Type,
        WI.DebuggerTabContentView.Type,
        WI.ResourcesTabContentView.Type,
        WI.TimelineTabContentView.Type,
        WI.StorageTabContentView.Type,
        WI.CanvasTabContentView.Type,
        WI.AuditTabContentView.Type,
        WI.ConsoleTabContentView.Type,
    ]);
    WI._selectedTabIndexSetting = new WI.Setting("selected-tab-index", 0);

    // State.
    WI.printStylesEnabled = false;
    WI.setZoomFactor(WI.settings.zoomFactor.value);
    WI.mouseCoords = {x: 0, y: 0};
    WI.modifierKeys = {altKey: false, metaKey: false, shiftKey: false};
    WI.visible = false;
    WI._windowKeydownListeners = [];
    WI._targetsAvailablePromise = new WI.WrappedPromise;
    WI._overridenDeviceUserAgent = null;
    WI._overridenDeviceSettings = new Map;

    // Targets.
    WI.backendTarget = null;
    WI.pageTarget = null;

    if (!window.TargetAgent)
        WI.targetManager.createDirectBackendTarget();
    else {
        // FIXME: Eliminate `TargetAgent.exists` once the local inspector
        // is configured to use the Multiplexing code path.
        TargetAgent.exists((error) => {
            if (error)
                WI.targetManager.createDirectBackendTarget();
        });
    }
};

WI.initializeBackendTarget = function(target)
{
    console.assert(!WI.mainTarget);

    WI.backendTarget = target;

    WI.resetMainExecutionContext();

    WI._targetsAvailablePromise.resolve();
};

WI.initializePageTarget = function(target)
{
    console.assert(WI.sharedApp.debuggableType === WI.DebuggableType.Web);
    console.assert(target.type === WI.Target.Type.Page || target instanceof WI.DirectBackendTarget);

    WI.pageTarget = target;

    WI.redirectGlobalAgentsToConnection(WI.pageTarget.connection);

    WI.resetMainExecutionContext();
};

WI.transitionPageTarget = function(target)
{
    console.assert(!WI.pageTarget);
    console.assert(WI.sharedApp.debuggableType === WI.DebuggableType.Web);
    console.assert(target.type === WI.Target.Type.Page);

    WI.pageTarget = target;

    WI.redirectGlobalAgentsToConnection(WI.pageTarget.connection);

    WI.resetMainExecutionContext();

    // Actions to transition the page target.
    WI.notifications.dispatchEventToListeners(WI.Notification.TransitionPageTarget);
    WI.domManager.transitionPageTarget();
    WI.networkManager.transitionPageTarget();
    WI.timelineManager.transitionPageTarget();
};

WI.terminatePageTarget = function(target)
{
    console.assert(WI.pageTarget);
    console.assert(WI.pageTarget === target);
    console.assert(WI.sharedApp.debuggableType === WI.DebuggableType.Web);

    // Remove any Worker targets associated with this page.
    let workerTargets = WI.targets.filter((x) => x.type === WI.Target.Type.Worker);
    for (let workerTarget of workerTargets)
        WI.workerManager.workerTerminated(workerTarget.identifier);

    WI.pageTarget = null;

    WI.redirectGlobalAgentsToConnection(WI.backendConnection);
};

WI.resetMainExecutionContext = function()
{
    if (WI.mainTarget instanceof WI.MultiplexingBackendTarget)
        return;

    if (WI.mainTarget.executionContext) {
        WI.runtimeManager.activeExecutionContext = WI.mainTarget.executionContext;
        if (WI.quickConsole)
            WI.quickConsole.initializeMainExecutionContextPathComponent();
    }
};

WI.redirectGlobalAgentsToConnection = function(connection)
{
    // This makes global window.FooAgent dispatch to the active page target.
    for (let [domain, agent] of Object.entries(InspectorBackend._agents)) {
        if (domain !== "Target")
            agent.connection = connection;
    }
};

WI.contentLoaded = function()
{
    // If there was an uncaught exception earlier during loading, then
    // abort loading more content. We could be in an inconsistent state.
    if (window.__uncaughtExceptions)
        return;

    // Register for global events.
    document.addEventListener("beforecopy", WI._beforecopy);
    document.addEventListener("copy", WI._copy);
    document.addEventListener("paste", WI._paste);

    document.addEventListener("click", WI._mouseWasClicked);
    document.addEventListener("dragover", WI._handleDragOver);
    document.addEventListener("drop", WI._handleDrop);
    document.addEventListener("focus", WI._focusChanged, true);

    window.addEventListener("focus", WI._windowFocused);
    window.addEventListener("blur", WI._windowBlurred);
    window.addEventListener("resize", WI._windowResized);
    window.addEventListener("keydown", WI._windowKeyDown);
    window.addEventListener("keyup", WI._windowKeyUp);
    window.addEventListener("mousedown", WI._mouseDown, true);
    window.addEventListener("mousemove", WI._mouseMoved, true);
    window.addEventListener("pagehide", WI._pageHidden);
    window.addEventListener("contextmenu", WI._contextMenuRequested);

    // Add platform style classes so the UI can be tweaked per-platform.
    document.body.classList.add(WI.Platform.name + "-platform");
    if (WI.Platform.isNightlyBuild)
        document.body.classList.add("nightly-build");

    if (WI.Platform.name === "mac")
        document.body.classList.add(WI.Platform.version.name);

    document.body.classList.add(WI.sharedApp.debuggableType);
    document.body.setAttribute("dir", WI.resolvedLayoutDirection());

    WI.settings.showJavaScriptTypeInformation.addEventListener(WI.Setting.Event.Changed, WI._showJavaScriptTypeInformationSettingChanged);
    WI.settings.enableControlFlowProfiler.addEventListener(WI.Setting.Event.Changed, WI._enableControlFlowProfilerSettingChanged);
    WI.settings.resourceCachingDisabled.addEventListener(WI.Setting.Event.Changed, WI._resourceCachingDisabledSettingChanged);

    function setTabSize() {
        document.body.style.tabSize = WI.settings.tabSize.value;
    }
    WI.settings.tabSize.addEventListener(WI.Setting.Event.Changed, setTabSize);
    setTabSize();

    function setInvisibleCharacterClassName() {
        document.body.classList.toggle("show-invisible-characters", WI.settings.showInvisibleCharacters.value);
    }
    WI.settings.showInvisibleCharacters.addEventListener(WI.Setting.Event.Changed, setInvisibleCharacterClassName);
    setInvisibleCharacterClassName();

    function setWhitespaceCharacterClassName() {
        document.body.classList.toggle("show-whitespace-characters", WI.settings.showWhitespaceCharacters.value);
    }
    WI.settings.showWhitespaceCharacters.addEventListener(WI.Setting.Event.Changed, setWhitespaceCharacterClassName);
    setWhitespaceCharacterClassName();

    WI.settingsTabContentView = new WI.SettingsTabContentView;

    WI._settingsKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, WI.KeyboardShortcut.Key.Comma, WI._showSettingsTab);

    // Create the user interface elements.
    WI.toolbar = new WI.Toolbar(document.getElementById("toolbar"));

    if (WI.settings.experimentalEnableNewTabBar.value)
        WI.tabBar = new WI.TabBar(document.getElementById("tab-bar"));
    else {
        WI.tabBar = new WI.LegacyTabBar(document.getElementById("tab-bar"));
        WI.tabBar.addEventListener(WI.TabBar.Event.OpenDefaultTab, WI._openDefaultTab);
    }

    WI._contentElement = document.getElementById("content");
    WI._contentElement.setAttribute("role", "main");
    WI._contentElement.setAttribute("aria-label", WI.UIString("Content"));

    WI.clearKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, "K", WI._clear);

    // FIXME: <https://webkit.org/b/151310> Web Inspector: Command-E should propagate to other search fields (including the system)
    WI.populateFindKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, "E", WI._populateFind);
    WI.populateFindKeyboardShortcut.implicitlyPreventsDefault = false;
    WI.findNextKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, "G", WI._findNext);
    WI.findPreviousKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.Shift | WI.KeyboardShortcut.Modifier.CommandOrControl, "G", WI._findPrevious);

    WI.consoleDrawer = new WI.ConsoleDrawer(document.getElementById("console-drawer"));
    WI.consoleDrawer.addEventListener(WI.ConsoleDrawer.Event.CollapsedStateChanged, WI._consoleDrawerCollapsedStateDidChange);
    WI.consoleDrawer.addEventListener(WI.ConsoleDrawer.Event.Resized, WI._consoleDrawerDidResize);

    WI.quickConsole = new WI.QuickConsole(document.getElementById("quick-console"));

    WI._consoleRepresentedObject = new WI.LogObject;
    WI.consoleContentView = WI.consoleDrawer.contentViewForRepresentedObject(WI._consoleRepresentedObject);
    WI.consoleLogViewController = WI.consoleContentView.logViewController;
    WI.breakpointPopoverController = new WI.BreakpointPopoverController;

    // FIXME: The sidebars should be flipped in RTL languages.
    WI.navigationSidebar = new WI.Sidebar(document.getElementById("navigation-sidebar"), WI.Sidebar.Sides.Left);
    WI.navigationSidebar.addEventListener(WI.Sidebar.Event.WidthDidChange, WI._sidebarWidthDidChange);

    WI.detailsSidebar = new WI.Sidebar(document.getElementById("details-sidebar"), WI.Sidebar.Sides.Right, null, null, WI.UIString("Details"), true);
    WI.detailsSidebar.addEventListener(WI.Sidebar.Event.WidthDidChange, WI._sidebarWidthDidChange);

    WI.searchKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl | WI.KeyboardShortcut.Modifier.Shift, "F", WI._focusSearchField);
    WI._findKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, "F", WI._find);
    WI.saveKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, "S", WI._save);
    WI._saveAsKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.Shift | WI.KeyboardShortcut.Modifier.CommandOrControl, "S", WI._saveAs);

    WI.openResourceKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl | WI.KeyboardShortcut.Modifier.Shift, "O", WI._showOpenResourceDialog);
    new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, "P", WI._showOpenResourceDialog);

    WI.navigationSidebarKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl | WI.KeyboardShortcut.Modifier.Shift, "0", WI.toggleNavigationSidebar);
    WI.detailsSidebarKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl | WI.KeyboardShortcut.Modifier.Option, "0", WI.toggleDetailsSidebar);

    let boundIncreaseZoom = WI._increaseZoom;
    let boundDecreaseZoom = WI._decreaseZoom;
    WI._increaseZoomKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, WI.KeyboardShortcut.Key.Plus, boundIncreaseZoom);
    WI._decreaseZoomKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, WI.KeyboardShortcut.Key.Minus, boundDecreaseZoom);
    WI._increaseZoomKeyboardShortcut2 = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl | WI.KeyboardShortcut.Modifier.Shift, WI.KeyboardShortcut.Key.Plus, boundIncreaseZoom);
    WI._decreaseZoomKeyboardShortcut2 = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl | WI.KeyboardShortcut.Modifier.Shift, WI.KeyboardShortcut.Key.Minus, boundDecreaseZoom);
    WI._resetZoomKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, "0", WI._resetZoom);

    WI._showTabAtIndexKeyboardShortcuts = [1, 2, 3, 4, 5, 6, 7, 8, 9].map((i) => new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl | WI.KeyboardShortcut.Modifier.Option, `${i}`, (event) => { WI._showTabAtIndex(i); }));
    WI._openNewTabKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl | WI.KeyboardShortcut.Modifier.Option, "T", WI.showNewTabTab);

    WI.tabBrowser = new WI.TabBrowser(document.getElementById("tab-browser"), WI.tabBar, WI.navigationSidebar, WI.detailsSidebar);
    WI.tabBrowser.addEventListener(WI.TabBrowser.Event.SelectedTabContentViewDidChange, WI._tabBrowserSelectedTabContentViewDidChange);

    WI._reloadPageKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, "R", WI._reloadPage);
    WI._reloadPageFromOriginKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl | WI.KeyboardShortcut.Modifier.Option, "R", WI._reloadPageFromOrigin);
    WI._reloadPageKeyboardShortcut.implicitlyPreventsDefault = WI._reloadPageFromOriginKeyboardShortcut.implicitlyPreventsDefault = false;

    WI._consoleTabKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.Option | WI.KeyboardShortcut.Modifier.CommandOrControl, "C", WI._showConsoleTab);
    WI._quickConsoleKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.Control, WI.KeyboardShortcut.Key.Apostrophe, WI._focusConsolePrompt);

    WI._inspectModeKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl | WI.KeyboardShortcut.Modifier.Shift, "C", WI._toggleInspectMode);

    WI._undoKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, "Z", WI._undoKeyboardShortcut);
    WI._redoKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl | WI.KeyboardShortcut.Modifier.Shift, "Z", WI._redoKeyboardShortcut);
    WI._undoKeyboardShortcut.implicitlyPreventsDefault = WI._redoKeyboardShortcut.implicitlyPreventsDefault = false;

    WI.toggleBreakpointsKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, "Y", WI.debuggerToggleBreakpoints);
    WI.pauseOrResumeKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.Control | WI.KeyboardShortcut.Modifier.CommandOrControl, "Y", WI.debuggerPauseResumeToggle);
    WI.stepOverKeyboardShortcut = new WI.KeyboardShortcut(null, WI.KeyboardShortcut.Key.F6, WI.debuggerStepOver);
    WI.stepIntoKeyboardShortcut = new WI.KeyboardShortcut(null, WI.KeyboardShortcut.Key.F7, WI.debuggerStepInto);
    WI.stepOutKeyboardShortcut = new WI.KeyboardShortcut(null, WI.KeyboardShortcut.Key.F8, WI.debuggerStepOut);

    WI.pauseOrResumeAlternateKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, WI.KeyboardShortcut.Key.Backslash, WI.debuggerPauseResumeToggle);
    WI.stepOverAlternateKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, WI.KeyboardShortcut.Key.SingleQuote, WI.debuggerStepOver);
    WI.stepIntoAlternateKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, WI.KeyboardShortcut.Key.Semicolon, WI.debuggerStepInto);
    WI.stepOutAlternateKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.Shift | WI.KeyboardShortcut.Modifier.CommandOrControl, WI.KeyboardShortcut.Key.Semicolon, WI.debuggerStepOut);

    WI._closeToolbarButton = new WI.ControlToolbarItem("dock-close", WI.UIString("Close"), "Images/Close.svg", 16, 14);
    WI._closeToolbarButton.addEventListener(WI.ButtonNavigationItem.Event.Clicked, WI.close);

    WI._undockToolbarButton = new WI.ButtonToolbarItem("undock", WI.UIString("Detach into separate window"), "Images/Undock.svg");
    WI._undockToolbarButton.element.classList.add(WI.Popover.IgnoreAutoDismissClassName);
    WI._undockToolbarButton.addEventListener(WI.ButtonNavigationItem.Event.Clicked, WI._undock);

    let dockImage = WI.resolvedLayoutDirection() === WI.LayoutDirection.RTL ? "Images/DockLeft.svg" : "Images/DockRight.svg";
    WI._dockToSideToolbarButton = new WI.ButtonToolbarItem("dock-right", WI.UIString("Dock to side of window"), dockImage);
    WI._dockToSideToolbarButton.element.classList.add(WI.Popover.IgnoreAutoDismissClassName);

    let dockToSideCallback = WI.resolvedLayoutDirection() === WI.LayoutDirection.RTL ? WI._dockLeft : WI._dockRight;
    WI._dockToSideToolbarButton.addEventListener(WI.ButtonNavigationItem.Event.Clicked, dockToSideCallback);

    WI._dockBottomToolbarButton = new WI.ButtonToolbarItem("dock-bottom", WI.UIString("Dock to bottom of window"), "Images/DockBottom.svg");
    WI._dockBottomToolbarButton.element.classList.add(WI.Popover.IgnoreAutoDismissClassName);
    WI._dockBottomToolbarButton.addEventListener(WI.ButtonNavigationItem.Event.Clicked, WI._dockBottom);

    WI._togglePreviousDockConfigurationKeyboardShortcut = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl | WI.KeyboardShortcut.Modifier.Shift, "D", WI._togglePreviousDockConfiguration);

    let reloadToolTip;
    if (WI.sharedApp.debuggableType === WI.DebuggableType.JavaScript)
        reloadToolTip = WI.UIString("Restart (%s)").format(WI._reloadPageKeyboardShortcut.displayName);
    else
        reloadToolTip = WI.UIString("Reload page (%s)\nReload page ignoring cache (%s)").format(WI._reloadPageKeyboardShortcut.displayName, WI._reloadPageFromOriginKeyboardShortcut.displayName);
    WI._reloadToolbarButton = new WI.ButtonToolbarItem("reload", reloadToolTip, "Images/ReloadToolbar.svg");
    WI._reloadToolbarButton.addEventListener(WI.ButtonNavigationItem.Event.Clicked, WI._reloadToolbarButtonClicked);

    WI._downloadToolbarButton = new WI.ButtonToolbarItem("download", WI.UIString("Download Web Archive"), "Images/DownloadArrow.svg");
    WI._downloadToolbarButton.addEventListener(WI.ButtonNavigationItem.Event.Clicked, WI._downloadWebArchive);

    let elementSelectionToolTip = WI.UIString("Start element selection (%s)").format(WI._inspectModeKeyboardShortcut.displayName);
    let activatedElementSelectionToolTip = WI.UIString("Stop element selection (%s)").format(WI._inspectModeKeyboardShortcut.displayName);
    WI._inspectModeToolbarButton = new WI.ActivateButtonToolbarItem("inspect", elementSelectionToolTip, activatedElementSelectionToolTip, "Images/Crosshair.svg");
    WI._inspectModeToolbarButton.addEventListener(WI.ButtonNavigationItem.Event.Clicked, WI._toggleInspectMode);

    // COMPATIBILITY (iOS 12.2): Page.overrideSetting did not exist.
    if (InspectorFrontendHost.isRemote && WI.sharedApp.debuggableType === WI.DebuggableType.Web && InspectorBackend.domains.Page && InspectorBackend.domains.Page.overrideUserAgent && InspectorBackend.domains.Page.overrideSetting) {
        const deviceSettingsTooltip = WI.UIString("Device Settings");
        WI._deviceSettingsToolbarButton = new WI.ActivateButtonToolbarItem("device-settings", deviceSettingsTooltip, deviceSettingsTooltip, "Images/Device.svg");
        WI._deviceSettingsToolbarButton.addEventListener(WI.ButtonNavigationItem.Event.Clicked, WI._handleDeviceSettingsToolbarButtonClicked);

        WI._deviceSettingsPopover = null;
    }

    WI._updateReloadToolbarButton();
    WI._updateDownloadToolbarButton();
    WI._updateInspectModeToolbarButton();

    WI._dashboards = {
        default: new WI.DefaultDashboard,
        debugger: new WI.DebuggerDashboard,
    };

    WI._dashboardContainer = new WI.DashboardContainerView;
    WI._dashboardContainer.showDashboardViewForRepresentedObject(WI._dashboards.default);

    WI.toolbar.addToolbarItem(WI._closeToolbarButton, WI.Toolbar.Section.Control);

    WI.toolbar.addToolbarItem(WI._undockToolbarButton, WI.Toolbar.Section.Left);
    WI.toolbar.addToolbarItem(WI._dockToSideToolbarButton, WI.Toolbar.Section.Left);
    WI.toolbar.addToolbarItem(WI._dockBottomToolbarButton, WI.Toolbar.Section.Left);

    WI.toolbar.addToolbarItem(WI._reloadToolbarButton, WI.Toolbar.Section.CenterLeft);
    WI.toolbar.addToolbarItem(WI._downloadToolbarButton, WI.Toolbar.Section.CenterLeft);

    WI.toolbar.addToolbarItem(WI._dashboardContainer.toolbarItem, WI.Toolbar.Section.Center);

    WI.toolbar.addToolbarItem(WI._inspectModeToolbarButton, WI.Toolbar.Section.CenterRight);

    if (WI._deviceSettingsToolbarButton)
        WI.toolbar.addToolbarItem(WI._deviceSettingsToolbarButton, WI.Toolbar.Section.CenterRight);

    WI._searchTabContentView = new WI.SearchTabContentView;

    if (WI.settings.experimentalEnableNewTabBar.value) {
        WI.tabBrowser.addTabForContentView(WI._searchTabContentView, {suppressAnimations: true});
        WI.tabBar.addTabBarItem(WI.settingsTabContentView.tabBarItem, {suppressAnimations: true});
    } else {
        const incremental = false;
        WI._searchToolbarItem = new WI.SearchBar("inspector-search", WI.UIString("Search"), incremental);
        WI._searchToolbarItem.addEventListener(WI.SearchBar.Event.TextChanged, WI._searchTextDidChange);
        WI.toolbar.addToolbarItem(WI._searchToolbarItem, WI.Toolbar.Section.Right);
    }

    let dockedResizerElement = document.getElementById("docked-resizer");
    dockedResizerElement.classList.add(WI.Popover.IgnoreAutoDismissClassName);
    dockedResizerElement.addEventListener("mousedown", WI._dockedResizerMouseDown);

    WI._dockingAvailable = false;

    WI._updateDockNavigationItems();
    WI._setupViewHierarchy();

    // These tabs are always available for selecting, modulo isTabAllowed().
    // Other tabs may be engineering-only or toggled at runtime if incomplete.
    let productionTabClasses = [
        WI.ElementsTabContentView,
        WI.NetworkTabContentView,
        WI.SourcesTabContentView,
        WI.DebuggerTabContentView,
        WI.ResourcesTabContentView,
        WI.TimelineTabContentView,
        WI.StorageTabContentView,
        WI.CanvasTabContentView,
        WI.LayersTabContentView,
        WI.AuditTabContentView,
        WI.ConsoleTabContentView,
        WI.SearchTabContentView,
        WI.NewTabContentView,
        WI.SettingsTabContentView,
    ];

    WI._knownTabClassesByType = new Map;
    // Set tab classes directly. The public API triggers other updates and
    // notifications that won't work or have no listeners at WI point.
    for (let tabClass of productionTabClasses)
        WI._knownTabClassesByType.set(tabClass.Type, tabClass);

    WI._pendingOpenTabs = [];

    // Previously we may have stored duplicates in WI setting. Avoid creating duplicate tabs.
    let openTabTypes = WI._openTabsSetting.value;
    let seenTabTypes = new Set;

    for (let i = 0; i < openTabTypes.length; ++i) {
        let tabType = openTabTypes[i];

        if (seenTabTypes.has(tabType))
            continue;
        seenTabTypes.add(tabType);

        if (!WI.isTabTypeAllowed(tabType)) {
            WI._pendingOpenTabs.push({tabType, index: i});
            continue;
        }

        if (!WI.isNewTabWithTypeAllowed(tabType))
            continue;

        let tabContentView = WI._createTabContentViewForType(tabType);
        if (!tabContentView)
            continue;
        WI.tabBrowser.addTabForContentView(tabContentView, {suppressAnimations: true});
    }

    WI._restoreCookieForOpenTabs(WI.StateRestorationType.Load);

    WI.tabBar.selectedTabBarItem = WI._selectedTabIndexSetting.value;

    if (!WI.tabBar.selectedTabBarItem)
        WI.tabBar.selectedTabBarItem = 0;

    if (!WI.tabBar.normalTabCount)
        WI.showNewTabTab({suppressAnimations: true});

    // Listen to the events after restoring the saved tabs to avoid recursion.
    WI.tabBar.addEventListener(WI.TabBar.Event.TabBarItemAdded, WI._rememberOpenTabs);
    WI.tabBar.addEventListener(WI.TabBar.Event.TabBarItemRemoved, WI._rememberOpenTabs);
    WI.tabBar.addEventListener(WI.TabBar.Event.TabBarItemsReordered, WI._rememberOpenTabs);

    function updateConsoleSavedResultPrefixCSSVariable() {
        document.body.style.setProperty("--console-saved-result-prefix", "\"" + WI.RuntimeManager.preferredSavedResultPrefix() + "\"");
    }
    WI.settings.consoleSavedResultAlias.addEventListener(WI.Setting.Event.Changed, updateConsoleSavedResultPrefixCSSVariable);
    updateConsoleSavedResultPrefixCSSVariable();

    // Signal that the frontend is now ready to receive messages.
    WI.whenTargetsAvailable().then(() => {
        InspectorFrontendAPI.loadCompleted();
    });

    WI._updateSheetRect();

    // Tell the InspectorFrontendHost we loaded, which causes the window to display
    // and pending InspectorFrontendAPI commands to be sent.
    InspectorFrontendHost.loaded();

    if (WI._showingSplitConsoleSetting.value)
        WI.showSplitConsole();

    // Store WI on the window in case the WebInspector global gets corrupted.
    window.__frontendCompletedLoad = true;

    if (WI.runBootstrapOperations)
        WI.runBootstrapOperations();
};

WI.performOneTimeFrontendInitializationsUsingTarget = function(target)
{
    if (!WI.__didPerformConsoleInitialization && target.ConsoleAgent) {
        WI.__didPerformConsoleInitialization = true;
        WI.consoleManager.initializeLogChannels(target);
    }

    if (!WI.__didPerformCSSInitialization && target.CSSAgent) {
        WI.__didPerformCSSInitialization = true;
        WI.CSSCompletions.initializeCSSCompletions(target);
    }
};

WI.initializeTarget = function(target)
{
    if (target.PageAgent) {
        // COMPATIBILITY (iOS 12.2): Page.overrideUserAgent did not exist.
        if (target.PageAgent.overrideUserAgent && WI._overridenDeviceUserAgent)
            target.PageAgent.overrideUserAgent(WI._overridenDeviceUserAgent);

        // COMPATIBILITY (iOS 12.2): Page.overrideSetting did not exist.
        if (target.PageAgent.overrideSetting) {
            for (let [setting, value] of WI._overridenDeviceSettings)
                target.PageAgent.overrideSetting(setting, value);
        }

        // COMPATIBILITY (iOS 11.3)
        if (target.PageAgent.setShowRulers && WI.settings.showRulers.value)
            target.PageAgent.setShowRulers(true);

        // COMPATIBILITY (iOS 8): Page.setShowPaintRects did not exist.
        if (target.PageAgent.setShowPaintRects && WI.settings.showPaintRects.value)
            target.PageAgent.setShowPaintRects(true);
    }
};

WI.targetsAvailable = function()
{
    return WI._targetsAvailablePromise.settled;
};

WI.whenTargetsAvailable = function()
{
    return WI._targetsAvailablePromise.promise;
};

WI.isTabTypeAllowed = function(tabType)
{
    let tabClass = WI._knownTabClassesByType.get(tabType);
    if (!tabClass)
        return false;

    return tabClass.isTabAllowed();
};

WI.knownTabClasses = function()
{
    return new Set(WI._knownTabClassesByType.values());
};

WI._showOpenResourceDialog = function()
{
    if (!WI._openResourceDialog)
        WI._openResourceDialog = new WI.OpenResourceDialog(WI);

    if (WI._openResourceDialog.visible)
        return;

    WI._openResourceDialog.present(WI._contentElement);
};

WI._createTabContentViewForType = function(tabType)
{
    let tabClass = WI._knownTabClassesByType.get(tabType);
    if (!tabClass) {
        console.error("Unknown tab type", tabType);
        return null;
    }

    console.assert(WI.TabContentView.isPrototypeOf(tabClass));
    return new tabClass;
};

WI._rememberOpenTabs = function()
{
    let seenTabTypes = new Set;
    let openTabs = [];

    for (let tabBarItem of WI.tabBar.tabBarItems) {
        let tabContentView = tabBarItem.representedObject;
        if (!(tabContentView instanceof WI.TabContentView))
            continue;
        if (!tabContentView.constructor.shouldSaveTab())
            continue;
        console.assert(tabContentView.type, "Tab type can't be null, undefined, or empty string", tabContentView.type, tabContentView);
        openTabs.push(tabContentView.type);
        seenTabTypes.add(tabContentView.type);
    }

    // Keep currently unsupported tabs in the setting at their previous index.
    for (let {tabType, index} of WI._pendingOpenTabs) {
        if (seenTabTypes.has(tabType))
            continue;
        openTabs.insertAtIndex(tabType, index);
        seenTabTypes.add(tabType);
    }

    WI._openTabsSetting.value = openTabs;
};

WI._openDefaultTab = function(event)
{
    WI.showNewTabTab({suppressAnimations: true});
};

WI._showSettingsTab = function(event)
{
    if (event.keyIdentifier === "U+002C") // ","
        WI.tabBrowser.showTabForContentView(WI.settingsTabContentView);
};

WI._tryToRestorePendingTabs = function()
{
    let stillPendingOpenTabs = [];
    for (let {tabType, index} of WI._pendingOpenTabs) {
        if (!WI.isTabTypeAllowed(tabType)) {
            stillPendingOpenTabs.push({tabType, index});
            continue;
        }

        let tabContentView = WI._createTabContentViewForType(tabType);
        if (!tabContentView)
            continue;

        WI.tabBrowser.addTabForContentView(tabContentView, {
            suppressAnimations: true,
            insertionIndex: index,
        });

        tabContentView.restoreStateFromCookie(WI.StateRestorationType.Load);
    }

    WI._pendingOpenTabs = stillPendingOpenTabs;

    if (!WI.settings.experimentalEnableNewTabBar.value)
        WI.tabBar.updateNewTabTabBarItemState();
};

WI.showNewTabTab = function(options)
{
    if (!WI.isNewTabWithTypeAllowed(WI.NewTabContentView.Type))
        return;

    let tabContentView = WI.tabBrowser.bestTabContentViewForClass(WI.NewTabContentView);
    if (!tabContentView)
        tabContentView = new WI.NewTabContentView;
    WI.tabBrowser.showTabForContentView(tabContentView, options);
};

WI.isNewTabWithTypeAllowed = function(tabType)
{
    let tabClass = WI._knownTabClassesByType.get(tabType);
    if (!tabClass || !tabClass.isTabAllowed())
        return false;

    // Only allow one tab per class for now.
    for (let tabBarItem of WI.tabBar.tabBarItems) {
        let tabContentView = tabBarItem.representedObject;
        if (!(tabContentView instanceof WI.TabContentView))
            continue;
        if (tabContentView.constructor === tabClass)
            return false;
    }

    if (tabClass === WI.NewTabContentView) {
        let allTabs = Array.from(WI.knownTabClasses());
        let addableTabs = allTabs.filter((tabClass) => !tabClass.tabInfo().isEphemeral);
        let canMakeNewTab = addableTabs.some((tabClass) => WI.isNewTabWithTypeAllowed(tabClass.Type));
        return canMakeNewTab;
    }

    return true;
};

WI.createNewTabWithType = function(tabType, options = {})
{
    console.assert(WI.isNewTabWithTypeAllowed(tabType));

    let {referencedView, shouldReplaceTab, shouldShowNewTab} = options;
    console.assert(!referencedView || referencedView instanceof WI.TabContentView, referencedView);
    console.assert(!shouldReplaceTab || referencedView, "Must provide a reference view to replace a tab.");

    let tabContentView = WI._createTabContentViewForType(tabType);
    const suppressAnimations = true;
    WI.tabBrowser.addTabForContentView(tabContentView, {
        suppressAnimations,
        insertionIndex: referencedView ? WI.tabBar.tabBarItems.indexOf(referencedView.tabBarItem) : undefined,
    });

    if (shouldReplaceTab)
        WI.tabBrowser.closeTabForContentView(referencedView, {suppressAnimations});

    if (shouldShowNewTab)
        WI.tabBrowser.showTabForContentView(tabContentView);
};

WI.activateExtraDomains = function(domains)
{
    WI.notifications.dispatchEventToListeners(WI.Notification.ExtraDomainsActivated, {domains});

    if (WI.mainTarget) {
        if (!WI.pageTarget && WI.mainTarget.DOMAgent)
            WI.pageTarget = WI.mainTarget;

        if (WI.mainTarget.CSSAgent)
            WI.CSSCompletions.initializeCSSCompletions(WI.assumingMainTarget());

        if (WI.mainTarget.DOMAgent)
            WI.domManager.ensureDocument();

        if (WI.mainTarget.PageAgent)
            WI.networkManager.initializeTarget(WI.mainTarget);
    }

    WI._updateReloadToolbarButton();
    WI._updateDownloadToolbarButton();
    WI._updateInspectModeToolbarButton();

    WI._tryToRestorePendingTabs();
};

WI.updateWindowTitle = function()
{
    var mainFrame = WI.networkManager.mainFrame;
    if (!mainFrame)
        return;

    var urlComponents = mainFrame.mainResource.urlComponents;

    var lastPathComponent;
    try {
        lastPathComponent = decodeURIComponent(urlComponents.lastPathComponent || "");
    } catch {
        lastPathComponent = urlComponents.lastPathComponent;
    }

    // Build a title based on the URL components.
    if (urlComponents.host && lastPathComponent)
        var title = WI.displayNameForHost(urlComponents.host) + " \u2014 " + lastPathComponent;
    else if (urlComponents.host)
        var title = WI.displayNameForHost(urlComponents.host);
    else if (lastPathComponent)
        var title = lastPathComponent;
    else
        var title = mainFrame.url;

    // The name "inspectedURLChanged" sounds like the whole URL is required, however WI is only
    // used for updating the window title and it can be any string.
    InspectorFrontendHost.inspectedURLChanged(title);
};

WI.updateDockingAvailability = function(available)
{
    WI._dockingAvailable = available;

    WI._updateDockNavigationItems();
};

WI.updateDockedState = function(side)
{
    if (WI._dockConfiguration === side)
        return;

    WI._previousDockConfiguration = WI._dockConfiguration;

    if (!WI._previousDockConfiguration) {
        if (side === WI.DockConfiguration.Right || side === WI.DockConfiguration.Left)
            WI._previousDockConfiguration = WI.DockConfiguration.Bottom;
        else
            WI._previousDockConfiguration = WI.resolvedLayoutDirection() === WI.LayoutDirection.RTL ? WI.DockConfiguration.Left : WI.DockConfiguration.Right;
    }

    WI._dockConfiguration = side;

    WI.docked = side !== WI.DockConfiguration.Undocked;

    WI._ignoreToolbarModeDidChangeEvents = true;

    if (side === WI.DockConfiguration.Bottom) {
        document.body.classList.add("docked", WI.DockConfiguration.Bottom);
        document.body.classList.remove("window-inactive", WI.DockConfiguration.Right, WI.DockConfiguration.Left);
    } else if (side === WI.DockConfiguration.Right) {
        document.body.classList.add("docked", WI.DockConfiguration.Right);
        document.body.classList.remove("window-inactive", WI.DockConfiguration.Bottom, WI.DockConfiguration.Left);
    } else if (side === WI.DockConfiguration.Left) {
        document.body.classList.add("docked", WI.DockConfiguration.Left);
        document.body.classList.remove("window-inactive", WI.DockConfiguration.Bottom, WI.DockConfiguration.Right);
    } else
        document.body.classList.remove("docked", WI.DockConfiguration.Right, WI.DockConfiguration.Left, WI.DockConfiguration.Bottom);

    WI._ignoreToolbarModeDidChangeEvents = false;

    WI._updateDockNavigationItems();

    if (!WI.dockedConfigurationSupportsSplitContentBrowser() && !WI.doesCurrentTabSupportSplitContentBrowser())
        WI.hideSplitConsole();
};

WI.updateVisibilityState = function(visible)
{
    WI.visible = visible;
    WI.notifications.dispatchEventToListeners(WI.Notification.VisibilityStateDidChange);
};

WI.handlePossibleLinkClick = function(event, frame, options = {})
{
    let anchorElement = event.target.closest("a");
    if (!anchorElement || !anchorElement.href)
        return false;

    if (WI.isBeingEdited(anchorElement)) {
        // Don't follow the link when it is being edited.
        return false;
    }

    // Prevent the link from navigating, since we don't do any navigation by following links normally.
    event.preventDefault();
    event.stopPropagation();

    WI.openURL(anchorElement.href, frame, {
        ...options,
        lineNumber: anchorElement.lineNumber,
        ignoreSearchTab: !WI.isShowingSearchTab(),
    });

    return true;
};

WI.openURL = function(url, frame, options = {})
{
    console.assert(url);
    if (!url)
        return;

    console.assert(typeof options.lineNumber === "undefined" || typeof options.lineNumber === "number", "lineNumber should be a number.");

    // If alwaysOpenExternally is not defined, base it off the command/meta key for the current event.
    if (options.alwaysOpenExternally === undefined || options.alwaysOpenExternally === null)
        options.alwaysOpenExternally = window.event ? window.event.metaKey : false;

    if (options.alwaysOpenExternally) {
        InspectorFrontendHost.openInNewTab(url);
        return;
    }

    let searchChildFrames = false;
    if (!frame) {
        frame = WI.networkManager.mainFrame;
        searchChildFrames = true;
    }

    let resource;
    let simplifiedURL = removeURLFragment(url);
    if (frame) {
        // WI.Frame.resourceForURL does not check the main resource, only sub-resources. So check both.
        resource = frame.url === simplifiedURL ? frame.mainResource : frame.resourceForURL(simplifiedURL, searchChildFrames);
    } else if (WI.sharedApp.debuggableType === WI.DebuggableType.ServiceWorker)
        resource = WI.mainTarget.resourceCollection.resourceForURL(removeURLFragment(url));

    if (resource) {
        let positionToReveal = new WI.SourceCodePosition(options.lineNumber, 0);
        WI.showSourceCode(resource, {...options, positionToReveal});
        return;
    }

    InspectorFrontendHost.openInNewTab(url);
};

WI.close = function()
{
    if (WI._isClosing)
        return;

    WI._isClosing = true;

    InspectorFrontendHost.closeWindow();
};

WI.isConsoleFocused = function()
{
    return WI.quickConsole.prompt.focused;
};

WI.isShowingSplitConsole = function()
{
    return !WI.consoleDrawer.collapsed;
};

WI.dockedConfigurationSupportsSplitContentBrowser = function()
{
    return WI._dockConfiguration !== WI.DockConfiguration.Bottom;
};

WI.doesCurrentTabSupportSplitContentBrowser = function()
{
    var currentContentView = WI.tabBrowser.selectedTabContentView;
    return !currentContentView || currentContentView.supportsSplitContentBrowser;
};

WI.toggleSplitConsole = function()
{
    if (!WI.doesCurrentTabSupportSplitContentBrowser()) {
        WI.showConsoleTab();
        return;
    }

    if (WI.isShowingSplitConsole())
        WI.hideSplitConsole();
    else
        WI.showSplitConsole();
};

WI.showSplitConsole = function()
{
    if (!WI.doesCurrentTabSupportSplitContentBrowser()) {
        WI.showConsoleTab();
        return;
    }

    WI.consoleDrawer.collapsed = false;

    if (WI.consoleDrawer.currentContentView === WI.consoleContentView)
        return;

    WI.consoleDrawer.showContentView(WI.consoleContentView);
};

WI.hideSplitConsole = function()
{
    if (!WI.isShowingSplitConsole())
        return;

    WI.consoleDrawer.collapsed = true;
};

WI.showConsoleTab = function(requestedScope)
{
    requestedScope = requestedScope || WI.LogContentView.Scopes.All;

    WI.hideSplitConsole();

    WI.consoleContentView.scopeBar.item(requestedScope).selected = true;

    WI.showRepresentedObject(WI._consoleRepresentedObject);

    console.assert(WI.isShowingConsoleTab());
};

WI.isShowingConsoleTab = function()
{
    return WI.tabBrowser.selectedTabContentView instanceof WI.ConsoleTabContentView;
};

WI.showElementsTab = function()
{
    var tabContentView = WI.tabBrowser.bestTabContentViewForClass(WI.ElementsTabContentView);
    if (!tabContentView)
        tabContentView = new WI.ElementsTabContentView;
    WI.tabBrowser.showTabForContentView(tabContentView);
};

WI.isShowingElementsTab = function()
{
    return WI.tabBrowser.selectedTabContentView instanceof WI.ElementsTabContentView;
};

WI.showSourcesTab = function(options = {})
{
    let tabContentView = WI.tabBrowser.bestTabContentViewForClass(WI.SourcesTabContentView);
    if (!tabContentView)
        tabContentView = new WI.SourcesTabContentView;

    if (options.breakpointToSelect instanceof WI.Breakpoint)
        tabContentView.revealAndSelectBreakpoint(options.breakpointToSelect);

    if (options.showScopeChainSidebar)
        tabContentView.showScopeChainDetailsSidebarPanel();

    WI.tabBrowser.showTabForContentView(tabContentView);
};

WI.isShowingSourcesTab = function()
{
    return WI.tabBrowser.selectedTabContentView instanceof WI.SourcesTabContentView;
};

WI.showDebuggerTab = function(options)
{
    var tabContentView = WI.tabBrowser.bestTabContentViewForClass(WI.DebuggerTabContentView);
    if (!tabContentView)
        tabContentView = new WI.DebuggerTabContentView;

    if (options.breakpointToSelect instanceof WI.Breakpoint)
        tabContentView.revealAndSelectBreakpoint(options.breakpointToSelect);

    if (options.showScopeChainSidebar)
        tabContentView.showScopeChainDetailsSidebarPanel();

    WI.tabBrowser.showTabForContentView(tabContentView);
};

WI.isShowingDebuggerTab = function()
{
    return WI.tabBrowser.selectedTabContentView instanceof WI.DebuggerTabContentView;
};

WI.showResourcesTab = function()
{
    var tabContentView = WI.tabBrowser.bestTabContentViewForClass(WI.ResourcesTabContentView);
    if (!tabContentView)
        tabContentView = new WI.ResourcesTabContentView;
    WI.tabBrowser.showTabForContentView(tabContentView);
};

WI.isShowingResourcesTab = function()
{
    return WI.tabBrowser.selectedTabContentView instanceof WI.ResourcesTabContentView;
};

WI.showStorageTab = function()
{
    var tabContentView = WI.tabBrowser.bestTabContentViewForClass(WI.StorageTabContentView);
    if (!tabContentView)
        tabContentView = new WI.StorageTabContentView;
    WI.tabBrowser.showTabForContentView(tabContentView);
};

WI.showNetworkTab = function()
{
    let tabContentView = WI.tabBrowser.bestTabContentViewForClass(WI.NetworkTabContentView);
    if (!tabContentView)
        tabContentView = new WI.NetworkTabContentView;

    WI.tabBrowser.showTabForContentView(tabContentView);
};

WI.isShowingNetworkTab = function()
{
    return WI.tabBrowser.selectedTabContentView instanceof WI.NetworkTabContentView;
};

WI.isShowingSearchTab = function()
{
    return WI.tabBrowser.selectedTabContentView instanceof WI.SearchTabContentView;
};

WI.showTimelineTab = function()
{
    var tabContentView = WI.tabBrowser.bestTabContentViewForClass(WI.TimelineTabContentView);
    if (!tabContentView)
        tabContentView = new WI.TimelineTabContentView;
    WI.tabBrowser.showTabForContentView(tabContentView);
};

WI.showLayersTab = function(options = {})
{
    let tabContentView = WI.tabBrowser.bestTabContentViewForClass(WI.LayersTabContentView);
    if (!tabContentView)
        tabContentView = new WI.LayersTabContentView;
    if (options.nodeToSelect)
        tabContentView.selectLayerForNode(options.nodeToSelect);
    WI.tabBrowser.showTabForContentView(tabContentView);
};

WI.isShowingLayersTab = function()
{
    return WI.tabBrowser.selectedTabContentView instanceof WI.LayersTabContentView;
};

WI.indentString = function()
{
    if (WI.settings.indentWithTabs.value)
        return "\t";
    return " ".repeat(WI.settings.indentUnit.value);
};

WI.restoreFocusFromElement = function(element)
{
    if (element && element.contains(WI.currentFocusElement))
        WI.previousFocusElement.focus();
};

WI.toggleNavigationSidebar = function(event)
{
    if (!WI.navigationSidebar.collapsed || !WI.navigationSidebar.sidebarPanels.length) {
        WI.navigationSidebar.collapsed = true;
        return;
    }

    if (!WI.navigationSidebar.selectedSidebarPanel)
        WI.navigationSidebar.selectedSidebarPanel = WI.navigationSidebar.sidebarPanels[0];
    WI.navigationSidebar.collapsed = false;
};

WI.toggleDetailsSidebar = function(event)
{
    if (!WI.detailsSidebar.collapsed || !WI.detailsSidebar.sidebarPanels.length) {
        WI.detailsSidebar.collapsed = true;
        return;
    }

    if (!WI.detailsSidebar.selectedSidebarPanel)
        WI.detailsSidebar.selectedSidebarPanel = WI.detailsSidebar.sidebarPanels[0];
    WI.detailsSidebar.collapsed = false;
};

WI.getMaximumSidebarWidth = function(sidebar)
{
    console.assert(sidebar instanceof WI.Sidebar);

    const minimumContentBrowserWidth = 100;

    let minimumWidth = window.innerWidth - minimumContentBrowserWidth;
    let tabContentView = WI.tabBrowser.selectedTabContentView;
    console.assert(tabContentView);
    if (!tabContentView)
        return minimumWidth;

    let otherSidebar = null;
    if (sidebar === WI.navigationSidebar)
        otherSidebar = tabContentView.detailsSidebarPanels.length ? WI.detailsSidebar : null;
    else
        otherSidebar = tabContentView.navigationSidebarPanel ? WI.navigationSidebar : null;

    if (otherSidebar)
        minimumWidth -= otherSidebar.width;

    return minimumWidth;
};

WI.tabContentViewClassForRepresentedObject = function(representedObject)
{
    if (representedObject instanceof WI.DOMTree)
        return WI.ElementsTabContentView;

    if (representedObject instanceof WI.TimelineRecording)
        return WI.TimelineTabContentView;

    // We only support one console tab right now. So WI isn't an instanceof check.
    if (representedObject === WI._consoleRepresentedObject)
        return WI.ConsoleTabContentView;

    if (WI.settings.experimentalEnableSourcesTab.value) {
        if (representedObject instanceof WI.Frame
            || representedObject instanceof WI.FrameCollection
            || representedObject instanceof WI.Resource
            || representedObject instanceof WI.ResourceCollection
            || representedObject instanceof WI.Script
            || representedObject instanceof WI.ScriptCollection
            || representedObject instanceof WI.CSSStyleSheet
            || representedObject instanceof WI.CSSStyleSheetCollection)
            return WI.SourcesTabContentView;
    } else {
        if (WI.debuggerManager.paused) {
            if (representedObject instanceof WI.Script)
                return WI.DebuggerTabContentView;

            if (representedObject instanceof WI.Resource && (representedObject.type === WI.Resource.Type.Document || representedObject.type === WI.Resource.Type.Script))
                return WI.DebuggerTabContentView;
        }

        if (representedObject instanceof WI.Frame
            || representedObject instanceof WI.FrameCollection
            || representedObject instanceof WI.Resource
            || representedObject instanceof WI.ResourceCollection
            || representedObject instanceof WI.Script
            || representedObject instanceof WI.ScriptCollection
            || representedObject instanceof WI.CSSStyleSheet
            || representedObject instanceof WI.CSSStyleSheetCollection)
            return WI.ResourcesTabContentView;
    }

    if (representedObject instanceof WI.DOMStorageObject || representedObject instanceof WI.CookieStorageObject ||
        representedObject instanceof WI.DatabaseTableObject || representedObject instanceof WI.DatabaseObject ||
        representedObject instanceof WI.ApplicationCacheFrame || representedObject instanceof WI.IndexedDatabaseObjectStore ||
        representedObject instanceof WI.IndexedDatabase || representedObject instanceof WI.IndexedDatabaseObjectStoreIndex)
        return WI.StorageTabContentView;

    if (representedObject instanceof WI.AuditTestCase || representedObject instanceof WI.AuditTestGroup
        || representedObject instanceof WI.AuditTestCaseResult || representedObject instanceof WI.AuditTestGroupResult)
        return WI.AuditTabContentView;

    if (representedObject instanceof WI.CanvasCollection)
        return WI.CanvasTabContentView;

    if (representedObject instanceof WI.Recording)
        return WI.CanvasTabContentView;

    return null;
};

WI.tabContentViewForRepresentedObject = function(representedObject, options = {})
{
    let tabContentView = WI.tabBrowser.bestTabContentViewForRepresentedObject(representedObject, options);
    if (tabContentView)
        return tabContentView;

    var tabContentViewClass = WI.tabContentViewClassForRepresentedObject(representedObject);
    if (!tabContentViewClass) {
        console.error("Unknown representedObject, couldn't create TabContentView.", representedObject);
        return null;
    }

    tabContentView = new tabContentViewClass;

    WI.tabBrowser.addTabForContentView(tabContentView);

    return tabContentView;
};

WI.showRepresentedObject = function(representedObject, cookie, options = {})
{
    let tabContentView = WI.tabContentViewForRepresentedObject(representedObject, options);
    console.assert(tabContentView);
    if (!tabContentView)
        return;

    WI.tabBrowser.showTabForContentView(tabContentView, options);
    tabContentView.showRepresentedObject(representedObject, cookie);
};

WI.showMainFrameDOMTree = function(nodeToSelect, options = {})
{
    console.assert(WI.networkManager.mainFrame);
    if (!WI.networkManager.mainFrame)
        return;
    WI.showRepresentedObject(WI.networkManager.mainFrame.domTree, {nodeToSelect}, options);
};

WI.showSourceCodeForFrame = function(frameIdentifier, options = {})
{
    var frame = WI.networkManager.frameForIdentifier(frameIdentifier);
    if (!frame) {
        WI._frameIdentifierToShowSourceCodeWhenAvailable = frameIdentifier;
        return;
    }

    WI._frameIdentifierToShowSourceCodeWhenAvailable = undefined;

    WI.showRepresentedObject(frame, null, options);
};

WI.showSourceCode = function(sourceCode, options = {})
{
    const positionToReveal = options.positionToReveal;

    console.assert(!positionToReveal || positionToReveal instanceof WI.SourceCodePosition, positionToReveal);
    var representedObject = sourceCode;

    if (representedObject instanceof WI.Script) {
        // A script represented by a resource should always show the resource.
        representedObject = representedObject.resource || representedObject;
    }

    var cookie = positionToReveal ? {lineNumber: positionToReveal.lineNumber, columnNumber: positionToReveal.columnNumber} : {};
    WI.showRepresentedObject(representedObject, cookie, options);
};

WI.showSourceCodeLocation = function(sourceCodeLocation, options = {})
{
    WI.showSourceCode(sourceCodeLocation.displaySourceCode, {
        ...options,
        positionToReveal: sourceCodeLocation.displayPosition(),
    });
};

WI.showOriginalUnformattedSourceCodeLocation = function(sourceCodeLocation, options = {})
{
    WI.showSourceCode(sourceCodeLocation.sourceCode, {
        ...options,
        positionToReveal: sourceCodeLocation.position(),
        forceUnformatted: true,
    });
};

WI.showOriginalOrFormattedSourceCodeLocation = function(sourceCodeLocation, options = {})
{
    WI.showSourceCode(sourceCodeLocation.sourceCode, {
        ...options,
        positionToReveal: sourceCodeLocation.formattedPosition(),
    });
};

WI.showOriginalOrFormattedSourceCodeTextRange = function(sourceCodeTextRange, options = {})
{
    var textRangeToSelect = sourceCodeTextRange.formattedTextRange;
    WI.showSourceCode(sourceCodeTextRange.sourceCode, {
        ...options,
        positionToReveal: textRangeToSelect.startPosition(),
        textRangeToSelect,
    });
};

WI.showResourceRequest = function(resource, options = {})
{
    WI.showRepresentedObject(resource, {[WI.ResourceClusterContentView.ContentViewIdentifierCookieKey]: WI.ResourceClusterContentView.CustomRequestIdentifier}, options);
};

WI.debuggerToggleBreakpoints = function(event)
{
    WI.debuggerManager.breakpointsEnabled = !WI.debuggerManager.breakpointsEnabled;
};

WI.debuggerPauseResumeToggle = function(event)
{
    if (WI.debuggerManager.paused)
        WI.debuggerManager.resume();
    else
        WI.debuggerManager.pause();
};

WI.debuggerStepOver = function(event)
{
    WI.debuggerManager.stepOver();
};

WI.debuggerStepInto = function(event)
{
    WI.debuggerManager.stepInto();
};

WI.debuggerStepOut = function(event)
{
    WI.debuggerManager.stepOut();
};

WI._searchTextDidChange = function(event)
{
    var tabContentView = WI.tabBrowser.bestTabContentViewForClass(WI.SearchTabContentView);
    if (!tabContentView)
        tabContentView = new WI.SearchTabContentView;

    var searchQuery = WI._searchToolbarItem.text;
    WI._searchToolbarItem.text = "";

    WI.tabBrowser.showTabForContentView(tabContentView);

    tabContentView.performSearch(searchQuery);
};

WI._focusSearchField = function(event)
{
    if (WI.settings.experimentalEnableNewTabBar.value)
        WI.tabBrowser.showTabForContentView(WI._searchTabContentView);

    if (WI.tabBrowser.selectedTabContentView instanceof WI.SearchTabContentView) {
        WI.tabBrowser.selectedTabContentView.focusSearchField();
        return;
    }

    if (WI._searchToolbarItem)
        WI._searchToolbarItem.focus();
};

WI._focusChanged = function(event)
{
    // Make a caret selection inside the focused element if there isn't a range selection and there isn't already
    // a caret selection inside. This is needed (at least) to remove caret from console when focus is moved.
    // The selection change should not apply to text fields and text areas either.

    if (WI.isEventTargetAnEditableField(event)) {
        // Still update the currentFocusElement if inside of a CodeMirror editor or an input element.
        let newFocusElement = null;
        if (event.target instanceof HTMLInputElement || event.target instanceof HTMLTextAreaElement)
            newFocusElement = event.target;
        else {
            let codeMirror = WI.enclosingCodeMirror(event.target);
            if (codeMirror) {
                let codeMirrorElement = codeMirror.getWrapperElement();
                if (codeMirrorElement && codeMirrorElement !== WI.currentFocusElement)
                    newFocusElement = codeMirrorElement;
            }
        }

        if (newFocusElement) {
            WI.previousFocusElement = WI.currentFocusElement;
            WI.currentFocusElement = newFocusElement;
        }

        // Due to the change in WI.isEventTargetAnEditableField (r196271), WI return
        // will also get run when WI.startEditing is called on an element. We do not want
        // to return early in WI case, as WI.EditingConfig handles its own editing
        // completion, so only return early if the focus change target is not from WI.startEditing.
        if (!WI.isBeingEdited(event.target))
            return;
    }

    var selection = window.getSelection();
    if (!selection.isCollapsed)
        return;

    var element = event.target;

    if (element !== WI.currentFocusElement) {
        WI.previousFocusElement = WI.currentFocusElement;
        WI.currentFocusElement = element;
    }

    if (element.isInsertionCaretInside())
        return;

    var selectionRange = element.ownerDocument.createRange();
    selectionRange.setStart(element, 0);
    selectionRange.setEnd(element, 0);

    selection.removeAllRanges();
    selection.addRange(selectionRange);
};

WI._mouseWasClicked = function(event)
{
    WI.handlePossibleLinkClick(event);
};

WI._handleDragOver = function(event)
{
    // Do nothing if another event listener handled the event already.
    if (event.defaultPrevented)
        return;

    // Allow dropping into editable areas.
    if (WI.isEventTargetAnEditableField(event))
        return;

    let tabContentView = WI.tabBrowser.selectedTabContentView;
    if (!tabContentView || !tabContentView.handleFileDrop || !event.dataTransfer.types.includes("Files")) {
        // Prevent the drop from being accepted.
        event.dataTransfer.dropEffect = "none";
    }

    event.preventDefault();
};

WI._handleDrop = function(event)
{
    // Do nothing if another event listener handled the event already.
    if (event.defaultPrevented)
        return;

    // Allow dropping into editable areas.
    if (WI.isEventTargetAnEditableField(event))
        return;

    let tabContentView = WI.tabBrowser.selectedTabContentView;
    if (tabContentView && tabContentView.handleFileDrop && event.dataTransfer.files) {
        event.preventDefault();

        tabContentView.handleFileDrop(event.dataTransfer.files)
        .then(() => {
            event.dataTransfer.clearData();
        });
    }
};

WI._debuggerDidPause = function(event)
{
    if (WI.settings.experimentalEnableSourcesTab.value)
        WI.showSourcesTab({showScopeChainSidebar: WI.settings.showScopeChainOnPause.value});
    else
        WI.showDebuggerTab({showScopeChainSidebar: WI.settings.showScopeChainOnPause.value});

    WI._dashboardContainer.showDashboardViewForRepresentedObject(WI._dashboards.debugger);

    InspectorFrontendHost.bringToFront();
};

WI._debuggerDidResume = function(event)
{
    WI._dashboardContainer.closeDashboardViewForRepresentedObject(WI._dashboards.debugger);
};

WI._frameWasAdded = function(event)
{
    if (!WI._frameIdentifierToShowSourceCodeWhenAvailable)
        return;

    var frame = event.data.frame;
    if (frame.id !== WI._frameIdentifierToShowSourceCodeWhenAvailable)
        return;

    function delayedWork()
    {
        const options = {
            ignoreNetworkTab: true,
            ignoreSearchTab: true,
        };
        WI.showSourceCodeForFrame(frame.id, options);
    }

    // Delay showing the frame since FrameWasAdded is called before MainFrameChanged.
    // Calling showSourceCodeForFrame before MainFrameChanged will show the frame then close it.
    setTimeout(delayedWork);
};

WI._mainFrameDidChange = function(event)
{
    WI._updateDownloadToolbarButton();

    WI.updateWindowTitle();
};

WI._mainResourceDidChange = function(event)
{
    if (!event.target.isMainFrame())
        return;

    // Run cookie restoration after we are sure all of the Tabs and NavigationSidebarPanels
    // have updated with respect to the main resource change.
    setTimeout(() => {
        WI._restoreCookieForOpenTabs(WI.StateRestorationType.Navigation);
    });

    WI._updateDownloadToolbarButton();

    WI.updateWindowTitle();
};

WI._provisionalLoadStarted = function(event)
{
    if (!event.target.isMainFrame())
        return;

    WI._saveCookieForOpenTabs();
};

WI._restoreCookieForOpenTabs = function(restorationType)
{
    for (var tabBarItem of WI.tabBar.tabBarItems) {
        var tabContentView = tabBarItem.representedObject;
        if (!(tabContentView instanceof WI.TabContentView))
            continue;
        tabContentView.restoreStateFromCookie(restorationType);
    }
};

WI._saveCookieForOpenTabs = function()
{
    for (var tabBarItem of WI.tabBar.tabBarItems) {
        var tabContentView = tabBarItem.representedObject;
        if (!(tabContentView instanceof WI.TabContentView))
            continue;
        tabContentView.saveStateToCookie();
    }
};

WI._windowFocused = function(event)
{
    if (event.target.document.nodeType !== Node.DOCUMENT_NODE)
        return;

    // FIXME: We should use the :window-inactive pseudo class once https://webkit.org/b/38927 is fixed.
    document.body.classList.remove(WI.docked ? "window-docked-inactive" : "window-inactive");
};

WI._windowBlurred = function(event)
{
    if (event.target.document.nodeType !== Node.DOCUMENT_NODE)
        return;

    // FIXME: We should use the :window-inactive pseudo class once https://webkit.org/b/38927 is fixed.
    document.body.classList.add(WI.docked ? "window-docked-inactive" : "window-inactive");
};

WI._windowResized = function(event)
{
    WI.toolbar.updateLayout(WI.View.LayoutReason.Resize);
    WI.tabBar.updateLayout(WI.View.LayoutReason.Resize);
    WI._tabBrowserSizeDidChange();
    WI._updateSheetRect();
};

WI._updateSheetRect = function()
{
    let mainElementRect = document.getElementById("main").getBoundingClientRect();
    InspectorFrontendHost.setSheetRect(mainElementRect.x, mainElementRect.y, mainElementRect.width, mainElementRect.height);
};

WI._updateModifierKeys = function(event)
{
    let keys = {
        altKey: event.altKey,
        metaKey: event.metaKey,
        ctrlKey: event.ctrlKey,
        shiftKey: event.shiftKey,
    };

    let changed = !Object.shallowEqual(WI.modifierKeys, keys);

    WI.modifierKeys = keys;

    document.body.classList.toggle("alt-key-pressed", WI.modifierKeys.altKey);
    document.body.classList.toggle("ctrl-key-pressed", WI.modifierKeys.ctrlKey);
    document.body.classList.toggle("meta-key-pressed", WI.modifierKeys.metaKey);
    document.body.classList.toggle("shift-key-pressed", WI.modifierKeys.shiftKey);

    if (changed)
        WI.notifications.dispatchEventToListeners(WI.Notification.GlobalModifierKeysDidChange, event);
};

WI._windowKeyDown = function(event)
{
    WI._updateModifierKeys(event);
};

WI._windowKeyUp = function(event)
{
    WI._updateModifierKeys(event);
};

WI._mouseDown = function(event)
{
    if (WI.toolbar.element.contains(event.target))
        WI._toolbarMouseDown(event);
};

WI._mouseMoved = function(event)
{
    WI._updateModifierKeys(event);
    WI.mouseCoords = {
        x: event.pageX,
        y: event.pageY
    };
};

WI._pageHidden = function(event)
{
    WI._saveCookieForOpenTabs();
};

WI._contextMenuRequested = function(event)
{
    let proposedContextMenu;

    // This is setting is only defined in engineering builds.
    if (WI.isDebugUIEnabled()) {
        proposedContextMenu = WI.ContextMenu.createFromEvent(event);
        proposedContextMenu.appendSeparator();
        proposedContextMenu.appendItem(WI.unlocalizedString("Reload Web Inspector"), () => {
            InspectorFrontendHost.reopen();
        });

        let protocolSubMenu = proposedContextMenu.appendSubMenuItem(WI.unlocalizedString("Protocol Debugging"), null, false);
        let isCapturingTraffic = InspectorBackend.activeTracer instanceof WI.CapturingProtocolTracer;

        protocolSubMenu.appendCheckboxItem(WI.unlocalizedString("Capture Trace"), () => {
            if (isCapturingTraffic)
                InspectorBackend.activeTracer = null;
            else
                InspectorBackend.activeTracer = new WI.CapturingProtocolTracer;
        }, isCapturingTraffic);

        protocolSubMenu.appendSeparator();

        protocolSubMenu.appendItem(WI.unlocalizedString("Export Trace\u2026"), () => {
            const forceSaveAs = true;
            WI.FileUtilities.save(InspectorBackend.activeTracer.trace.saveData, forceSaveAs);
        }, !isCapturingTraffic);
    } else {
        const onlyExisting = true;
        proposedContextMenu = WI.ContextMenu.createFromEvent(event, onlyExisting);
    }

    if (proposedContextMenu)
        proposedContextMenu.show();
};

WI.isDebugUIEnabled = function()
{
    return WI.showDebugUISetting && WI.showDebugUISetting.value;
};

WI._undock = function(event)
{
    InspectorFrontendHost.requestSetDockSide(WI.DockConfiguration.Undocked);
};

WI._dockBottom = function(event)
{
    InspectorFrontendHost.requestSetDockSide(WI.DockConfiguration.Bottom);
};

WI._dockRight = function(event)
{
    InspectorFrontendHost.requestSetDockSide(WI.DockConfiguration.Right);
};

WI._dockLeft = function(event)
{
    InspectorFrontendHost.requestSetDockSide(WI.DockConfiguration.Left);
};

WI._togglePreviousDockConfiguration = function(event)
{
    InspectorFrontendHost.requestSetDockSide(WI._previousDockConfiguration);
};

WI._updateDockNavigationItems = function()
{
    if (WI._dockingAvailable || WI.docked) {
        WI._closeToolbarButton.hidden = !WI.docked;
        WI._undockToolbarButton.hidden = WI._dockConfiguration === WI.DockConfiguration.Undocked;
        WI._dockBottomToolbarButton.hidden = WI._dockConfiguration === WI.DockConfiguration.Bottom;
        WI._dockToSideToolbarButton.hidden = WI._dockConfiguration === WI.DockConfiguration.Right || WI._dockConfiguration === WI.DockConfiguration.Left;
    } else {
        WI._closeToolbarButton.hidden = true;
        WI._undockToolbarButton.hidden = true;
        WI._dockBottomToolbarButton.hidden = true;
        WI._dockToSideToolbarButton.hidden = true;
    }
};

WI._tabBrowserSizeDidChange = function()
{
    WI.tabBrowser.updateLayout(WI.View.LayoutReason.Resize);
    WI.consoleDrawer.updateLayout(WI.View.LayoutReason.Resize);
    WI.quickConsole.updateLayout(WI.View.LayoutReason.Resize);
};

WI._consoleDrawerCollapsedStateDidChange = function(event)
{
    WI._showingSplitConsoleSetting.value = WI.isShowingSplitConsole();

    WI._consoleDrawerDidResize();
};

WI._consoleDrawerDidResize = function(event)
{
    WI.tabBrowser.updateLayout(WI.View.LayoutReason.Resize);
};

WI._sidebarWidthDidChange = function(event)
{
    WI._tabBrowserSizeDidChange();
};

WI._setupViewHierarchy = function()
{
    let rootView = WI.View.rootView();
    rootView.addSubview(WI.toolbar);
    rootView.addSubview(WI.tabBar);
    rootView.addSubview(WI.navigationSidebar);
    rootView.addSubview(WI.tabBrowser);
    rootView.addSubview(WI.consoleDrawer);
    rootView.addSubview(WI.quickConsole);
    rootView.addSubview(WI.detailsSidebar);
};

WI._tabBrowserSelectedTabContentViewDidChange = function(event)
{
    if (WI.tabBar.selectedTabBarItem && WI.tabBar.selectedTabBarItem.representedObject.constructor.shouldSaveTab())
        WI._selectedTabIndexSetting.value = WI.tabBar.tabBarItems.indexOf(WI.tabBar.selectedTabBarItem);

    if (WI.doesCurrentTabSupportSplitContentBrowser()) {
        if (WI._shouldRevealSpitConsoleIfSupported) {
            WI._shouldRevealSpitConsoleIfSupported = false;
            WI.showSplitConsole();
        }
        return;
    }

    WI._shouldRevealSpitConsoleIfSupported = WI.isShowingSplitConsole();
    WI.hideSplitConsole();
};

WI._toolbarMouseDown = function(event)
{
    if (event.ctrlKey)
        return;

    if (WI._dockConfiguration === WI.DockConfiguration.Right || WI._dockConfiguration === WI.DockConfiguration.Left)
        return;

    if (WI.docked)
        WI._dockedResizerMouseDown(event);
    else
        WI._moveWindowMouseDown(event);
};

WI._dockedResizerMouseDown = function(event)
{
    if (event.button !== 0 || event.ctrlKey)
        return;

    if (!WI.docked)
        return;

    // Only start dragging if the target is one of the elements that we expect.
    if (event.target.id !== "docked-resizer" && !event.target.classList.contains("toolbar") &&
        !event.target.classList.contains("flexible-space") && !event.target.classList.contains("item-section"))
        return;

    event[WI.Popover.EventPreventDismissSymbol] = true;

    let windowProperty = WI._dockConfiguration === WI.DockConfiguration.Bottom ? "innerHeight" : "innerWidth";
    let eventScreenProperty = WI._dockConfiguration === WI.DockConfiguration.Bottom ? "screenY" : "screenX";
    let eventClientProperty = WI._dockConfiguration === WI.DockConfiguration.Bottom ? "clientY" : "clientX";

    var resizerElement = event.target;
    var firstClientPosition = event[eventClientProperty];
    var lastScreenPosition = event[eventScreenProperty];

    function dockedResizerDrag(event)
    {
        if (event.button !== 0)
            return;

        var position = event[eventScreenProperty];
        var delta = position - lastScreenPosition;
        var clientPosition = event[eventClientProperty];

        lastScreenPosition = position;

        if (WI._dockConfiguration === WI.DockConfiguration.Left) {
            // If the mouse is travelling rightward but is positioned left of the resizer, ignore the event.
            if (delta > 0 && clientPosition < firstClientPosition)
                return;

            // If the mouse is travelling leftward but is positioned to the right of the resizer, ignore the event.
            if (delta < 0 && clientPosition > window[windowProperty])
                return;

            // We later subtract the delta from the current position, but since the inspected view and inspector view
            // are flipped when docked to left, we want dragging to have the opposite effect from docked to right.
            delta *= -1;
        } else {
            // If the mouse is travelling downward/rightward but is positioned above/left of the resizer, ignore the event.
            if (delta > 0 && clientPosition < firstClientPosition)
                return;

            // If the mouse is travelling upward/leftward but is positioned below/right of the resizer, ignore the event.
            if (delta < 0 && clientPosition > firstClientPosition)
                return;
        }

        let dimension = Math.max(0, window[windowProperty] - delta);
        // If zoomed in/out, there be greater/fewer document pixels shown, but the inspector's
        // width or height should be the same in device pixels regardless of the document zoom.
        dimension *= WI.getZoomFactor();

        if (WI._dockConfiguration === WI.DockConfiguration.Bottom)
            InspectorFrontendHost.setAttachedWindowHeight(dimension);
        else
            InspectorFrontendHost.setAttachedWindowWidth(dimension);
    }

    function dockedResizerDragEnd(event)
    {
        if (event.button !== 0)
            return;

        WI.elementDragEnd(event);
    }

    WI.elementDragStart(resizerElement, dockedResizerDrag, dockedResizerDragEnd, event, WI._dockConfiguration === WI.DockConfiguration.Bottom ? "row-resize" : "col-resize");
};

WI._moveWindowMouseDown = function(event)
{
    console.assert(!WI.docked);

    if (event.button !== 0 || event.ctrlKey)
        return;

    // Only start dragging if the target is one of the elements that we expect.
    if (!event.target.classList.contains("toolbar") && !event.target.classList.contains("flexible-space") &&
        !event.target.classList.contains("item-section"))
        return;

    event[WI.Popover.EventPreventDismissSymbol] = true;

    if (WI.Platform.name === "mac") {
        InspectorFrontendHost.startWindowDrag();
        event.preventDefault();
        return;
    }

    var lastScreenX = event.screenX;
    var lastScreenY = event.screenY;

    function toolbarDrag(event)
    {
        if (event.button !== 0)
            return;

        var x = event.screenX - lastScreenX;
        var y = event.screenY - lastScreenY;

        InspectorFrontendHost.moveWindowBy(x, y);

        lastScreenX = event.screenX;
        lastScreenY = event.screenY;
    }

    function toolbarDragEnd(event)
    {
        if (event.button !== 0)
            return;

        WI.elementDragEnd(event);
    }

    WI.elementDragStart(event.target, toolbarDrag, toolbarDragEnd, event, "default");
};

WI._domStorageWasInspected = function(event)
{
    WI.showStorageTab();
    WI.showRepresentedObject(event.data.domStorage, null, {ignoreSearchTab: true});
};

WI._databaseWasInspected = function(event)
{
    WI.showStorageTab();
    WI.showRepresentedObject(event.data.database, null, {ignoreSearchTab: true});
};

WI._domNodeWasInspected = function(event)
{
    WI.domManager.highlightDOMNodeForTwoSeconds(event.data.node.id);

    InspectorFrontendHost.bringToFront();

    WI.showElementsTab();
    WI.showMainFrameDOMTree(event.data.node, {ignoreSearchTab: true});
};

WI._inspectModeStateChanged = function(event)
{
    WI._inspectModeToolbarButton.activated = WI.domManager.inspectModeEnabled;
};

WI._toggleInspectMode = function(event)
{
    WI.domManager.inspectModeEnabled = !WI.domManager.inspectModeEnabled;
};

WI._handleDeviceSettingsToolbarButtonClicked = function(event)
{
    if (WI._deviceSettingsPopover) {
        WI._deviceSettingsPopover.dismiss();
        WI._deviceSettingsPopover = null;
        return;
    }

    function updateActivatedState() {
        WI._deviceSettingsToolbarButton.activated = WI._overridenDeviceUserAgent || WI._overridenDeviceSettings.size > 0;
    }

    function applyOverriddenUserAgent(value, force) {
        if (value === WI._overridenDeviceUserAgent)
            return;

        if (!force && (!value || value === "default")) {
            PageAgent.overrideUserAgent((error) => {
                if (error) {
                    console.error(error);
                    return;
                }

                WI._overridenDeviceUserAgent = null;
                updateActivatedState();
                PageAgent.reload();
            });
        } else {
            PageAgent.overrideUserAgent(value, (error) => {
                if (error) {
                    console.error(error);
                    return;
                }

                WI._overridenDeviceUserAgent = value;
                updateActivatedState();
                PageAgent.reload();
            });
        }
    }

    function applyOverriddenSetting(setting, value, callback) {
        if (WI._overridenDeviceSettings.has(setting)) {
            // We've just "disabled" the checkbox, so clear the override instead of applying it.
            PageAgent.overrideSetting(setting, (error) => {
                if (error) {
                    console.error(error);
                    return;
                }

                WI._overridenDeviceSettings.delete(setting);
                callback(false);
                updateActivatedState();
            });
        } else {
            PageAgent.overrideSetting(setting, value, (error) => {
                if (error) {
                    console.error(error);
                    return;
                }

                WI._overridenDeviceSettings.set(setting, value);
                callback(true);
                updateActivatedState();
            });
        }
    }

    function createCheckbox(container, label, setting, value) {
        if (!setting)
            return;

        let labelElement = container.appendChild(document.createElement("label"));

        let checkboxElement = labelElement.appendChild(document.createElement("input"));
        checkboxElement.type = "checkbox";
        checkboxElement.checked = WI._overridenDeviceSettings.has(setting);
        checkboxElement.addEventListener("change", (event) => {
            applyOverriddenSetting(setting, value, (enabled) => {
                checkboxElement.checked = enabled;
            });
        });

        labelElement.append(label);
    }

    function calculateTargetFrame() {
        return WI.Rect.rectFromClientRect(WI._deviceSettingsToolbarButton.element.getBoundingClientRect());
    }

    const preferredEdges = [WI.RectEdge.MAX_Y, WI.RectEdge.MIN_X, WI.RectEdge.MAX_X];

    WI._deviceSettingsPopover = new WI.Popover(WI);
    WI._deviceSettingsPopover.windowResizeHandler = function(event) {
        WI._deviceSettingsPopover.present(calculateTargetFrame(), preferredEdges);
    };

    let contentElement = document.createElement("table");
    contentElement.classList.add("device-settings-content");

    let userAgentRow = contentElement.appendChild(document.createElement("tr"));

    let userAgentTitle = userAgentRow.appendChild(document.createElement("td"));
    userAgentTitle.textContent = WI.UIString("User Agent:");

    let userAgentValue = userAgentRow.appendChild(document.createElement("td"));
    userAgentValue.classList.add("user-agent");

    let userAgentValueSelect = userAgentValue.appendChild(document.createElement("select"));

    let userAgentValueInput = null;

    const userAgents = [
        [
            { name: WI.UIString("Default"), value: "default" },
        ],
        [
            { name: "Safari 13.0", value: "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/13.0 Safari/605.1.15" },
        ],
        [
            { name: `Safari ${emDash} iOS 12.1.3 ${emDash} iPhone`, value: "Mozilla/5.0 (iPhone; CPU iPhone OS 12_1_3 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/12.0 Mobile/15E148 Safari/604.1" },
            { name: `Safari ${emDash} iOS 12.1.3 ${emDash} iPod touch`, value: "Mozilla/5.0 (iPod; CPU iPhone OS 12_1_3 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/12.0 Mobile/15E148 Safari/604.1" },
            { name: `Safari ${emDash} iOS 12.1.3 ${emDash} iPad`, value: "Mozilla/5.0 (iPad; CPU iPhone OS 12_1_3 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/12.0 Mobile/15E148 Safari/604.1" },
        ],
        [
            { name: `Microsoft Edge`, value: "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36 Edge/16.16299" },
        ],
        [
            { name: `Google Chrome ${emDash} macOS`, value: "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_14_3) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/71.0.3578.98 Safari/537.36" },
            { name: `Google Chrome ${emDash} Windows`, value: "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/71.0.3578.98 Safari/537.36" },
        ],
        [
            { name: `Firefox ${emDash} macOS`, value: "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.14; rv:63.0) Gecko/20100101 Firefox/63.0" },
            { name: `Firefox ${emDash} Windows`, value: "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:63.0) Gecko/20100101 Firefox/63.0" },
        ],
        [
            { name: WI.UIString("Other\u2026"), value: "other" },
        ],
    ];

    let selectedOptionElement = null;

    for (let group of userAgents) {
        for (let {name, value} of group) {
            let optionElement = userAgentValueSelect.appendChild(document.createElement("option"));
            optionElement.value = value;
            optionElement.textContent = name;

            if (value === WI._overridenDeviceUserAgent)
                selectedOptionElement = optionElement;
        }

        if (group !== userAgents.lastValue)
            userAgentValueSelect.appendChild(document.createElement("hr"));
    }

    function showUserAgentInput() {
        if (userAgentValueInput)
            return;

        userAgentValueInput = userAgentValue.appendChild(document.createElement("input"));
        userAgentValueInput.value = userAgentValueInput.placeholder = WI._overridenDeviceUserAgent || navigator.userAgent;
        userAgentValueInput.addEventListener("click", (clickEvent) => {
            clickEvent.preventDefault();
        });
        userAgentValueInput.addEventListener("change", (inputEvent) => {
            applyOverriddenUserAgent(userAgentValueInput.value, true);
        });

        WI._deviceSettingsPopover.update();
    }

    if (selectedOptionElement)
        userAgentValueSelect.value = selectedOptionElement.value;
    else if (WI._overridenDeviceUserAgent) {
        userAgentValueSelect.value = "other";
        showUserAgentInput();
    }

    userAgentValueSelect.addEventListener("change", () => {
        let value = userAgentValueSelect.value;
        if (value === "other") {
            showUserAgentInput();
            userAgentValueInput.select();
        } else {
            if (userAgentValueInput) {
                userAgentValueInput.remove();
                userAgentValueInput = null;

                WI._deviceSettingsPopover.update();
            }

            applyOverriddenUserAgent(value);
        }
    });

    const settings = [
        {
            name: WI.UIString("Disable:"),
            columns: [
                [
                    {name: WI.UIString("Images"), setting: PageAgent.Setting.ImagesEnabled, value: false},
                    {name: WI.UIString("Styles"), setting: PageAgent.Setting.AuthorAndUserStylesEnabled, value: false},
                    {name: WI.UIString("JavaScript"), setting: PageAgent.Setting.ScriptEnabled, value: false},
                ],
                [
                    {name: WI.UIString("Site-specific Hacks"), setting: PageAgent.Setting.NeedsSiteSpecificQuirks, value: false},
                    {name: WI.UIString("Cross-Origin Restrictions"), setting: PageAgent.Setting.WebSecurityEnabled, value: false},
                ]
            ],
        },
        {
            name: WI.UIString("%s:").format(WI.unlocalizedString("WebRTC")),
            columns: [
                [
                    {name: WI.UIString("Allow Media Capture on Insecure Sites"), setting: PageAgent.Setting.MediaCaptureRequiresSecureConnection, value: false},
                    {name: WI.UIString("Disable ICE Candidate Restrictions"), setting: PageAgent.Setting.ICECandidateFilteringEnabled, value: false},
                    {name: WI.UIString("Use Mock Capture Devices"), setting: PageAgent.Setting.MockCaptureDevicesEnabled, value: true},
                    {name: WI.UIString("Disable Encryption"), setting: PageAgent.Setting.WebRTCEncryptionEnabled, value: false},
                ],
            ],
        },
    ];

    for (let group of settings) {
        if (!group.columns.some((column) => column.some((item) => item.setting)))
            continue;

        let settingsGroupRow = contentElement.appendChild(document.createElement("tr"));

        let settingsGroupTitle = settingsGroupRow.appendChild(document.createElement("td"));
        settingsGroupTitle.textContent = group.name;

        let settingsGroupValue = settingsGroupRow.appendChild(document.createElement("td"));

        let settingsGroupItemContainer = settingsGroupValue.appendChild(document.createElement("div"));
        settingsGroupItemContainer.classList.add("container");

        for (let column of group.columns) {
            let columnElement = settingsGroupItemContainer.appendChild(document.createElement("div"));
            columnElement.classList.add("column");

            for (let item of column)
                createCheckbox(columnElement, item.name, item.setting, item.value);
        }
    }

    WI._deviceSettingsPopover.presentNewContentWithFrame(contentElement, calculateTargetFrame(), preferredEdges);
};

WI._downloadWebArchive = function(event)
{
    WI.archiveMainFrame();
};

WI._reloadInspectedInspector = function()
{
    const options = {};
    WI.runtimeManager.evaluateInInspectedWindow(`InspectorFrontendHost.reopen()`, options, function(){});
};

WI._reloadPage = function(event)
{
    if (!window.PageAgent)
        return;

    event.preventDefault();

    if (InspectorFrontendHost.inspectionLevel() > 1) {
        WI._reloadInspectedInspector();
        return;
    }

    PageAgent.reload();
};

WI._reloadPageFromOrigin = function(event)
{
    if (!window.PageAgent)
        return;

    event.preventDefault();

    if (InspectorFrontendHost.inspectionLevel() > 1) {
        WI._reloadInspectedInspector();
        return;
    }

    PageAgent.reload.invoke({ignoreCache: true});
};

WI._reloadToolbarButtonClicked = function(event)
{
    if (InspectorFrontendHost.inspectionLevel() > 1) {
        WI._reloadInspectedInspector();
        return;
    }

    // Reload page from origin if the button is clicked while the shift key is pressed down.
    PageAgent.reload.invoke({ignoreCache: WI.modifierKeys.shiftKey});
};

WI._updateReloadToolbarButton = function()
{
    if (!window.PageAgent) {
        WI._reloadToolbarButton.hidden = true;
        return;
    }

    WI._reloadToolbarButton.hidden = false;
};

WI._updateDownloadToolbarButton = function()
{
    if (!window.PageAgent || WI.sharedApp.debuggableType !== WI.DebuggableType.Web) {
        WI._downloadToolbarButton.hidden = true;
        return;
    }

    if (WI._downloadingPage) {
        WI._downloadToolbarButton.enabled = false;
        return;
    }

    WI._downloadToolbarButton.enabled = WI.canArchiveMainFrame();
};

WI._updateInspectModeToolbarButton = function()
{
    if (!window.DOMAgent || !DOMAgent.setInspectModeEnabled) {
        WI._inspectModeToolbarButton.hidden = true;
        return;
    }

    WI._inspectModeToolbarButton.hidden = false;
};

WI._toggleInspectMode = function(event)
{
    WI.domManager.inspectModeEnabled = !WI.domManager.inspectModeEnabled;
};

WI._showConsoleTab = function(event)
{
    WI.showConsoleTab();
};

WI._focusConsolePrompt = function(event)
{
    WI.quickConsole.prompt.focus();
};

WI._focusedContentBrowser = function()
{
    if (WI.currentFocusElement) {
        let contentBrowserElement = WI.currentFocusElement.closest(".content-browser");
        if (contentBrowserElement && contentBrowserElement.__view && contentBrowserElement.__view instanceof WI.ContentBrowser)
            return contentBrowserElement.__view;
    }

    if (WI.tabBrowser.element.contains(WI.currentFocusElement) || document.activeElement === document.body) {
        let tabContentView = WI.tabBrowser.selectedTabContentView;
        if (tabContentView.contentBrowser)
            return tabContentView.contentBrowser;
        return null;
    }

    if (WI.consoleDrawer.element.contains(WI.currentFocusElement)
        || (WI.isShowingSplitConsole() && WI.quickConsole.element.contains(WI.currentFocusElement)))
        return WI.consoleDrawer;

    return null;
};

WI._focusedContentView = function()
{
    if (WI.tabBrowser.element.contains(WI.currentFocusElement) || document.activeElement === document.body) {
        var tabContentView = WI.tabBrowser.selectedTabContentView;
        if (tabContentView.contentBrowser)
            return tabContentView.contentBrowser.currentContentView;
        return tabContentView;
    }

    if (WI.consoleDrawer.element.contains(WI.currentFocusElement)
        || (WI.isShowingSplitConsole() && WI.quickConsole.element.contains(WI.currentFocusElement)))
        return WI.consoleDrawer.currentContentView;

    return null;
};

WI._focusedOrVisibleContentBrowser = function()
{
    let focusedContentBrowser = WI._focusedContentBrowser();
    if (focusedContentBrowser)
        return focusedContentBrowser;

    var tabContentView = WI.tabBrowser.selectedTabContentView;
    if (tabContentView.contentBrowser)
        return tabContentView.contentBrowser;

    return null;
};

WI.focusedOrVisibleContentView = function()
{
    let focusedContentView = WI._focusedContentView();
    if (focusedContentView)
        return focusedContentView;

    var tabContentView = WI.tabBrowser.selectedTabContentView;
    if (tabContentView.contentBrowser)
        return tabContentView.contentBrowser.currentContentView;
    return tabContentView;
};

WI._beforecopy = function(event)
{
    var selection = window.getSelection();

    // If there is no selection, see if the focused element or focused ContentView can handle the copy event.
    if (selection.isCollapsed && !WI.isEventTargetAnEditableField(event)) {
        var focusedCopyHandler = WI.currentFocusElement && WI.currentFocusElement.copyHandler;
        if (focusedCopyHandler && typeof focusedCopyHandler.handleBeforeCopyEvent === "function") {
            focusedCopyHandler.handleBeforeCopyEvent(event);
            if (event.defaultPrevented)
                return;
        }

        var focusedContentView = WI._focusedContentView();
        if (focusedContentView && typeof focusedContentView.handleCopyEvent === "function") {
            event.preventDefault();
            return;
        }

        return;
    }

    if (selection.isCollapsed)
        return;

    // Say we can handle it (by preventing default) to remove word break characters.
    event.preventDefault();
};

WI._find = function(event)
{
    let tabContentView = WI.tabBrowser.selectedTabContentView;
    if (tabContentView && tabContentView.canHandleFindEvent) {
        tabContentView.handleFindEvent(event);
        return;
    }

    let contentBrowser = WI._focusedOrVisibleContentBrowser();
    if (!contentBrowser)
        return;

    contentBrowser.showFindBanner();
};

WI._save = function(event)
{
    var contentView = WI.focusedOrVisibleContentView();
    if (!contentView || !contentView.supportsSave)
        return;

    WI.FileUtilities.save(contentView.saveData);
};

WI._saveAs = function(event)
{
    var contentView = WI.focusedOrVisibleContentView();
    if (!contentView || !contentView.supportsSave)
        return;

    WI.FileUtilities.save(contentView.saveData, true);
};

WI._clear = function(event)
{
    let contentView = WI.focusedOrVisibleContentView();
    if (!contentView || typeof contentView.handleClearShortcut !== "function") {
        // If the current content view is unable to handle WI event, clear the console to reset
        // the dashboard counters.
        WI.consoleManager.requestClearMessages();
        return;
    }

    contentView.handleClearShortcut(event);
};

WI._populateFind = function(event)
{
    let focusedContentView = WI._focusedContentView();
    if (!focusedContentView)
        return;

    if (focusedContentView.supportsCustomFindBanner) {
        focusedContentView.handlePopulateFindShortcut();
        return;
    }

    let contentBrowser = WI._focusedOrVisibleContentBrowser();
    if (!contentBrowser)
        return;

    contentBrowser.handlePopulateFindShortcut();
};

WI._findNext = function(event)
{
    let focusedContentView = WI._focusedContentView();
    if (!focusedContentView)
        return;

    if (focusedContentView.supportsCustomFindBanner) {
        focusedContentView.handleFindNextShortcut();
        return;
    }

    let contentBrowser = WI._focusedOrVisibleContentBrowser();
    if (!contentBrowser)
        return;

    contentBrowser.handleFindNextShortcut();
};

WI._findPrevious = function(event)
{
    let focusedContentView = WI._focusedContentView();
    if (!focusedContentView)
        return;

    if (focusedContentView.supportsCustomFindBanner) {
        focusedContentView.handleFindPreviousShortcut();
        return;
    }

    let contentBrowser = WI._focusedOrVisibleContentBrowser();
    if (!contentBrowser)
        return;

    contentBrowser.handleFindPreviousShortcut();
};

WI._copy = function(event)
{
    var selection = window.getSelection();

    // If there is no selection, pass the copy event on to the focused element or focused ContentView.
    if (selection.isCollapsed && !WI.isEventTargetAnEditableField(event)) {
        var focusedCopyHandler = WI.currentFocusElement && WI.currentFocusElement.copyHandler;
        if (focusedCopyHandler && typeof focusedCopyHandler.handleCopyEvent === "function") {
            focusedCopyHandler.handleCopyEvent(event);
            if (event.defaultPrevented)
                return;
        }

        var focusedContentView = WI._focusedContentView();
        if (focusedContentView && typeof focusedContentView.handleCopyEvent === "function") {
            focusedContentView.handleCopyEvent(event);
            return;
        }

        let tabContentView = WI.tabBrowser.selectedTabContentView;
        if (tabContentView && typeof tabContentView.handleCopyEvent === "function") {
            tabContentView.handleCopyEvent(event);
            return;
        }

        return;
    }

    if (selection.isCollapsed)
        return;

    // Remove word break characters from the selection before putting it on the pasteboard.
    var selectionString = selection.toString().removeWordBreakCharacters();
    event.clipboardData.setData("text/plain", selectionString);
    event.preventDefault();
};

WI._paste = function(event)
{
    if (event.defaultPrevented)
        return;

    let selection = window.getSelection();

    // If there is no selection, pass the paste event on to the focused element or focused ContentView.
    if (!selection.isCollapsed || WI.isEventTargetAnEditableField(event))
        return;

    let focusedPasteHandler = WI.currentFocusElement && WI.currentFocusElement.pasteHandler;
    if (focusedPasteHandler && focusedPasteHandler.handlePasteEvent) {
        focusedPasteHandler.handlePasteEvent(event);
        if (event.defaultPrevented)
            return;
    }

    let focusedContentView = WI._focusedContentView();
    if (focusedContentView && focusedContentView.handlePasteEvent) {
        focusedContentView.handlePasteEvent(event);
        return;
    }

    let tabContentView = WI.tabBrowser.selectedTabContentView;
    if (tabContentView && tabContentView.handlePasteEvent) {
        tabContentView.handlePasteEvent(event);
        return;
    }
};

WI._increaseZoom = function(event)
{
    const epsilon = 0.0001;
    const maximumZoom = 2.4;
    let currentZoom = WI.getZoomFactor();
    if (currentZoom + epsilon >= maximumZoom) {
        InspectorFrontendHost.beep();
        return;
    }

    WI.setZoomFactor(Math.min(maximumZoom, currentZoom + 0.2));
};

WI._decreaseZoom = function(event)
{
    const epsilon = 0.0001;
    const minimumZoom = 0.6;
    let currentZoom = WI.getZoomFactor();
    if (currentZoom - epsilon <= minimumZoom) {
        InspectorFrontendHost.beep();
        return;
    }

    WI.setZoomFactor(Math.max(minimumZoom, currentZoom - 0.2));
};

WI._resetZoom = function(event)
{
    WI.setZoomFactor(1);
};

WI.getZoomFactor = function()
{
    return WI.settings.zoomFactor.value;
};

WI.setZoomFactor = function(factor)
{
    InspectorFrontendHost.setZoomFactor(factor);
    // Round-trip through the frontend host API in case the requested factor is not used.
    WI.settings.zoomFactor.value = InspectorFrontendHost.zoomFactor();
};

WI.resolvedLayoutDirection = function()
{
    let layoutDirection = WI.settings.debugLayoutDirection.value;
    if (!WI.isDebugUIEnabled() || layoutDirection === WI.LayoutDirection.System)
        layoutDirection = InspectorFrontendHost.userInterfaceLayoutDirection();
    return layoutDirection;
};

WI.setLayoutDirection = function(value)
{
    console.assert(WI.isDebugUIEnabled());

    if (!Object.values(WI.LayoutDirection).includes(value))
        WI.reportInternalError("Unknown layout direction requested: " + value);

    if (value === WI.settings.debugLayoutDirection.value)
        return;

    WI.settings.debugLayoutDirection.value = value;

    if (WI.resolvedLayoutDirection() === WI.LayoutDirection.RTL && WI._dockConfiguration === WI.DockConfiguration.Right)
        WI._dockLeft();

    if (WI.resolvedLayoutDirection() === WI.LayoutDirection.LTR && WI._dockConfiguration === WI.DockConfiguration.Left)
        WI._dockRight();

    InspectorFrontendHost.reopen();
};

WI._showTabAtIndex = function(i)
{
    if (i <= WI.tabBar.tabBarItems.length)
        WI.tabBar.selectedTabBarItem = i - 1;
};

WI._showJavaScriptTypeInformationSettingChanged = function(event)
{
    if (WI.settings.showJavaScriptTypeInformation.value) {
        for (let target of WI.targets)
            target.RuntimeAgent.enableTypeProfiler();
    } else {
        for (let target of WI.targets)
            target.RuntimeAgent.disableTypeProfiler();
    }
};

WI._enableControlFlowProfilerSettingChanged = function(event)
{
    if (WI.settings.enableControlFlowProfiler.value) {
        for (let target of WI.targets)
            target.RuntimeAgent.enableControlFlowProfiler();
    } else {
        for (let target of WI.targets)
            target.RuntimeAgent.disableControlFlowProfiler();
    }
};

WI._resourceCachingDisabledSettingChanged = function(event)
{
    NetworkAgent.setResourceCachingDisabled(WI.settings.resourceCachingDisabled.value);
};

WI.elementDragStart = function(element, dividerDrag, elementDragEnd, event, cursor, eventTarget)
{
    if (WI._elementDraggingEventListener || WI._elementEndDraggingEventListener)
        WI.elementDragEnd(event);

    if (element) {
        // Install glass pane
        if (WI._elementDraggingGlassPane)
            WI._elementDraggingGlassPane.remove();

        var glassPane = document.createElement("div");
        glassPane.style.cssText = "position:absolute;top:0;bottom:0;left:0;right:0;opacity:0;z-index:1";
        glassPane.id = "glass-pane-for-drag";
        element.ownerDocument.body.appendChild(glassPane);
        WI._elementDraggingGlassPane = glassPane;
    }

    WI._elementDraggingEventListener = dividerDrag;
    WI._elementEndDraggingEventListener = elementDragEnd;

    var targetDocument = event.target.ownerDocument;

    WI._elementDraggingEventTarget = eventTarget || targetDocument;
    WI._elementDraggingEventTarget.addEventListener("mousemove", dividerDrag, true);
    WI._elementDraggingEventTarget.addEventListener("mouseup", elementDragEnd, true);

    targetDocument.body.style.cursor = cursor;

    event.preventDefault();
};

WI.elementDragEnd = function(event)
{
    WI._elementDraggingEventTarget.removeEventListener("mousemove", WI._elementDraggingEventListener, true);
    WI._elementDraggingEventTarget.removeEventListener("mouseup", WI._elementEndDraggingEventListener, true);

    event.target.ownerDocument.body.style.removeProperty("cursor");

    if (WI._elementDraggingGlassPane)
        WI._elementDraggingGlassPane.remove();

    delete WI._elementDraggingGlassPane;
    delete WI._elementDraggingEventTarget;
    delete WI._elementDraggingEventListener;
    delete WI._elementEndDraggingEventListener;

    event.preventDefault();
};

WI.createMessageTextView = function(message, isError)
{
    let messageElement = document.createElement("div");
    messageElement.className = "message-text-view";
    if (isError)
        messageElement.classList.add("error");

    let textElement = messageElement.appendChild(document.createElement("div"));
    textElement.className = "message";
    textElement.textContent = message;

    return messageElement;
};

WI.createNavigationItemHelp = function(formatString, navigationItem)
{
    console.assert(typeof formatString === "string");
    console.assert(navigationItem instanceof WI.NavigationItem);

    function append(a, b) {
        a.append(b);
        return a;
    }

    let containerElement = document.createElement("div");
    containerElement.className = "navigation-item-help";
    containerElement.__navigationItem = navigationItem;

    let wrapperElement = document.createElement("div");
    wrapperElement.className = "navigation-bar";
    wrapperElement.appendChild(navigationItem.element);

    String.format(formatString, [wrapperElement], String.standardFormatters, containerElement, append);
    return containerElement;
};

WI.createGoToArrowButton = function()
{
    var button = document.createElement("button");
    button.addEventListener("mousedown", (event) => { event.stopPropagation(); }, true);
    button.className = "go-to-arrow";
    button.tabIndex = -1;
    return button;
};

WI.createSourceCodeLocationLink = function(sourceCodeLocation, options = {})
{
    console.assert(sourceCodeLocation);
    if (!sourceCodeLocation)
        return null;

    var linkElement = document.createElement("a");
    linkElement.className = "go-to-link";
    WI.linkifyElement(linkElement, sourceCodeLocation, options);
    sourceCodeLocation.populateLiveDisplayLocationTooltip(linkElement);

    if (options.useGoToArrowButton)
        linkElement.appendChild(WI.createGoToArrowButton());
    else
        sourceCodeLocation.populateLiveDisplayLocationString(linkElement, "textContent", options.columnStyle, options.nameStyle, options.prefix);

    if (options.dontFloat)
        linkElement.classList.add("dont-float");

    return linkElement;
};

WI.linkifyLocation = function(url, sourceCodePosition, options = {})
{
    var sourceCode = WI.sourceCodeForURL(url);
    if (sourceCode)
        return WI.linkifySourceCode(sourceCode, sourceCodePosition, options);

    var anchor = document.createElement("a");
    anchor.href = url;
    anchor.lineNumber = sourceCodePosition.lineNumber;
    if (options.className)
        anchor.className = options.className;
    anchor.append(WI.displayNameForURL(url) + ":" + sourceCodePosition.lineNumber);
    return anchor;
};

WI.linkifySourceCode = function(sourceCode, sourceCodePosition, options = {})
{
    let sourceCodeLocation = sourceCode.createSourceCodeLocation(sourceCodePosition.lineNumber, sourceCodePosition.columnNumber);
    let linkElement = WI.createSourceCodeLocationLink(sourceCodeLocation, {
        ...options,
        dontFloat: true,
    });

    if (options.className)
        linkElement.classList.add(options.className);

    return linkElement;
};

WI.linkifyElement = function(linkElement, sourceCodeLocation, options = {}) {
    console.assert(sourceCodeLocation);

    function showSourceCodeLocation(event)
    {
        event.stopPropagation();
        event.preventDefault();

        if (event.metaKey)
            WI.showOriginalUnformattedSourceCodeLocation(sourceCodeLocation, options);
        else
            WI.showSourceCodeLocation(sourceCodeLocation, options);
    }

    linkElement.addEventListener("click", showSourceCodeLocation);
    linkElement.addEventListener("contextmenu", (event) => {
        let contextMenu = WI.ContextMenu.createFromEvent(event);
        WI.appendContextMenuItemsForSourceCode(contextMenu, sourceCodeLocation);
    });
};

WI.sourceCodeForURL = function(url)
{
    var sourceCode = WI.networkManager.resourceForURL(url);
    if (!sourceCode) {
        sourceCode = WI.debuggerManager.scriptsForURL(url, WI.assumingMainTarget())[0];
        if (sourceCode)
            sourceCode = sourceCode.resource || sourceCode;
    }
    return sourceCode || null;
};

WI.linkifyURLAsNode = function(url, linkText, className)
{
    let a = document.createElement("a");
    a.href = url;
    a.className = className || "";
    a.textContent = linkText || url;
    a.style.maxWidth = "100%";
    return a;
};

WI.linkifyStringAsFragmentWithCustomLinkifier = function(string, linkifier)
{
    var container = document.createDocumentFragment();
    var linkStringRegEx = /(?:[a-zA-Z][a-zA-Z0-9+.-]{2,}:\/\/|www\.)[\w$\-_+*'=\|\/\\(){}[\]%@&#~,:;.!?]{2,}[\w$\-_+*=\|\/\\({%@&#~]/;
    var lineColumnRegEx = /:(\d+)(:(\d+))?$/;

    while (string) {
        var linkString = linkStringRegEx.exec(string);
        if (!linkString)
            break;

        linkString = linkString[0];
        var linkIndex = string.indexOf(linkString);
        var nonLink = string.substring(0, linkIndex);
        container.append(nonLink);

        if (linkString.startsWith("data:") || linkString.startsWith("javascript:") || linkString.startsWith("mailto:")) {
            container.append(linkString);
            string = string.substring(linkIndex + linkString.length, string.length);
            continue;
        }

        var title = linkString;
        var realURL = linkString.startsWith("www.") ? "http://" + linkString : linkString;
        var lineColumnMatch = lineColumnRegEx.exec(realURL);
        if (lineColumnMatch)
            realURL = realURL.substring(0, realURL.length - lineColumnMatch[0].length);

        var lineNumber;
        if (lineColumnMatch)
            lineNumber = parseInt(lineColumnMatch[1]) - 1;

        var linkNode = linkifier(title, realURL, lineNumber);
        container.appendChild(linkNode);
        string = string.substring(linkIndex + linkString.length, string.length);
    }

    if (string)
        container.append(string);

    return container;
};

WI.linkifyStringAsFragment = function(string)
{
    function linkifier(title, url, lineNumber)
    {
        var urlNode = WI.linkifyURLAsNode(url, title, undefined);
        if (lineNumber !== undefined)
            urlNode.lineNumber = lineNumber;

        return urlNode;
    }

    return WI.linkifyStringAsFragmentWithCustomLinkifier(string, linkifier);
};

WI.createResourceLink = function(resource, className)
{
    function handleClick(event)
    {
        event.stopPropagation();
        event.preventDefault();

        WI.showRepresentedObject(resource);
    }

    let linkNode = document.createElement("a");
    linkNode.classList.add("resource-link", className);
    linkNode.title = resource.url;
    linkNode.textContent = (resource.urlComponents.lastPathComponent || resource.url).insertWordBreakCharacters();
    linkNode.addEventListener("click", handleClick);
    return linkNode;
};

WI._undoKeyboardShortcut = function(event)
{
    if (!WI.isEditingAnyField() && !WI.isEventTargetAnEditableField(event)) {
        WI.undo();
        event.preventDefault();
    }
};

WI._redoKeyboardShortcut = function(event)
{
    if (!WI.isEditingAnyField() && !WI.isEventTargetAnEditableField(event)) {
        WI.redo();
        event.preventDefault();
    }
};

WI.undo = function()
{
    DOMAgent.undo();
};

WI.redo = function()
{
    DOMAgent.redo();
};

WI.highlightRangesWithStyleClass = function(element, resultRanges, styleClass, changes)
{
    changes = changes || [];
    var highlightNodes = [];
    var lineText = element.textContent;
    var ownerDocument = element.ownerDocument;
    var textNodeSnapshot = ownerDocument.evaluate(".//text()", element, null, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE, null);

    var snapshotLength = textNodeSnapshot.snapshotLength;
    if (snapshotLength === 0)
        return highlightNodes;

    var nodeRanges = [];
    var rangeEndOffset = 0;
    for (var i = 0; i < snapshotLength; ++i) {
        var range = {};
        range.offset = rangeEndOffset;
        range.length = textNodeSnapshot.snapshotItem(i).textContent.length;
        rangeEndOffset = range.offset + range.length;
        nodeRanges.push(range);
    }

    var startIndex = 0;
    for (var i = 0; i < resultRanges.length; ++i) {
        var startOffset = resultRanges[i].offset;
        var endOffset = startOffset + resultRanges[i].length;

        while (startIndex < snapshotLength && nodeRanges[startIndex].offset + nodeRanges[startIndex].length <= startOffset)
            startIndex++;
        var endIndex = startIndex;
        while (endIndex < snapshotLength && nodeRanges[endIndex].offset + nodeRanges[endIndex].length < endOffset)
            endIndex++;
        if (endIndex === snapshotLength)
            break;

        var highlightNode = ownerDocument.createElement("span");
        highlightNode.className = styleClass;
        highlightNode.textContent = lineText.substring(startOffset, endOffset);

        var lastTextNode = textNodeSnapshot.snapshotItem(endIndex);
        var lastText = lastTextNode.textContent;
        lastTextNode.textContent = lastText.substring(endOffset - nodeRanges[endIndex].offset);
        changes.push({node: lastTextNode, type: "changed", oldText: lastText, newText: lastTextNode.textContent});

        if (startIndex === endIndex) {
            lastTextNode.parentElement.insertBefore(highlightNode, lastTextNode);
            changes.push({node: highlightNode, type: "added", nextSibling: lastTextNode, parent: lastTextNode.parentElement});
            highlightNodes.push(highlightNode);

            var prefixNode = ownerDocument.createTextNode(lastText.substring(0, startOffset - nodeRanges[startIndex].offset));
            lastTextNode.parentElement.insertBefore(prefixNode, highlightNode);
            changes.push({node: prefixNode, type: "added", nextSibling: highlightNode, parent: lastTextNode.parentElement});
        } else {
            var firstTextNode = textNodeSnapshot.snapshotItem(startIndex);
            var firstText = firstTextNode.textContent;
            var anchorElement = firstTextNode.nextSibling;

            firstTextNode.parentElement.insertBefore(highlightNode, anchorElement);
            changes.push({node: highlightNode, type: "added", nextSibling: anchorElement, parent: firstTextNode.parentElement});
            highlightNodes.push(highlightNode);

            firstTextNode.textContent = firstText.substring(0, startOffset - nodeRanges[startIndex].offset);
            changes.push({node: firstTextNode, type: "changed", oldText: firstText, newText: firstTextNode.textContent});

            for (var j = startIndex + 1; j < endIndex; j++) {
                var textNode = textNodeSnapshot.snapshotItem(j);
                var text = textNode.textContent;
                textNode.textContent = "";
                changes.push({node: textNode, type: "changed", oldText: text, newText: textNode.textContent});
            }
        }
        startIndex = endIndex;
        nodeRanges[startIndex].offset = endOffset;
        nodeRanges[startIndex].length = lastTextNode.textContent.length;

    }
    return highlightNodes;
};

WI.revertDOMChanges = function(domChanges)
{
    for (var i = domChanges.length - 1; i >= 0; --i) {
        var entry = domChanges[i];
        switch (entry.type) {
        case "added":
            entry.node.remove();
            break;
        case "changed":
            entry.node.textContent = entry.oldText;
            break;
        }
    }
};

WI.archiveMainFrame = function()
{
    WI._downloadingPage = true;
    WI._updateDownloadToolbarButton();

    PageAgent.archive((error, data) => {
        WI._downloadingPage = false;
        WI._updateDownloadToolbarButton();

        if (error)
            return;

        let mainFrame = WI.networkManager.mainFrame;
        let archiveName = mainFrame.mainResource.urlComponents.host || mainFrame.mainResource.displayName || "Archive";
        let url = WI.FileUtilities.inspectorURLForFilename(archiveName + ".webarchive");

        InspectorFrontendHost.save(url, data, true, true);
    });
};

WI.canArchiveMainFrame = function()
{
    if (WI.sharedApp.debuggableType !== WI.DebuggableType.Web)
        return false;

    if (!WI.networkManager.mainFrame || !WI.networkManager.mainFrame.mainResource)
        return false;

    return WI.Resource.typeFromMIMEType(WI.networkManager.mainFrame.mainResource.mimeType) === WI.Resource.Type.Document;
};

WI.addWindowKeydownListener = function(listener)
{
    if (typeof listener.handleKeydownEvent !== "function")
        return;

    WI._windowKeydownListeners.push(listener);

    WI._updateWindowKeydownListener();
};

WI.removeWindowKeydownListener = function(listener)
{
    WI._windowKeydownListeners.remove(listener);

    WI._updateWindowKeydownListener();
};

WI._updateWindowKeydownListener = function()
{
    if (WI._windowKeydownListeners.length === 1)
        window.addEventListener("keydown", WI._sharedWindowKeydownListener, true);
    else if (!WI._windowKeydownListeners.length)
        window.removeEventListener("keydown", WI._sharedWindowKeydownListener, true);
};

WI._sharedWindowKeydownListener = function(event)
{
    for (var i = WI._windowKeydownListeners.length - 1; i >= 0; --i) {
        if (WI._windowKeydownListeners[i].handleKeydownEvent(event)) {
            event.stopImmediatePropagation();
            event.preventDefault();
            break;
        }
    }
};

WI.reportInternalError = function(errorOrString, details = {})
{
    // The 'details' object includes additional information from the caller as free-form string keys and values.
    // Each key and value will be shown in the uncaught exception reporter, console error message, or in
    // a pre-filled bug report generated for this internal error.

    let error = errorOrString instanceof Error ? errorOrString : new Error(errorOrString);
    error.details = details;

    // The error will be displayed in the Uncaught Exception Reporter sheet if DebugUI is enabled.
    if (WI.isDebugUIEnabled()) {
        // This assert allows us to stop the debugger at an internal exception. It doesn't re-throw
        // exceptions because the original exception would be lost through window.onerror.
        // This workaround can be removed once <https://webkit.org/b/158192> is fixed.
        console.assert(false, "An internal exception was thrown.", error);
        handleInternalException(error);
    } else
        console.error(error);
};

// Many places assume the "main" target has resources.
// In the case where the main backend target is a MultiplexingBackendTarget
// that target has essentially nothing. In that case defer to the page
// target, since that is the real "main" target the frontend is assuming.
Object.defineProperty(WI, "mainTarget",
{
    get() { return WI.pageTarget || WI.backendTarget; }
});

// This list of targets are non-Multiplexing targets.
// So if there is a multiplexing target, and multiple sub-targets
// this is just the list of sub-targets. Almost no code expects
// to actually interact with the Multiplexing target.
Object.defineProperty(WI, "targets",
{
    get() { return WI.targetManager.targets; }
});

// Many places assume the main target because they cannot yet be
// used by reached by Worker debugging. Eventually, once all
// Worker domains have been implemented, all of these must be
// handled properly.
WI.assumingMainTarget = function()
{
    return WI.mainTarget;
};

WI.reset = async function()
{
    await WI.ObjectStore.reset();
    WI.Setting.reset();
    InspectorFrontendHost.reset();
};

WI.isEngineeringBuild = false;

// OpenResourceDialog delegate

WI.dialogWasDismissedWithRepresentedObject = function(dialog, representedObject)
{
    if (!representedObject)
        return;

    WI.showRepresentedObject(representedObject, dialog.cookie, {
        ignoreSearchTab: true,
        ignoreNetworkTab: true,
    });
};

// Popover delegate

WI.didDismissPopover = function(popover)
{
    if (popover === WI._deviceSettingsPopover)
        WI._deviceSettingsPopover = null;
};

WI.DockConfiguration = {
    Right: "right",
    Left: "left",
    Bottom: "bottom",
    Undocked: "undocked",
};
