/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import io.mockk.every
import io.mockk.mockk
import mozilla.components.service.pocket.PocketStory.PocketRecommendedStory
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStory
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStoryCaps
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStoryShim
import mozilla.components.service.pocket.PocketStory.SponsoredContent
import mozilla.components.service.pocket.PocketStory.SponsoredContentCallbacks
import mozilla.components.service.pocket.PocketStory.SponsoredContentFrequencyCaps
import org.junit.Assert
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.TestUtils.getFakeContentRecommendations
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.components.appstate.recommendations.ContentRecommendationsState
import org.mozilla.fenix.components.appstate.recommendations.copyWithRecommendationsState
import org.mozilla.fenix.home.pocket.POCKET_STORIES_DEFAULT_CATEGORY_NAME
import org.mozilla.fenix.home.pocket.PocketRecommendedStoriesCategory
import org.mozilla.fenix.home.pocket.PocketRecommendedStoriesSelectedCategory
import org.mozilla.fenix.home.recentsyncedtabs.RecentSyncedTabState
import org.mozilla.fenix.utils.Settings
import java.util.concurrent.TimeUnit
import kotlin.random.Random

class AppStateTest {
    private val otherStoriesCategory =
        PocketRecommendedStoriesCategory("other", getFakePocketStories(3, "other"))
    private val anotherStoriesCategory =
        PocketRecommendedStoriesCategory("another", getFakePocketStories(3, "another"))
    private val defaultStoriesCategory = PocketRecommendedStoriesCategory(
        POCKET_STORIES_DEFAULT_CATEGORY_NAME,
        getFakePocketStories(3),
    )

    @Test
    fun `GIVEN no category is selected and no sponsored stories are available WHEN getFilteredStories is called THEN only Pocket stories from the default category are returned`() {
        val state = AppState(
            recommendationState = ContentRecommendationsState(
                pocketStoriesCategories = listOf(
                    otherStoriesCategory,
                    anotherStoriesCategory,
                    defaultStoriesCategory,
                ),
            ),
        )

        val result = state.getFilteredStories()

        assertNull(
            result.firstOrNull {
                it is PocketRecommendedStory && it.category != POCKET_STORIES_DEFAULT_CATEGORY_NAME
            },
        )
    }

    @Test
    fun `GIVEN no category is selected and no sponsored stories are available WHEN getFilteredStories is called THEN no more than the default stories number are returned from the default category`() {
        val defaultStoriesCategoryWithManyStories = PocketRecommendedStoriesCategory(
            POCKET_STORIES_DEFAULT_CATEGORY_NAME,
            getFakePocketStories(CONTENT_RECOMMENDATIONS_TO_SHOW_COUNT + 2),
        )
        val state = AppState(
            recommendationState = ContentRecommendationsState(
                pocketStoriesCategories = listOf(
                    otherStoriesCategory,
                    anotherStoriesCategory,
                    defaultStoriesCategoryWithManyStories,
                ),
            ),
        )

        val result = state.getFilteredStories()

        assertEquals(CONTENT_RECOMMENDATIONS_TO_SHOW_COUNT, result.size)
    }

    @Test
    fun `GIVEN no category is selected and 1 sponsored story available WHEN getFilteredStories is called THEN get stories from the default category combined with the sponsored one`() {
        val defaultStoriesCategoryWithManyStories = PocketRecommendedStoriesCategory(
            POCKET_STORIES_DEFAULT_CATEGORY_NAME,
            getFakePocketStories(CONTENT_RECOMMENDATIONS_TO_SHOW_COUNT),
        )
        val sponsoredStories = getFakeSponsoredStories(1)
        val state = AppState(
            recommendationState = ContentRecommendationsState(
                pocketStoriesCategories = listOf(
                    otherStoriesCategory,
                    anotherStoriesCategory,
                    defaultStoriesCategoryWithManyStories,
                ),
                pocketSponsoredStories = sponsoredStories,
            ),
        )

        val result = state.getFilteredStories().toMutableList()

        assertEquals(CONTENT_RECOMMENDATIONS_TO_SHOW_COUNT, result.size)
        assertEquals(sponsoredStories[0], result[1]) // second story should be a sponsored one
        result.removeAt(1) // remove the sponsored story to hopefully only remain with general recommendations
        assertNull(
            result.firstOrNull {
                it is PocketRecommendedStory && it.category != POCKET_STORIES_DEFAULT_CATEGORY_NAME
            },
        )
    }

