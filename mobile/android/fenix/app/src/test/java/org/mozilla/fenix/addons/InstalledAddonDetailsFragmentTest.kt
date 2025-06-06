/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.view.LayoutInflater
import android.view.View
import android.widget.TextView
import androidx.core.view.isVisible
import androidx.navigation.NavController
import androidx.navigation.Navigation
import com.google.android.material.switchmaterial.SwitchMaterial
import io.mockk.Runs
import io.mockk.coEvery
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.slot
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.Dispatchers
import mozilla.components.concept.engine.webextension.EnableSource
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.AddonManager
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.databinding.FragmentInstalledAddOnDetailsBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.settings.SupportUtils
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class InstalledAddonDetailsFragmentTest {
    @get:Rule
    val coroutineRule = MainCoroutineRule()
    private lateinit var fragment: InstalledAddonDetailsFragment
    private val addonManager = mockk<AddonManager>()

    @Before
    fun setup() {
        fragment = spyk(InstalledAddonDetailsFragment())
    }

    @Test
    fun `GIVEN add-on is supported and not disabled WHEN enabling it THEN the add-on is requested by the user`() {
        val addon = mockk<Addon>()
        every { addon.isDisabledAsUnsupported() } returns false
        every { addon.isSupported() } returns true
        every { fragment.addon } returns addon
        every { addonManager.enableAddon(any(), any(), any(), any()) } just Runs

        fragment.enableAddon(addonManager, {}, {})

        verify { addonManager.enableAddon(addon, EnableSource.USER, any(), any()) }
    }

    @Test
    fun `GIVEN add-on is supported and disabled as previously unsupported WHEN enabling it THEN the add-on is requested by both the app and the user`() {
        val addon = mockk<Addon>()
        every { addon.isDisabledAsUnsupported() } returns true
        every { addon.isSupported() } returns true
        every { fragment.addon } returns addon
        val capturedAddon = slot<Addon>()
        val capturedOnSuccess = slot<(Addon) -> Unit>()
        every {
            addonManager.enableAddon(
                capture(capturedAddon),
                any(),
                capture(capturedOnSuccess),
                any(),
            )
        } answers { capturedOnSuccess.captured.invoke(capturedAddon.captured) }

        fragment.enableAddon(addonManager, {}, {})

        verify { addonManager.enableAddon(addon, EnableSource.APP_SUPPORT, any(), any()) }
        verify { addonManager.enableAddon(capturedAddon.captured, EnableSource.USER, any(), any()) }
    }

    @Test
    fun `GIVEN blocklisted addon WHEN binding the enable switch THEN disable the switch`() {
        val addon = mockk<Addon>()
        val enableSwitch = mockk<SwitchMaterial>(relaxed = true)
        val privateBrowsingSwitch = mockk<SwitchMaterial>(relaxed = true)

        every { fragment.provideEnableSwitch() } returns enableSwitch
        every { fragment.providePrivateBrowsingSwitch() } returns privateBrowsingSwitch
        every { addon.isEnabled() } returns true
        every { addon.isDisabledAsBlocklisted() } returns true
        every { fragment.addon } returns addon

        fragment.bindEnableSwitch()

        verify { enableSwitch.isEnabled = false }
    }

    @Test
    fun `GIVEN enabled addon WHEN binding the enable switch THEN do not disable the switch`() {
        val addon = mockk<Addon>()
        val enableSwitch = mockk<SwitchMaterial>(relaxed = true)
        val privateBrowsingSwitch = mockk<SwitchMaterial>(relaxed = true)

        every { fragment.provideEnableSwitch() } returns enableSwitch
        every { fragment.providePrivateBrowsingSwitch() } returns privateBrowsingSwitch
        every { addon.isDisabledAsBlocklisted() } returns false
        every { addon.isDisabledAsNotCorrectlySigned() } returns false
        every { addon.isDisabledAsIncompatible() } returns false
        every { addon.isEnabled() } returns true
        every { fragment.addon } returns addon

        fragment.bindEnableSwitch()

        verify(exactly = 0) { enableSwitch.isEnabled = false }
    }

    @Test
    fun `GIVEN addon not correctly signed WHEN binding the enable switch THEN disable the switch`() {
        val addon = mockk<Addon>()
        val enableSwitch = mockk<SwitchMaterial>(relaxed = true)
        val privateBrowsingSwitch = mockk<SwitchMaterial>(relaxed = true)

        every { fragment.provideEnableSwitch() } returns enableSwitch
        every { fragment.providePrivateBrowsingSwitch() } returns privateBrowsingSwitch
        every { addon.isEnabled() } returns true
        every { addon.isDisabledAsBlocklisted() } returns false
        every { addon.isDisabledAsNotCorrectlySigned() } returns true
        every { fragment.addon } returns addon

        fragment.bindEnableSwitch()

        verify { enableSwitch.isEnabled = false }
    }

    @Test
    fun `GIVEN incompatible addon WHEN binding the enable switch THEN disable the switch`() {
        val addon = mockk<Addon>()
        val enableSwitch = mockk<SwitchMaterial>(relaxed = true)
        val privateBrowsingSwitch = mockk<SwitchMaterial>(relaxed = true)

        every { fragment.provideEnableSwitch() } returns enableSwitch
        every { fragment.providePrivateBrowsingSwitch() } returns privateBrowsingSwitch
        every { addon.isEnabled() } returns true
        every { addon.isDisabledAsBlocklisted() } returns false
        every { addon.isDisabledAsNotCorrectlySigned() } returns false
        every { addon.isDisabledAsIncompatible() } returns true
        every { fragment.addon } returns addon

        fragment.bindEnableSwitch()

        verify { enableSwitch.isEnabled = false }
    }

    @Test
    fun `GIVEN an add-on WHEN clicking the report button THEN a new tab is open`() {
        val addon = mockAddon()
        every { fragment.addon } returns addon
        every { fragment.activity } returns mockk<HomeActivity>(relaxed = true)
        val useCases = mockk<TabsUseCases>()
        val selectOrAddTab = mockk<TabsUseCases.SelectOrAddUseCase>()
        every { selectOrAddTab.invoke(any(), any(), any(), any(), any()) } returns "some-tab-id"
        every { useCases.selectOrAddTab } returns selectOrAddTab
        every { testContext.components.useCases.tabsUseCases } returns useCases
        // We create the `binding` instance and bind the UI here because `onCreateView()` checks a late init variable
        // and we cannot easily mock it to skip the check.
        fragment.setBindingAndBindUI(
            FragmentInstalledAddOnDetailsBinding.inflate(
                LayoutInflater.from(testContext),
                mockk(relaxed = true),
                false,
            ),
        )
        val navController = mockk<NavController>(relaxed = true)
        Navigation.setViewNavController(fragment.binding.root, navController)

        // Click the report button.
        fragment.binding.reportAddOn.performClick()

        verify {
            selectOrAddTab.invoke(
                url = "https://addons.mozilla.org/android/feedback/addon/some-addon-id/",
                private = false,
                ignoreFragment = true,
            )
        }
        verify {
            navController.navigate(
                InstalledAddonDetailsFragmentDirections.actionGlobalBrowser(null),
            )
        }
    }

    @Test
    fun `GIVEN an add-on and private browsing mode is used WHEN clicking the report button THEN a new private tab is open`() {
        val addon = mockAddon()
        every { fragment.addon } returns addon
        val homeActivity = mockk<HomeActivity>(relaxed = true)
        every { homeActivity.browsingModeManager.mode.isPrivate } returns true
        every { fragment.activity } returns homeActivity
        val useCases = mockk<TabsUseCases>()
        val selectOrAddTab = mockk<TabsUseCases.SelectOrAddUseCase>()
        every { selectOrAddTab.invoke(any(), any(), any(), any(), any()) } returns "some-tab-id"
        every { useCases.selectOrAddTab } returns selectOrAddTab
        every { testContext.components.useCases.tabsUseCases } returns useCases
        // We create the `binding` instance and bind the UI here because `onCreateView()` checks a late init variable
        // and we cannot easily mock it to skip the check.
        fragment.setBindingAndBindUI(
            FragmentInstalledAddOnDetailsBinding.inflate(
                LayoutInflater.from(testContext),
                mockk(relaxed = true),
                false,
            ),
        )
        val navController = mockk<NavController>(relaxed = true)
        Navigation.setViewNavController(fragment.binding.root, navController)

        // Click the report button.
        fragment.binding.reportAddOn.performClick()

        verify {
            selectOrAddTab.invoke(
                url = "https://addons.mozilla.org/android/feedback/addon/some-addon-id/",
                private = true,
                ignoreFragment = true,
            )
        }
        verify {
            navController.navigate(
                InstalledAddonDetailsFragmentDirections.actionGlobalBrowser(null),
            )
        }
    }

    @Test
    fun `GIVEN addon does not allow private browsing WHEN binding THEN update switch`() {
        val addon = mockAddon()
        val privateBrowsingSwitch = mockk<SwitchMaterial>(relaxed = true)

        every { fragment.providePrivateBrowsingSwitch() } returns privateBrowsingSwitch
        every { addon.incognito } returns Addon.Incognito.NOT_ALLOWED
        every { fragment.addon } returns addon
        every { fragment.context } returns testContext

        fragment.bindAllowInPrivateBrowsingSwitch()

        verify { privateBrowsingSwitch.isEnabled = false }
        verify { privateBrowsingSwitch.isChecked = false }
        verify { privateBrowsingSwitch.text = "Not allowed in private windows" }
    }

    @Test
    fun `GIVEN a not found addon WHEN binding THEN show an error message`() = runTestOnMain {
        val addon = mockAddon()

        coEvery { addonManager.getAddonByID(any()) } returns null
        every { fragment.activity } returns mockk<HomeActivity>(relaxed = true)
        every { addon.isInstalled() } returns true
        every { fragment.provideAddonManager() } returns addonManager
        every { fragment.addon } returns addon
        every { fragment.context } returns testContext
        every { fragment.bindUI() } just Runs
        every { fragment.showUnableToQueryAddonsMessage() } just Runs
        every { testContext.components.analytics.crashReporter } returns mockk(relaxed = true)

        fragment._binding =
            FragmentInstalledAddOnDetailsBinding.inflate(
                LayoutInflater.from(testContext),
                mockk(relaxed = true),
                false,
            )

        fragment.bindAddon(Dispatchers.Main)

        verify { fragment.showUnableToQueryAddonsMessage() }
        verify(exactly = 0) { fragment.bindUI() }
    }

    @Test
    fun `GIVEN an addon WHEN binding THEN bind the UI`() = runTestOnMain {
        val addon = mockAddon()

        coEvery { addonManager.getAddonByID(any()) } returns addon
        every { fragment.activity } returns mockk<HomeActivity>(relaxed = true)
        every { addon.isInstalled() } returns true
        every { fragment.provideAddonManager() } returns addonManager
        every { fragment.addon } returns addon
        every { fragment.context } returns testContext
        every { fragment.bindUI() } just Runs
        every { fragment.showUnableToQueryAddonsMessage() } just Runs

        fragment._binding =
            FragmentInstalledAddOnDetailsBinding.inflate(
                LayoutInflater.from(testContext),
                mockk(relaxed = true),
                false,
            )

        fragment.bindAddon(Dispatchers.Main)

        verify { fragment.bindUI() }
        verify(exactly = 0) { fragment.showUnableToQueryAddonsMessage() }
    }

    @Test
    fun `GIVEN a hard blocked add-on WHEN binding THEN an error message is shown`() {
        val addon = mockAddon()
        every { addon.isDisabledAsBlocklisted() } returns true
        every { fragment.addon } returns addon
        every { fragment.context } returns testContext
        every {
            (fragment.activity as HomeActivity).openToBrowserAndLoad(
                searchTermOrURL = any(),
                newTab = any(),
                from = any(),
            )
        } returns Unit
        // We create the `binding` instance and bind the UI here because `onCreateView()` checks a late init variable
        // and we cannot easily mock it to skip the check.
        val binding = FragmentInstalledAddOnDetailsBinding.inflate(
            LayoutInflater.from(testContext),
            mockk(relaxed = true),
            false,
        )
        fragment.setBindingAndBindUI(binding)

        val warningView =
            binding.root.findViewById<View>(mozilla.components.feature.addons.R.id.add_on_messagebar_warning)
        assertFalse(warningView.isVisible)
        val errorView = binding.root.findViewById<View>(mozilla.components.feature.addons.R.id.add_on_messagebar_error)
        assertTrue(errorView.isVisible)

        errorView.findViewById<TextView>(mozilla.components.feature.addons.R.id.add_on_messagebar_error_learn_more_link)
            .performClick()

        verify {
            (fragment.activity as HomeActivity).openToBrowserAndLoad(
                searchTermOrURL = "${BuildConfig.AMO_BASE_URL}/android/blocked-addon/some-addon-id/1.2.3/",
                newTab = true,
                from = BrowserDirection.FromAddonDetailsFragment,
            )
        }
    }

    @Test
    fun `GIVEN an add-on not correctly signed WHEN binding THEN an error message is shown`() {
        val addon = mockAddon()
        every { addon.isDisabledAsNotCorrectlySigned() } returns true
        every { fragment.addon } returns addon
        every { fragment.context } returns testContext
        every {
            (fragment.activity as HomeActivity).openToBrowserAndLoad(
                searchTermOrURL = any(),
                newTab = any(),
                from = any(),
            )
        } returns Unit
        every { (fragment.activity as HomeActivity).baseContext } returns testContext
        // We create the `binding` instance and bind the UI here because `onCreateView()` checks a late init variable
        // and we cannot easily mock it to skip the check.
        val binding = FragmentInstalledAddOnDetailsBinding.inflate(
            LayoutInflater.from(testContext),
            mockk(relaxed = true),
            false,
        )
        fragment.setBindingAndBindUI(binding)

        val warningView =
            binding.root.findViewById<View>(mozilla.components.feature.addons.R.id.add_on_messagebar_warning)
        assertFalse(warningView.isVisible)
        val errorView = binding.root.findViewById<View>(mozilla.components.feature.addons.R.id.add_on_messagebar_error)
        assertTrue(errorView.isVisible)

        errorView.findViewById<TextView>(mozilla.components.feature.addons.R.id.add_on_messagebar_error_learn_more_link)
            .performClick()

        verify {
            (fragment.activity as HomeActivity).openToBrowserAndLoad(
                searchTermOrURL = SupportUtils.getSumoURLForTopic(testContext, SupportUtils.SumoTopic.UNSIGNED_ADDONS),
                newTab = true,
                from = BrowserDirection.FromAddonDetailsFragment,
            )
        }
    }

    @Test
    fun `GIVEN a soft-blocked add-on WHEN binding THEN a warning message is shown`() {
        val addon = mockAddon()
        every { addon.isSoftBlocked() } returns true
        every { fragment.addon } returns addon
        every { fragment.context } returns testContext
        every {
            (fragment.activity as HomeActivity).openToBrowserAndLoad(
                searchTermOrURL = any(),
                newTab = any(),
                from = any(),
            )
        } returns Unit

        // We create the `binding` instance and bind the UI here because `onCreateView()` checks a late init variable
        // and we cannot easily mock it to skip the check.
        val binding = FragmentInstalledAddOnDetailsBinding.inflate(
            LayoutInflater.from(testContext),
            mockk(relaxed = true),
            false,
        )
        fragment.setBindingAndBindUI(binding)

        val warningView = binding.root.findViewById<View>(mozilla.components.feature.addons.R.id.add_on_messagebar_warning)
        assertTrue(warningView.isVisible)
        val errorView = binding.root.findViewById<View>(mozilla.components.feature.addons.R.id.add_on_messagebar_error)
        assertFalse(errorView.isVisible)

        warningView.findViewById<TextView>(mozilla.components.feature.addons.R.id.add_on_messagebar_warning_learn_more_link)
            .performClick()

        verify {
            (fragment.activity as HomeActivity).openToBrowserAndLoad(
                searchTermOrURL = "${BuildConfig.AMO_BASE_URL}/android/blocked-addon/some-addon-id/1.2.3/",
                newTab = true,
                from = BrowserDirection.FromAddonDetailsFragment,
            )
        }
    }

    private fun mockAddon(): Addon {
        val addon: Addon = mockk()
        every { addon.id } returns "some-addon-id"
        every { addon.version } returns "1.2.3"
        every { addon.incognito } returns Addon.Incognito.SPANNING
        every { addon.isEnabled() } returns true
        every { addon.isDisabledAsBlocklisted() } returns false
        every { addon.isDisabledAsNotCorrectlySigned() } returns false
        every { addon.isDisabledAsIncompatible() } returns false
        every { addon.installedState } returns null
        every { addon.isAllowedInPrivateBrowsing() } returns false
        every { addon.translatableName } returns mapOf("en-US" to "some-name")
        every { addon.defaultLocale } returns "en-US"
        return addon
    }
}
