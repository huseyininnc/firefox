/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  BackgroundUpdate: "resource://gre/modules/BackgroundUpdate.sys.mjs",
  MigrationUtils: "resource:///modules/MigrationUtils.sys.mjs",
  PermissionTestUtils: "resource://testing-common/PermissionTestUtils.sys.mjs",
  WindowsLaunchOnLogin: "resource://gre/modules/WindowsLaunchOnLogin.sys.mjs",
});

const { MockRegistry } = ChromeUtils.importESModule(
  "resource://testing-common/MockRegistry.sys.mjs"
);

let profileService = Cc["@mozilla.org/toolkit/profile-service;1"].getService(
  Ci.nsIToolkitProfileService
);
let startWithLastProfileOriginal = profileService.startWithLastProfile;
let registry = null;
add_setup(() => {
  registry = new MockRegistry();
  registry.setValue(
    Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
    "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
    "",
    ""
  );
  registry.setValue(
    Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
    "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run",
    "",
    ""
  );
  registerCleanupFunction(() => {
    registry.shutdown();
  });
});

add_task(async function test_check_uncheck_checkbox() {
  await ExperimentAPI.ready();
  let doCleanup = await NimbusTestUtils.enrollWithFeatureConfig({
    featureId: "windowsLaunchOnLogin",
    value: { enabled: true },
  });
  await WindowsLaunchOnLogin.withLaunchOnLoginRegistryKey(async wrk => {
    // Open preferences to general pane
    await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
      leaveOpen: true,
    });
    let doc = gBrowser.contentDocument;

    let launchOnLoginCheckbox = doc.getElementById("windowsLaunchOnLogin");
    launchOnLoginCheckbox.click();
    ok(launchOnLoginCheckbox.checked, "Autostart checkbox checked");

    ok(
      wrk.hasValue(WindowsLaunchOnLogin.getLaunchOnLoginRegistryName()),
      "Key exists"
    );

    launchOnLoginCheckbox.click();
    ok(!launchOnLoginCheckbox.checked, "Autostart checkbox unchecked");

    await TestUtils.waitForCondition(
      () => !wrk.hasValue(WindowsLaunchOnLogin.getLaunchOnLoginRegistryName()),
      "Waiting for Autostart registry key to be removed",
      undefined,
      200
    );

    gBrowser.removeCurrentTab();
  });
  await doCleanup();
});

add_task(async function create_external_regkey() {
  if (Services.sysinfo.getProperty("hasWinPackageId")) {
    return;
  }
  await ExperimentAPI.ready();
  let doCleanup = await NimbusTestUtils.enrollWithFeatureConfig({
    featureId: "windowsLaunchOnLogin",
    value: { enabled: true },
  });
  await WindowsLaunchOnLogin.withLaunchOnLoginRegistryKey(async wrk => {
    // Delete any existing entries before testing
    // Both functions are install specific so it's safe to run them
    // like this.
    wrk.removeValue(WindowsLaunchOnLogin.getLaunchOnLoginRegistryName());
    await WindowsLaunchOnLogin._removeLaunchOnLoginShortcuts();
    // Create registry key without using settings pane to check if
    // this is reflected in the settings
    let autostartPath =
      WindowsLaunchOnLogin.quoteString(
        Services.dirsvc.get("XREExeF", Ci.nsIFile).path
      ) + " -os-autostart";
    wrk.writeStringValue(
      WindowsLaunchOnLogin.getLaunchOnLoginRegistryName(),
      autostartPath
    );

    // Open preferences to general pane
    await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
      leaveOpen: true,
    });
    let doc = gBrowser.contentDocument;

    let launchOnLoginCheckbox = doc.getElementById("windowsLaunchOnLogin");
    ok(
      launchOnLoginCheckbox.checked,
      "Autostart checkbox automatically checked"
    );

    gBrowser.removeCurrentTab();
  });
  await doCleanup();
});

add_task(async function testRemoveLaunchOnLoginGuard() {
  let registryName = WindowsLaunchOnLogin.getLaunchOnLoginRegistryName();
  // Simulate launch on login disabled from Windows settings
  registry.setValue(
    Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
    "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run",
    registryName,
    0b1,
    Ci.nsIWindowsRegKey.TYPE_BINARY
  );
  // This function should now be non-op
  WindowsLaunchOnLogin.removeLaunchOnLogin();
  await WindowsLaunchOnLogin.withLaunchOnLoginRegistryKey(async wrk => {
    ok(wrk.hasValue(registryName), "Registry value is not deleted");
  });
});

add_task(async function delete_external_regkey() {
  if (Services.sysinfo.getProperty("hasWinPackageId")) {
    return;
  }
  await ExperimentAPI.ready();
  let doCleanup = await NimbusTestUtils.enrollWithFeatureConfig({
    featureId: "windowsLaunchOnLogin",
    value: { enabled: true },
  });
  await WindowsLaunchOnLogin.withLaunchOnLoginRegistryKey(async wrk => {
    // Delete registry key without using settings pane to check if
    // this is reflected in the settings
    wrk.removeValue(WindowsLaunchOnLogin.getLaunchOnLoginRegistryName());

    // Open preferences to general pane
    await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
      leaveOpen: true,
    });
    let doc = gBrowser.contentDocument;

    let launchOnLoginCheckbox = doc.getElementById("windowsLaunchOnLogin");
    ok(
      !launchOnLoginCheckbox.checked,
      "Launch on login checkbox automatically unchecked"
    );

    gBrowser.removeCurrentTab();
  });
  await doCleanup();
});

add_task(async function testDisablingLaunchOnLogin() {
  Cc["@mozilla.org/toolkit/profile-service;1"].getService(
    Ci.nsIToolkitProfileService
  ).startWithLastProfile = false;

  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });
  let doc = gBrowser.contentDocument;

  let launchOnLoginCheckbox = doc.getElementById("windowsLaunchOnLogin");
  ok(launchOnLoginCheckbox.disabled, "Autostart checkbox disabled");
  ok(!launchOnLoginCheckbox.checked, "Autostart checkbox unchecked");

  let launchOnLoginDisabledMessage = doc.getElementById(
    "windowsLaunchOnLoginDisabledProfileBox"
  );
  ok(!launchOnLoginDisabledMessage.hidden, "Disabled message is displayed");

  gBrowser.removeCurrentTab();
});

registerCleanupFunction(async function () {
  await WindowsLaunchOnLogin._removeLaunchOnLoginShortcuts();
  await WindowsLaunchOnLogin.withLaunchOnLoginRegistryKey(async wrk => {
    let registryName = WindowsLaunchOnLogin.getLaunchOnLoginRegistryName();
    if (wrk.hasValue(registryName)) {
      wrk.removeValue(registryName);
    }
  });
  profileService.startWithLastProfile = startWithLastProfileOriginal;
});