    @Test
    fun `GIVEN no category is selected and 2 sponsored stories available WHEN getFilteredStories is called THEN get stories from the default category combined with the sponsored stories`() {
        val defaultStoriesCategoryWithManyStories = PocketRecommendedStoriesCategory(
            POCKET_STORIES_DEFAULT_CATEGORY_NAME,
            getFakePocketStories(CONTENT_RECOMMENDATIONS_TO_SHOW_COUNT),
        )
        val sponsoredStories = getFakeSponsoredStories(4)
        val state = AppState(
            recommendationState = ContentRecommendationsState(
                pocketStoriesCategories = listOf(
                    otherStoriesCategory,
                    anotherStoriesCategory,
                    defaultStoriesCategoryWithManyStories,
                ),
                pocketSponsoredStories = sponsoredStories,
            ),
        )

        val result = state.getFilteredStories().toMutableList()

        assertEquals(CONTENT_RECOMMENDATIONS_TO_SHOW_COUNT, result.size)
        // second story should be a sponsored one
        assertEquals(sponsoredStories[1], result[1])
        assertEquals(sponsoredStories[3], result[CONTENT_RECOMMENDATIONS_TO_SHOW_COUNT - 1])
        // remove the sponsored stories to hopefully only remain with general recommendations
        result.removeAt(7)
        result.removeAt(1)
        assertNull(
            result.firstOrNull {
                it is PocketRecommendedStory && it.category != POCKET_STORIES_DEFAULT_CATEGORY_NAME
            },
        )
    }

    @Test
    fun `GIVEN no category is selected and sponsored contents are available WHEN getFilteredStories is called THEN return stories from the default category combined with the sponsored contents`() {
        val defaultStoriesCategoryWithManyStories = PocketRecommendedStoriesCategory(
            POCKET_STORIES_DEFAULT_CATEGORY_NAME,
            getFakePocketStories(CONTENT_RECOMMENDATIONS_TO_SHOW_COUNT),
        )
        val sponsoredContents = getFakeSponsoredContents(4)
        val state = AppState(
            recommendationState = ContentRecommendationsState(
                pocketStoriesCategories = listOf(
                    otherStoriesCategory,
                    anotherStoriesCategory,
                    defaultStoriesCategoryWithManyStories,
                ),
                sponsoredContents = sponsoredContents,
            ),
        )

        var result = state.getFilteredStories(useSponsoredStoriesState = false).toMutableList()

        assertEquals(CONTENT_RECOMMENDATIONS_TO_SHOW_COUNT, result.size)
        assertEquals(sponsoredContents[1], result[1])
        assertEquals(sponsoredContents[3], result[CONTENT_RECOMMENDATIONS_TO_SHOW_COUNT - 1])

        result = result.filterIsInstance<PocketRecommendedStory>().toMutableList()

        assertNull(
            result.firstOrNull {
                it is PocketRecommendedStory && it.category != POCKET_STORIES_DEFAULT_CATEGORY_NAME
            },
        )
    }

    @Test
    fun `GIVEN a list of sponsored stories WHEN filtering them THEN have them ordered by priority`() {
        val stories = getFakeSponsoredStories(4).mapIndexed { index, story ->
            story.copy(priority = index)
        }

        val result = getFilteredSponsoredStories(stories, 10)

        assertEquals(4, result.size)
        assertEquals(stories.reversed(), result)
    }

    @Test
    fun `GIVEN a list of sponsored stories WHEN filtering them THEN drop the ones already shown for the maximum number of times in lifetime`() {
        val stories = getFakeSponsoredStories(4).mapIndexed { index, story ->
            when (index % 2 == 0) {
                true -> story.copy(
                    caps = story.caps.copy(
                        currentImpressions = listOf(1, 2, 3),
                        lifetimeCount = 3,
                    ),
                )
                false -> story
            }
        }

        val result = getFilteredSponsoredStories(stories, 10)

        assertEquals(2, result.size)
        assertEquals(stories[1], result[0])
        assertEquals(stories[3], result[1])
    }

    @Test
    fun `GIVEN a list of sponsored stories WHEN filtering them THEN drop the ones already shown for the maximum number of times in flight`() {
        val stories = getFakeSponsoredStories(4).mapIndexed { index, story ->
            when (index % 2 == 0) {
                true -> story
                false -> story.copy(
                    caps = story.caps.copy(
                        currentImpressions = listOf(
                            TimeUnit.MILLISECONDS.toSeconds(System.currentTimeMillis()),
                            TimeUnit.MILLISECONDS.toSeconds(System.currentTimeMillis()),
                            TimeUnit.MILLISECONDS.toSeconds(System.currentTimeMillis()),
                        ),
                        flightCount = 3,
                    ),
                )
            }
        }

        val result = getFilteredSponsoredStories(stories, 10)

        assertEquals(2, result.size)
        assertEquals(stories[0], result[0])
        assertEquals(stories[2], result[1])
    }

