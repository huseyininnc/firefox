import pytest

URL = "https://whooshmotorsports.com/collections/2013-ford-focus-st/products/copy-of-whoosh-motorsports-performance-coil-packs-2014-2019-focus-st-2-0l"
SELECT_CSS = "select#productSelect-option-0"


async def is_fastclick_active(client):
    async with client.ensure_fastclick_activates():
        await client.navigate(URL)
        return client.test_for_fastclick(client.await_css(SELECT_CSS))


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await is_fastclick_active(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await is_fastclick_active(client)