    @Test
    fun `GIVEN a list of sponsored stories WHEN filtering them THEN return up to limit of stories asked`() {
        val stories = getFakeSponsoredStories(4)

        val result = getFilteredSponsoredStories(stories, 2)

        assertEquals(2, result.size)
    }

    @Test
    fun `WHEN filtering the sponsored contents THEN return the list of sponsored contents sorted by descending priority`() {
        val sponsoredContents = getFakeSponsoredContents(4).mapIndexed { index, sponsoredContent ->
            sponsoredContent.copy(priority = index)
        }
        val result = getFilteredSponsoredContents(sponsoredContents, 10)

        assertEquals(4, result.size)
        assertEquals(sponsoredContents.reversed(), result)
    }

    @Test
    fun `WHEN filtering the sponsored contents THEN return the list of sponsored content excluding entries that have reached flight impressions limit`() {
        val sponsoredContents = getFakeSponsoredContents(4).mapIndexed { index, sponsoredContent ->
            when (index % 2 == 0) {
                true -> sponsoredContent
                false -> sponsoredContent.copy(
                    caps = sponsoredContent.caps.copy(
                        currentImpressions = listOf(
                            TimeUnit.MILLISECONDS.toSeconds(System.currentTimeMillis()),
                            TimeUnit.MILLISECONDS.toSeconds(System.currentTimeMillis()),
                            TimeUnit.MILLISECONDS.toSeconds(System.currentTimeMillis()),
                        ),
                        flightCount = 3,
                    ),
                )
            }
        }
        val result = getFilteredSponsoredContents(sponsoredContents, 10)

        assertEquals(2, result.size)
        assertEquals(sponsoredContents[0], result[0])
        assertEquals(sponsoredContents[2], result[1])
    }

    @Test
    fun `GIVEN a limit is specified WHEN filtering the sponsored contents THEN return a list of sponsored contents that does not exceed the limit size`() {
        val sponsoredContents = getFakeSponsoredContents(4)
        val result = getFilteredSponsoredContents(sponsoredContents, 2)

        assertEquals(2, result.size)
    }

    @Test
    fun `GIVEN multiple stories of both types WHEN combining them THEN show sponsored stories at position 2 and 8`() {
        val recommendedStories = getFakePocketStories(CONTENT_RECOMMENDATIONS_TO_SHOW_COUNT, "other")
        val sponsoredStories = getFakeSponsoredStories(4)

        val result = combineRecommendationsAndSponsoredContents(recommendedStories, sponsoredStories)

        assertEquals(CONTENT_RECOMMENDATIONS_TO_SHOW_COUNT, result.size)
        assertEquals(recommendedStories[0], result[0])
        assertEquals(sponsoredStories[0], result[1])
        assertEquals(recommendedStories[1], result[2])
        assertEquals(recommendedStories[2], result[3])
        assertEquals(recommendedStories[3], result[4])
        assertEquals(recommendedStories[4], result[5])
        assertEquals(recommendedStories[5], result[6])
        assertEquals(sponsoredStories[1], result[CONTENT_RECOMMENDATIONS_TO_SHOW_COUNT - 1])
    }

    @Test
    fun `GIVEN content recommendations and sponsored stories WHEN combining them THEN show sponsored stories at position 2 and 9`() {
        val recommendations = getFakeContentRecommendations(CONTENT_RECOMMENDATIONS_TO_SHOW_COUNT)
        val sponsoredStories = getFakeSponsoredStories(4)

        val result = combineRecommendationsAndSponsoredContents(
            recommendations,
            sponsoredStories,
            totalLimit = CONTENT_RECOMMENDATIONS_TO_SHOW_COUNT,
        )

        assertEquals(CONTENT_RECOMMENDATIONS_TO_SHOW_COUNT, result.size)
        assertEquals(recommendations[0], result[0])
        assertEquals(sponsoredStories[0], result[1])
        assertEquals(recommendations[1], result[2])
        assertEquals(recommendations[2], result[3])
        assertEquals(recommendations[3], result[4])
        assertEquals(recommendations[4], result[5])
        assertEquals(recommendations[5], result[6])
        assertEquals(recommendations[6], result[7])
        assertEquals(sponsoredStories[1], result[CONTENT_RECOMMENDATIONS_TO_SHOW_COUNT - 1])
    }

    @Test
    fun `GIVEN a category is selected and 1 sponsored story is available WHEN getFilteredStories is called THEN only stories from that category and the sponsored story are returned`() {
        val sponsoredStories = getFakeSponsoredStories(1)
        val state = AppState(
            recommendationState = ContentRecommendationsState(
                pocketStoriesCategories = listOf(otherStoriesCategory, anotherStoriesCategory, defaultStoriesCategory),
                pocketStoriesCategoriesSelections = listOf(PocketRecommendedStoriesSelectedCategory(otherStoriesCategory.name)),
                pocketSponsoredStories = sponsoredStories,
            ),
        )

        val result = state.getFilteredStories().toMutableList()

        assertEquals(4, result.size)
        assertEquals(sponsoredStories[0], result[1]) // second story should be a sponsored one
        // remove the sponsored story to hopefully only remain with stories from the selected category
        result.removeAt(1)
        assertNull(
            result.firstOrNull {
                it is PocketRecommendedStory && it.category != otherStoriesCategory.name
            },
        )
    }

    @Test
    fun `GIVEN two categories selected and 1 sponsored story available WHEN getFilteredStories is called THEN only stories from the selected categories and the sponsored story are returned`() {
        val sponsoredStories = getFakeSponsoredStories(1)
        val yetAnotherStoriesCategory =
            PocketRecommendedStoriesCategory("yetAnother", getFakePocketStories(3, "yetAnother"))
        val state = AppState(
            recommendationState = ContentRecommendationsState(
                pocketStoriesCategories = listOf(
                    otherStoriesCategory,
                    anotherStoriesCategory,
                    yetAnotherStoriesCategory,
                    defaultStoriesCategory,
                ),
                pocketStoriesCategoriesSelections = listOf(
                    PocketRecommendedStoriesSelectedCategory(otherStoriesCategory.name),
                    PocketRecommendedStoriesSelectedCategory(anotherStoriesCategory.name),
                ),
                pocketSponsoredStories = sponsoredStories,
            ),
        )

        val result = state.getFilteredStories().toMutableList()

        // Only 7 stories available: 3*2 stories from the selected categories plus one sponsored story
        assertEquals(7, result.size)
        assertEquals(sponsoredStories[0], result[1]) // second story should be a sponsored one
        // remove the sponsored story to hopefully only remain with stories from the selected category
        result.removeAt(1)
        assertNull(
            result.firstOrNull {
                (it !is PocketRecommendedStory) ||
                    (it.category != otherStoriesCategory.name && it.category != anotherStoriesCategory.name)
            },
        )
    }

    @Test
    fun `GIVEN two categories selected and 2 sponsored stories available WHEN getFilteredStories is called THEN no more than the default stories number are returned`() {
        val sponsoredStories = getFakeSponsoredStories(2)
        val yetAnotherStoriesCategory =
            PocketRecommendedStoriesCategory("yetAnother", getFakePocketStories(10, "yetAnother"))
        val state = AppState(
            recommendationState = ContentRecommendationsState(
                pocketStoriesCategories = listOf(
                    otherStoriesCategory,
                    anotherStoriesCategory,
                    yetAnotherStoriesCategory,
                    defaultStoriesCategory,
                ),
                pocketStoriesCategoriesSelections = listOf(
                    PocketRecommendedStoriesSelectedCategory(otherStoriesCategory.name),
                    PocketRecommendedStoriesSelectedCategory(yetAnotherStoriesCategory.name),
                ),
                pocketSponsoredStories = sponsoredStories,
            ),
        )

        val result = state.getFilteredStories().toMutableList()

        assertEquals(CONTENT_RECOMMENDATIONS_TO_SHOW_COUNT, result.size)
        // second and penultimate story should be sponsored stories
        assertEquals(sponsoredStories[1], result[1])
        assertEquals(sponsoredStories[0], result[CONTENT_RECOMMENDATIONS_TO_SHOW_COUNT - 1])
        // remove the sponsored stories to hopefully only remain with stories from the selected categories
        result.removeAt(CONTENT_RECOMMENDATIONS_TO_SHOW_COUNT - 1)
        result.removeAt(1)
        assertNull(
            result.firstOrNull {
                (it !is PocketRecommendedStory) ||
                    (it.category != otherStoriesCategory.name && it.category != yetAnotherStoriesCategory.name)
            },
        )
    }

    @Test
    fun `GIVEN a category is selected WHEN getFilteredStories is called THEN no more than the default stories number are returned from the selected category`() {
        val otherStoriesCategoryWithManyStories =
            PocketRecommendedStoriesCategory(
                "other",
                getFakePocketStories(CONTENT_RECOMMENDATIONS_TO_SHOW_COUNT + 2, "other"),
            )
        val state = AppState(
            recommendationState = ContentRecommendationsState(
                pocketStoriesCategories =
                listOf(
                    otherStoriesCategoryWithManyStories,
                    anotherStoriesCategory,
                    defaultStoriesCategory,
                ),
                pocketStoriesCategoriesSelections =
                listOf(PocketRecommendedStoriesSelectedCategory(otherStoriesCategoryWithManyStories.name)),
            ),
        )

        val result = state.getFilteredStories()

        assertEquals(CONTENT_RECOMMENDATIONS_TO_SHOW_COUNT, result.size)
    }

    @Test
    fun `GIVEN two categories are selected WHEN getFilteredStories is called THEN only stories from those categories are returned`() {
        val state = AppState(
            recommendationState = ContentRecommendationsState(
                pocketStoriesCategories = listOf(
                    otherStoriesCategory,
                    anotherStoriesCategory,
                    defaultStoriesCategory,
                ),
                pocketStoriesCategoriesSelections = listOf(
                    PocketRecommendedStoriesSelectedCategory(otherStoriesCategory.name),
                    PocketRecommendedStoriesSelectedCategory(anotherStoriesCategory.name),
                ),
            ),
        )

        val result = state.getFilteredStories()
        assertEquals(6, result.size)
        assertNull(
            result.firstOrNull {
                it is PocketRecommendedStory &&
                    it.category != otherStoriesCategory.name &&
                    it.category != anotherStoriesCategory.name
            },
        )
    }

    @Test
    fun `GIVEN no category is selected WHEN getFilteredStoriesCount is called THEN return an empty result`() {
        val result = getFilteredStoriesCount(emptyList(), 1)

        assertTrue(result.isEmpty())
    }

    @Test
    fun `GIVEN a category is selected WHEN getFilteredStoriesCount is called for at most the stories from this category THEN only stories count only from that category are returned`() {
        var result = getFilteredStoriesCount(listOf(otherStoriesCategory), 2)
        assertEquals(1, result.keys.size)
        assertEquals(otherStoriesCategory.name, result.entries.first().key)
        assertEquals(2, result[otherStoriesCategory.name])

        result = getFilteredStoriesCount(listOf(otherStoriesCategory), 3)
        assertEquals(1, result.keys.size)
        assertEquals(otherStoriesCategory.name, result.entries.first().key)
        assertEquals(3, result[otherStoriesCategory.name])
    }

    @Test
    fun `GIVEN a category is selected WHEN getFilteredStoriesCount is called for more stories than in this category THEN return only that`() {
        val result = getFilteredStoriesCount(listOf(otherStoriesCategory), 4)
        assertEquals(1, result.keys.size)
        assertEquals(otherStoriesCategory.name, result.entries.first().key)
        assertEquals(3, result[otherStoriesCategory.name])
    }

    @Test
    fun `GIVEN two categories are selected WHEN getFilteredStoriesCount is called for at most the stories count in both THEN only stories counts from those categories are returned`() {
        var result = getFilteredStoriesCount(listOf(otherStoriesCategory, anotherStoriesCategory), 2)
        assertEquals(2, result.keys.size)
        assertTrue(
            result.keys.containsAll(
                listOf(
                    otherStoriesCategory.name,
                    anotherStoriesCategory.name,
                ),
            ),
        )
        assertEquals(1, result[otherStoriesCategory.name])
        assertEquals(1, result[anotherStoriesCategory.name])

        result = getFilteredStoriesCount(listOf(otherStoriesCategory, anotherStoriesCategory), 6)
        assertEquals(2, result.keys.size)
        assertTrue(
            result.keys.containsAll(
                listOf(
                    otherStoriesCategory.name,
                    anotherStoriesCategory.name,
                ),
            ),
        )
        assertEquals(3, result[otherStoriesCategory.name])
        assertEquals(3, result[anotherStoriesCategory.name])
    }

    @Test
    fun `GIVEN two categories are selected WHEN getFilteredStoriesCount is called for more results than stories in both THEN only stories counts from those categories are returned`() {
        val result = getFilteredStoriesCount(listOf(otherStoriesCategory, anotherStoriesCategory), 8)
        assertEquals(2, result.keys.size)
        assertTrue(
            result.keys.containsAll(
                listOf(
                    otherStoriesCategory.name,
                    anotherStoriesCategory.name,
                ),
            ),
        )
        assertEquals(3, result[otherStoriesCategory.name])
        assertEquals(3, result[anotherStoriesCategory.name])
    }

    @Test
    fun `GIVEN two categories are selected WHEN getFilteredStoriesCount is called for an odd number of results THEN there are more by one results from first selected category`() {
        val result = getFilteredStoriesCount(listOf(otherStoriesCategory, anotherStoriesCategory), 5)

        assertTrue(
            result.keys.containsAll(
                listOf(
                    otherStoriesCategory.name,
                    anotherStoriesCategory.name,
                ),
            ),
        )
        assertEquals(3, result[otherStoriesCategory.name])
        assertEquals(2, result[anotherStoriesCategory.name])
    }

    @Test
    fun `GIVEN two categories selected with more than needed stories WHEN getFilteredStories is called THEN the results are sorted in the order of least shown`() {
        val firstCategory = PocketRecommendedStoriesCategory(
            "first",
            getFakePocketStories(3, "first"),
        ).run {
            // Avoid the first item also being the oldest to eliminate a potential bug in code
            // that would still get the expected result.
            copy(
                stories = stories.mapIndexed { index, story ->
                    when (index) {
                        0 -> story.copy(timesShown = 333)
                        1 -> story.copy(timesShown = 0)
                        else -> story.copy(timesShown = 345)
                    }
                },
            )
        }
        val secondCategory = PocketRecommendedStoriesCategory(
            "second",
            getFakePocketStories(3, "second"),
        ).run {
            // Avoid the first item also being the oldest to eliminate a potential bug in code
            // that would still get the expected result.
            copy(
                stories = stories.mapIndexed { index, story ->
                    when (index) {
                        0 -> story.copy(timesShown = 222)
                        1 -> story.copy(timesShown = 111)
                        else -> story.copy(timesShown = 11)
                    }
                },
            )
        }

        val state = AppState(
            recommendationState = ContentRecommendationsState(
                pocketStoriesCategories = listOf(firstCategory, secondCategory),
                pocketStoriesCategoriesSelections = listOf(
                    PocketRecommendedStoriesSelectedCategory(
                        firstCategory.name,
                        selectionTimestamp = 0,
                    ),
                    PocketRecommendedStoriesSelectedCategory(
                        secondCategory.name,
                        selectionTimestamp = 222,
                    ),
                ),
            ),
        )

        val result = state.getFilteredStories()

        assertEquals(6, result.size)
        assertSame(secondCategory.stories[2], result.first())
        assertSame(secondCategory.stories[1], result[1])
        assertSame(secondCategory.stories[0], result[2])
        assertSame(firstCategory.stories[1], result[3])
        assertSame(firstCategory.stories[0], result[4])
        assertSame(firstCategory.stories[2], result[5])
    }

    @Test
    fun `GIVEN old selections of categories which do not exist anymore WHEN getFilteredStories is called THEN ignore not found selections`() {
        val state = AppState(
            recommendationState = ContentRecommendationsState(
                pocketStoriesCategories = listOf(
                    otherStoriesCategory,
                    anotherStoriesCategory,
                    defaultStoriesCategory,
                ),
                pocketStoriesCategoriesSelections = listOf(
                    PocketRecommendedStoriesSelectedCategory("unexistent"),
                    PocketRecommendedStoriesSelectedCategory(anotherStoriesCategory.name),
                ),
            ),
        )

        val result = state.getFilteredStories()

        assertEquals(3, result.size)
        assertNull(
            result.firstOrNull {
                it is PocketRecommendedStory && it.category != anotherStoriesCategory.name
            },
        )
    }

    @Test
    fun `GIVEN a content recommendations state update WHEN copying the content recommendations state THEN return the updated state`() {
        val sponsoredStories = getFakeSponsoredStories(1)
        val pocketStoriesCategories = listOf(otherStoriesCategory, anotherStoriesCategory)
        val state = AppState(
            recommendationState = ContentRecommendationsState(
                pocketStoriesCategories = pocketStoriesCategories,
            ),
        )

        assertEquals(0, state.recommendationState.pocketStories.size)
        assertEquals(pocketStoriesCategories, state.recommendationState.pocketStoriesCategories)
        assertEquals(0, state.recommendationState.pocketStoriesCategoriesSelections.size)
        assertEquals(0, state.recommendationState.pocketSponsoredStories.size)

        val newState = state.copyWithRecommendationsState {
            it.copy(pocketSponsoredStories = sponsoredStories)
        }

        assertEquals(0, newState.recommendationState.pocketStories.size)
        assertEquals(pocketStoriesCategories, newState.recommendationState.pocketStoriesCategories)
        assertEquals(0, newState.recommendationState.pocketStoriesCategoriesSelections.size)
        assertEquals(sponsoredStories, newState.recommendationState.pocketSponsoredStories)
    }

    @Test
    fun `GIVEN content recommendations with no sponsored stories WHEN getStories is called THEN return a list of content recommendations to displayed sorted by the number of impressions`() {
        val recommendations = getFakeContentRecommendations(10)
        val state = AppState(
            recommendationState = ContentRecommendationsState(
                contentRecommendations = recommendations.sortedByDescending { it.impressions },
            ),
        )

        val result = state.getStories()

        assertEquals(CONTENT_RECOMMENDATIONS_TO_SHOW_COUNT, result.size)
        assertEquals(recommendations[0], result[0])
        assertEquals(recommendations[1], result[1])
        assertEquals(recommendations[2], result[2])
        assertEquals(recommendations[3], result[3])
        assertEquals(recommendations[4], result[4])
        assertEquals(recommendations[5], result[5])
        assertEquals(recommendations[6], result[6])
        assertEquals(recommendations[7], result[7])
    }

    @Test
    fun `GIVEN content recommendations and sponsored stories WHEN getStories is called THEN return a list of 9 stories with sponsored stories at position 2 and 9`() {
        val recommendations = getFakeContentRecommendations(10)
        val sponsoredStories = getFakeSponsoredStories(4)
        val state = AppState(
            recommendationState = ContentRecommendationsState(
                pocketSponsoredStories = sponsoredStories,
                contentRecommendations = recommendations,
            ),
        )

        val result = state.getStories()

        assertEquals(CONTENT_RECOMMENDATIONS_TO_SHOW_COUNT, result.size)
        assertEquals(recommendations[0], result[0])
        assertEquals(sponsoredStories[1], result[1])
        assertEquals(recommendations[1], result[2])
        assertEquals(recommendations[2], result[3])
        assertEquals(recommendations[3], result[4])
        assertEquals(recommendations[4], result[5])
        assertEquals(recommendations[5], result[6])
        assertEquals(recommendations[6], result[7])
        assertEquals(sponsoredStories[3], result[8])
    }

    @Test
    fun `GIVEN content recommendations and sponsored contents WHEN getStories is called THEN return a list of 9 stories with sponsored contents at position 2 and 9`() {
        val recommendations = getFakeContentRecommendations(10)
        val sponsoredContents = getFakeSponsoredContents(4)
        val state = AppState(
            recommendationState = ContentRecommendationsState(
                contentRecommendations = recommendations,
                sponsoredContents = sponsoredContents,
            ),
        )

        val result = state.getStories(useSponsoredStoriesState = false)

        assertEquals(CONTENT_RECOMMENDATIONS_TO_SHOW_COUNT, result.size)
        assertEquals(recommendations[0], result[0])
        assertEquals(sponsoredContents[1], result[1])
        assertEquals(recommendations[1], result[2])
        assertEquals(recommendations[2], result[3])
        assertEquals(recommendations[3], result[4])
        assertEquals(recommendations[4], result[5])
        assertEquals(recommendations[5], result[6])
        assertEquals(recommendations[6], result[7])
        assertEquals(sponsoredContents[3], result[8])
    }

    @Test
    fun `GIVEN content recommendations and 1 sponsored story WHEN getStories is called THEN return a list of stories with sponsored stories at position 2`() {
        val recommendations = getFakeContentRecommendations(4)
        val sponsoredContents = getFakeSponsoredStories(1)
        val state = AppState(
            recommendationState = ContentRecommendationsState(
                pocketSponsoredStories = sponsoredContents,
                contentRecommendations = recommendations,
            ),
        )

        val result = state.getStories()

        assertEquals(5, result.size)
        assertEquals(recommendations[0], result[0])
        assertEquals(sponsoredContents[0], result[1])
        assertEquals(recommendations[1], result[2])
        assertEquals(recommendations[2], result[3])
        assertEquals(recommendations[3], result[4])
    }

    @Test
    fun `GIVEN content recommendations and 1 sponsored content WHEN getStories is called THEN return a list of stories with sponsored content at position 2`() {
        val recommendations = getFakeContentRecommendations(4)
        val sponsoredContents = getFakeSponsoredContents(1)
        val state = AppState(
            recommendationState = ContentRecommendationsState(
                contentRecommendations = recommendations,
                sponsoredContents = sponsoredContents,
            ),
        )

        val result = state.getStories(useSponsoredStoriesState = false)

        assertEquals(5, result.size)
        assertEquals(recommendations[0], result[0])
        assertEquals(sponsoredContents[0], result[1])
        assertEquals(recommendations[1], result[2])
        assertEquals(recommendations[2], result[3])
        assertEquals(recommendations[3], result[4])
    }

    @Test
    fun `GIVEN content recommendations and 2 sponsored story WHEN getStories is called THEN return a list of stories with sponsored stories at position 2 and 6`() {
        val recommendations = getFakeContentRecommendations(4)
        val sponsoredStories = getFakeSponsoredStories(2)
        val state = AppState(
            recommendationState = ContentRecommendationsState(
                pocketSponsoredStories = sponsoredStories,
                contentRecommendations = recommendations,
            ),
        )

        val result = state.getStories()

        assertEquals(6, result.size)
        assertEquals(recommendations[0], result[0])
        assertEquals(sponsoredStories[1], result[1])
        assertEquals(recommendations[1], result[2])
        assertEquals(recommendations[2], result[3])
        assertEquals(recommendations[3], result[4])
        assertEquals(sponsoredStories[0], result[5])
    }

    @Test
    fun `GIVEN content recommendations and 2 sponsored contents WHEN getStories is called THEN return a list of stories with sponsored contents at position 2 and 6`() {
        val recommendations = getFakeContentRecommendations(4)
        val sponsoredContents = getFakeSponsoredContents(2)
        val state = AppState(
            recommendationState = ContentRecommendationsState(
                contentRecommendations = recommendations,
                sponsoredContents = sponsoredContents,
            ),
        )

        val result = state.getStories(useSponsoredStoriesState = false)

        assertEquals(6, result.size)
        assertEquals(recommendations[0], result[0])
        assertEquals(sponsoredContents[1], result[1])
        assertEquals(recommendations[1], result[2])
        assertEquals(recommendations[2], result[3])
        assertEquals(recommendations[3], result[4])
        assertEquals(sponsoredContents[0], result[5])
    }

    @Test
    fun `GIVEN recent tabs disabled in settings WHEN checking to show tabs THEN section should not be shown`() {
        val settings = mockk<Settings> {
            every { showRecentTabsFeature } returns false
        }

        val state = AppState()

        Assert.assertFalse(state.shouldShowRecentTabs(settings))
    }

    @Test
    fun `GIVEN only local tabs WHEN checking to show tabs THEN section should be shown`() {
        val settings = mockk<Settings> {
            every { showRecentTabsFeature } returns true
        }

        val state = AppState(recentTabs = listOf(mockk()))

        assertTrue(state.shouldShowRecentTabs(settings))
    }

    @Test
    fun `GIVEN only remote tabs WHEN checking to show tabs THEN section should be shown`() {
        val settings = mockk<Settings> {
            every { showRecentTabsFeature } returns true
        }

        val state = AppState(recentSyncedTabState = RecentSyncedTabState.Success(mockk()))

        assertTrue(state.shouldShowRecentTabs(settings))
    }

    @Test
    fun `GIVEN local and remote tabs WHEN checking to show tabs THEN section should be shown`() {
        val settings = mockk<Settings> {
            every { showRecentTabsFeature } returns true
        }

        val state = AppState(
            recentTabs = listOf(mockk()),
            recentSyncedTabState = RecentSyncedTabState.Success(mockk()),
        )

        assertTrue(state.shouldShowRecentTabs(settings))
    }
}

private fun getFakePocketStories(
    limit: Int = 1,
    category: String = POCKET_STORIES_DEFAULT_CATEGORY_NAME,
): List<PocketRecommendedStory> {
    return mutableListOf<PocketRecommendedStory>().apply {
        for (index in 0 until limit) {
            val randomNumber = Random.nextInt(0, 10)

            add(
                PocketRecommendedStory(
                    title = "This is a ${"very ".repeat(randomNumber)} long title",
                    publisher = "Publisher",
                    url = "https://story$randomNumber.com",
                    imageUrl = "",
                    timeToRead = randomNumber,
                    category = category,
                    timesShown = index.toLong(),
                ),
            )
        }
    }
}

private fun getFakeSponsoredStories(limit: Int) = mutableListOf<PocketSponsoredStory>().apply {
    for (index in 0 until limit) {
        add(
            PocketSponsoredStory(
                id = index,
                title = "Story title $index",
                url = "https://sponsored.story",
                imageUrl = "https://sponsored.image",
                sponsor = "Sponsor $index",
                shim = PocketSponsoredStoryShim(
                    click = "Story title $index click shim",
                    impression = "Story title $index impression shim",
                ),
                priority = 2 + index % 2,
                caps = PocketSponsoredStoryCaps(
                    lifetimeCount = 1 + index * 5,
                    flightCount = 1 + index * 2,
                    flightPeriod = 1 + index * 3,
                ),
            ),
        )
    }
}

private fun getFakeSponsoredContents(limit: Int) = mutableListOf<SponsoredContent>().apply {
    for (index in 0 until limit) {
        add(
            SponsoredContent(
                url = "https://sponsored.story",
                title = "Story title $index",
                callbacks = SponsoredContentCallbacks(
                    clickUrl = "https://mozilla.com/click$index",
                    impressionUrl = "https://mozilla.com/impression$index",
                ),
                imageUrl = "https://sponsored.image",
                domain = "Domain $index",
                excerpt = "Excerpt $index",
                sponsor = "Sponsor $index",
                blockKey = "Block key $index",
                caps = SponsoredContentFrequencyCaps(
                    flightCount = 1 + index * 2,
                    flightPeriod = 1 + index * 3,
                ),
                priority = 2 + index % 2,
            ),
        )
    }
}
