/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "osos-2.0.5.h"
#include "rv_api.h"
#include "rv_media.h"

/* Apple's row layouts hold pointers; the host's are wider, and the host
 * compiler knows no ARM attribute. */
#define _Static_assert(...)
#undef RV_ARM
#define RV_ARM
#include "../fields.h"
#include "../browser.c"

/* The host doubles as the core: counted blocks from alloc and free. */
static unsigned live_blocks;
static int fail_alloc;

static void *host_alloc(rv_extension_handle s, uint32_t bytes)
{
    (void)s;
    if (fail_alloc && ++fail_alloc == 4)
        return NULL;
    void *block = calloc(1, bytes);
    assert(block);
    live_blocks++;
    return block;
}

static void host_free(rv_extension_handle s, void *block)
{
    (void)s;
    assert(live_blocks);
    live_blocks--;
    free(block);
}
static const struct rv_api host_api = {.alloc = host_alloc, .free = host_free};
const struct rv_api *api = &host_api;
rv_extension_handle self;

/* One owned browser over a TrackData, a list of six entries and their records. */
static _Alignas(16) uint8_t browser[MediaBrowser_track_data + 8];
static _Alignas(16) uint8_t owner[MediaBrowser_track_data + 8];
static _Alignas(16) uint8_t track_data[TrackData_artist_cache + 0x58];
static _Alignas(16) uint8_t list[MediaList_jump_index + 0x40 * 8];
static _Alignas(16) uint8_t root[MediaEntry_size];
static uint8_t *native_order; /* a MediaOrder, or NULL */
static uint32_t *native_jump;
static unsigned pins;

static _Alignas(16) uint8_t records[6][TrackData_dbid + 8];

/* What the fields table and the filter answer for each record. */
static struct tags {
    uint32_t album_artist, sort_artist, sort_album_artist;
    int accepted;
} tags[6];

static struct tags *tags_of(const void *record)
{
    return &tags[((const uint8_t *)record - records[0]) / sizeof records[0]];
}

static _Alignas(16) uint8_t entries[6][MediaEntry_size];
static const uint16_t *strings[] = {
    u"",     u"The Clash", u"The Cure", u"Alpha",     u"Guest",   u"Clash",
    u"Cure", u"Zulu",      u"Aardvark", u"The Zebra", u"Theatre", u"An Artist",
};

/* The fields table, as the test sets it per record. */
uint32_t fields_token(const void *r)
{
    return tags_of(r)->album_artist;
}

uint32_t fields_sort_token(const void *r, int album_artist)
{
    return album_artist ? tags_of(r)->sort_album_artist : tags_of(r)->sort_artist;
}

void *MediaBrowser_GetOwner(void *b)
{
    assert(b == browser);
    return owner;
}

struct MediaList *MediaBrowser_GetCurrentList(void *b)
{
    assert(b == browser);
    return (struct MediaList *)list;
}

int MediaBrowser_EvaluateEntryFilters(void *b, struct MediaEntry *entry,
                                      struct MediaList *override)
{
    assert(b == browser && !override);
    return tags_of(RV_FIELD(entry, MediaEntry_song_record, void *))->accepted;
}

uint32_t MediaBrowser_GetRecordArtworkId(void *o, void *record)
{
    assert(o == owner);
    return RV_FIELD(record, MediaRecord_album, uint32_t) == 2 ? 0x200 : 0;
}

static unsigned clears;

void MediaBrowser_ClearArtists(void *b)
{
    assert(b == browser && !RV_FIELD(b, MediaBrowser_artists, void *) &&
           !RV_FIELD(b, MediaBrowser_artist_tokens, void *));
    clears++;
}

void MediaBrowser_ClearAlbums(void *b)
{
    assert(b == browser);
}

void MediaBrowser_ClearSongs(void *b)
{
    assert(b == browser);
}

int MediaBrowser_InvalidateEntryMatches(void *b)
{
    assert(b == browser);
    return 0;
}

void Heap_Free(void *block)
{
    assert(!block);
}

struct MediaEntry *MediaList_GetEntryByOrderIndex(struct MediaList *l, int mode,
                                                  uint32_t reverse, uint32_t index)
{
    assert((void *)l == list && mode == MediaOrderMode_Album && !reverse && index < 6);
    return (struct MediaEntry *)entries[index];
}

int StringCache_GetTokenView(void *cache, uint32_t token, void **data, uint32_t *bytes)
{
    assert(cache == track_data + TrackData_artist_cache && pins);
    assert(token < sizeof strings / sizeof *strings);
    *data = (void *)strings[token];
    *bytes = 0;
    while (strings[token][*bytes / 2])
        *bytes += 2;
    return 0;
}

void TrackData_AcquireStringCaches(void *t)
{
    assert(t == track_data);
    pins++;
}

void TrackData_ReleaseStringCaches(void *t)
{
    assert(t == track_data && pins);
    pins--;
}

/* Apple's compare: case-insensitive after leading blanks, as far as the
 * grouping needs. */
int SongText_CompareUtf16(void *collation, const uint16_t *a, uint32_t na,
                          const uint16_t *b, uint32_t nb)
{
    assert(!collation);
    while (na && *a <= ' ') {
        a++;
        na--;
    }
    while (nb && *b <= ' ') {
        b++;
        nb--;
    }
    for (uint32_t i = 0; i < na && i < nb; i++) {
        uint16_t x = a[i] | 32, y = b[i] | 32;
        if (x != y)
            return x < y ? -1 : 1;
    }
    return (na > nb) - (na < nb);
}

static struct {
    uint32_t units;
    uint16_t text[64];
} assigned;

void NativeString_AssignUtf16(struct NativeString *s, const uint16_t *text,
                              int characters)
{
    (void)s;
    assigned.units = (uint32_t)characters;
    memcpy(assigned.text, text, (size_t)characters * 2);
}

static _Alignas(16) uint8_t album_storage[8 * AlbumGroup_bytes];

void AlbumGroupVector_Append(void *vector, const void *row)
{
    assert(vector == browser + MediaBrowser_albums);
    struct album *end = RV_FIELD(browser, MediaBrowser_albums_end, struct album *);
    if (!end) {
        RV_FIELD(browser, MediaBrowser_albums, void *) = album_storage;
        end = (struct album *)album_storage;
    }
    memcpy(end, row, sizeof *end);
    RV_FIELD(browser, MediaBrowser_albums_end, struct album *) = end + 1;
}

int MediaBrowser_AccumulateAlbumGroup(void *b, void *c, uint32_t lo, uint32_t hi,
                                      uint32_t i, struct MediaEntry *e, void *r,
                                      int first)
{
    (void)b, (void)c, (void)lo, (void)hi, (void)i, (void)e, (void)r, (void)first;
    abort();
}

/* Not reached by these tests: the open path and the artwork provider need the
 * emulator. */
void *LibraryBrowser_Create(uint32_t o)
{
    (void)o;
    abort();
}

void *LibraryBrowser_InitBorrowed(void *m, uint8_t f, void *b)
{
    (void)m, (void)f, (void)b;
    abort();
}

void *LibraryModelRef_Init(void *r, void *m, int s)
{
    (void)r, (void)m, (void)s;
    abort();
}

void *LibraryModelRef_Assign(void *d, void *s)
{
    (void)d, (void)s;
    abort();
}

void LibraryModelRef_Release(void *r)
{
    (void)r;
    abort();
}

void *LibraryModelRef_Get(void *r)
{
    (void)r;
    abort();
}

void *MediaListSelection_GetInstance(void)
{
    abort();
}

void *MediaListSelection_SetTitle(void *s, const struct NativeString *t)
{
    (void)s, (void)t;
    abort();
}

struct NativeString *NativeString_InitUtf8(struct NativeString *s, const char *u)
{
    (void)s, (void)u;
    abort();
}

struct NativeString *NativeString_Destroy(struct NativeString *s)
{
    (void)s;
    abort();
}

void MediaListSelection_SetList(void *s, void *r, uint32_t c, uint32_t o, uint8_t e)
{
    (void)s, (void)r, (void)c, (void)o, (void)e;
    abort();
}

void *ListArtworkSession_Get(void)
{
    abort();
}

void ArtworkSession_Stop(void *s)
{
    (void)s;
    abort();
}

int ListArtworkSession_SetSource(void *s, unsigned sel)
{
    (void)s, (void)sel;
    abort();
}

void *UI_GetControllerRegistry(void)
{
    abort();
}

void *UI_CreateScreenController(void *r, uint32_t s, uint32_t l, uint32_t t)
{
    (void)r, (void)s, (void)l, (void)t;
    abort();
}

int MediaBrowser_CreateChild(void *o, void **out)
{
    (void)o, (void)out;
    abort();
}

uint32_t MusicMenu_ConfigureLibraryBrowser(void *c, uint32_t i, uint32_t e, int r,
                                           uint32_t g, uint32_t s, uint32_t o)
{
    (void)c, (void)i, (void)e, (void)r, (void)g, (void)s, (void)o;
    abort();
}

void *Runtime_OperatorNew(uint32_t bytes)
{
    (void)bytes;
    abort();
}

uint8_t BrowserRegistry[16];

static void reset_groups(void)
{
    drop_rows(browser);
    assert(!pins && !live_blocks);
}

static void expect_groups(const uint32_t *tokens, const char *letters, uint32_t count)
{
    assert(build(browser) == count);
    assert(!pins && RV_FIELD(browser, MediaBrowser_artist_count, uint32_t) == count);
    struct group *rows = groups_of(browser);
    uint32_t *group_tokens = RV_FIELD(browser, MediaBrowser_artist_tokens, uint32_t *);
    for (uint32_t i = 0; i < count; i++) {
        assert(rows[i].token == tokens[i] && group_tokens[i] == tokens[i]);
        uint16_t letter[2];
        assert(!group_letter(browser, (int)i, letter));
        assert(letter[0] == 1 && letter[1] == (uint16_t)letters[i]);
        uint16_t name[256];
        assert(!group_name(browser, (int)i, name));
        assert(!memcmp(name + 1, strings[tokens[i]], name[0] * 2));
    }
    uint16_t none[2];
    assert(group_name(browser, (int)count, none) == -50 &&
           group_letter(browser, -1, none) == -50);
    /* Built rows are kept. */
    assert(build(browser) == count);
    reset_groups();
}

static void check_album_runs(void)
{
    const struct source source = {(struct MediaList *)list, NULL};
    struct track tracks[6];
    const uint32_t albums[] = {1, 1, 2, 1, 3, 1};
    for (unsigned i = 0; i < 6; i++)
        RV_FIELD(records[i], MediaRecord_album, uint32_t) = albums[i];
    tags[1].accepted = 0;
    tags[2].accepted = 0;
    pins++;
    assert(collect_album_runs(browser, &source, 6, NULL, 0) == 3);
    assert(collect_album_runs(browser, &source, 6, tracks, 3) == 3);
    assert(tracks[0].order == 0 && tracks[1].order == 4 && tracks[2].order == 5);
    tags[0].accepted = 0;
    tags[1].accepted = 1;
    assert(collect_album_runs(browser, &source, 6, tracks, 3) == 3);
    assert(tracks[0].order == 1);
    pins--;
    assert(build(browser) == 1);
    assert(groups_of(browser)->albums == 3);
    reset_groups();
    for (unsigned i = 0; i < 6; i++) {
        RV_FIELD(records[i], MediaRecord_album, uint32_t) = i + 1;
        tags[i].accepted = 1;
    }
}

/* The selected group's filter and the album rows of a group. */
static void check_group_albums(void)
{
    tags[0].album_artist = tags[4].album_artist = 1;
    tags[1].album_artist = 2;
    tags[2].album_artist = 3;
    RV_FIELD(records[3], MediaRecord_artist, uint32_t) = 9;
    tags[5].album_artist = 11;
    RV_FIELD(records[0], MediaRecord_album, uint32_t) =
        RV_FIELD(records[4], MediaRecord_album, uint32_t) = 5;
    assert(build(browser) == 5);
    pins++; /* Apple's filter evaluation and album build hold the caches */
    RV_FIELD(browser, MediaBrowser_selected_artist, int) = -1;
    assert(!outside_selected_group(browser, (struct MediaEntry *)entries[0]));
    RV_FIELD(browser, MediaBrowser_selected_artist, int) =
        3; /* The Clash, after the Sort As groups */
    assert(!outside_selected_group(browser, (struct MediaEntry *)entries[0]));
    assert(!outside_selected_group(browser, (struct MediaEntry *)entries[4]));
    assert(outside_selected_group(browser, (struct MediaEntry *)entries[1]));
    RV_FIELD(browser, MediaBrowser_selected_artist, int) = 9;
    assert(outside_selected_group(browser, (struct MediaEntry *)entries[0]));
    /* Album rows: one per album token, counting tracks, the resolved artist
     * as the subtitle, artwork from the first track. */
    assert(album_group(browser, NULL, 0, 0, 0, (struct MediaEntry *)entries[0],
                       records[0], 1) == 1);
    assert(album_group(browser, NULL, 0, 0, 1, (struct MediaEntry *)entries[4],
                       records[4], 0) == 1);
    assert(album_group(browser, NULL, 0, 0, 2, (struct MediaEntry *)entries[2],
                       records[2], 0) == 1);
    RV_FIELD(records[1], MediaRecord_album, uint32_t) = 0;
    assert(album_group(browser, NULL, 0, 0, 3, (struct MediaEntry *)entries[1],
                       records[1], 0) == 1);
    RV_FIELD(records[1], MediaRecord_album, uint32_t) = 2;
    assert(album_group(browser, NULL, 0, 0, 0, (struct MediaEntry *)entries[0],
                       records[0], 1) == 0);
    struct album *rows = RV_FIELD(browser, MediaBrowser_albums, struct album *);
    assert(RV_FIELD(browser, MediaBrowser_albums_end, struct album *) - rows == 2);
    assert(rows[0].token == 5 && rows[0].artist == 1 && rows[0].tracks == 2 &&
           !rows[0].artwork);
    assert(rows[1].token == 3 && rows[1].artist == 3 && rows[1].tracks == 1);
    pins--;
    album_artist_subtitle(browser, 0, NULL);
    assert(assigned.units == 9 && !memcmp(assigned.text, u"The Clash", 18));
    album_artist_subtitle(browser, 5, NULL);
    assert(assigned.units == 9);
    RV_FIELD(browser, MediaBrowser_albums, void *) = NULL;
    RV_FIELD(browser, MediaBrowser_albums_end, void *) = NULL;
    RV_FIELD(records[0], MediaRecord_album, uint32_t) = 1;
    RV_FIELD(records[4], MediaRecord_album, uint32_t) = 5;
    reset_groups();
}

int main(void)
{
    contexts[0].browser = browser;
    RV_FIELD(owner, MediaBrowser_track_data, void *) = track_data;
    RV_FIELD(list, MediaList_root, void *) = root;
    RV_FIELD(root, MediaEntry_groups, uint32_t) = 6; /* the widened track count */
    for (unsigned i = 0; i < 6; i++) {
        RV_FIELD(entries[i], MediaEntry_song_record, void *) = records[i];
        RV_FIELD(records[i], 0, void *) = track_data;
        tags[i].accepted = 1;
        RV_FIELD(records[i], MediaRecord_artist, uint32_t) = 4;
        RV_FIELD(records[i], MediaRecord_album, uint32_t) = i + 1;
    }
    check_album_runs();
    tags[0].album_artist = tags[4].album_artist = 1;
    tags[1].album_artist = 2;
    tags[2].album_artist = 3;
    RV_FIELD(records[3], MediaRecord_artist, uint32_t) = 9; /* the Artist fallback */
    tags[5].album_artist = 11;
    expect_groups((uint32_t[]){3, 11, 1, 2, 9}, "AATTT", 5);
    strings[11] = u"An Aardvark";
    expect_groups((uint32_t[]){3, 11, 1, 2, 9}, "AATTT", 5);
    strings[11] = u"A Zebra";
    expect_groups((uint32_t[]){11, 3, 1, 2, 9}, "AATTT", 5);
    strings[11] = u"An Artist";

    /* Sort As names come first, including a literal leading The. */
    tags[0].sort_album_artist = tags[1].sort_album_artist = 7;
    tags[4].sort_album_artist = 8;
    tags[3].sort_artist = 8;
    tags[2].sort_album_artist = 9;
    expect_groups((uint32_t[]){9, 11, 3, 1, 2}, "AATZZ", 5);

    /* Apple's own Album Artist order and jump table are consumed when complete. */
    native_order = calloc(1, MediaOrder_entries + 6 * sizeof(void *));
    RV_FIELD(native_order, MediaOrder_count, uint32_t) = 6;
    RV_FIELD(native_order, MediaOrder_capacity, uint32_t) = 6;
    const unsigned order[] = {3, 0, 2, 1, 4, 5};
    for (unsigned i = 0; i < 6; i++)
        ((void **)(native_order + MediaOrder_entries))[i] = entries[order[i]];
    RV_FIELD(list, MediaList_order_index + AlbumArtist_order_mode * sizeof(void *),
             void *) = native_order;
    uint32_t jumps[] = {4, 0xbeef0000 | 'B', 0, 'C', 1, 'X', 2, 'Y', 3};
    native_jump = jumps;
    RV_FIELD(list, MediaList_jump_index + AlbumArtist_order_mode * sizeof(void *),
             void *) = native_jump;
    expect_groups((uint32_t[]){9, 1, 3, 2, 11}, "BCXYY", 5);
    tags[0].accepted = 0;
    expect_groups((uint32_t[]){9, 3, 2, 1, 11}, "BXYYY", 5);
    tags[0].accepted = 1;
    /* A bad or missing jump table falls back to the Sort As letters in the
     * native order; an incomplete order falls back to the sort. */
    jumps[2] = 1;
    expect_groups((uint32_t[]){9, 1, 3, 2, 11}, "AZTZA", 5);
    RV_FIELD(list, MediaList_jump_index + AlbumArtist_order_mode * sizeof(void *),
             void *) = NULL;
    expect_groups((uint32_t[]){9, 1, 3, 2, 11}, "AZTZA", 5);
    RV_FIELD(native_order, MediaOrder_count, uint32_t) = 5;
    expect_groups((uint32_t[]){9, 11, 3, 1, 2}, "AATZZ", 5);
    RV_FIELD(list, MediaList_order_index + AlbumArtist_order_mode * sizeof(void *),
             void *) = NULL;
    free(native_order);

    /* No eligible track, then an allocation failure, leave no rows. */
    for (unsigned i = 0; i < 6; i++)
        tags[i].accepted = 0;
    expect_groups(NULL, "", 0);
    for (unsigned i = 0; i < 6; i++)
        tags[i].accepted = 1;
    fail_alloc = 1;
    assert(!build(browser) && !live_blocks && !pins);
    fail_alloc = 0;
    reset_groups();

    check_group_albums();

    /* Invalidation and the clear filter free the rows before Apple's clear. */
    assert(build(browser) == 5 && live_blocks);
    contexts[0].track_data = track_data;
    browser_invalidate(track_data);
    assert(!live_blocks && clears == 1 && !contexts[0].track_data);
    assert(build(browser) == 5);
    drop_rows(browser);
    MediaBrowser_ClearArtists(browser);
    assert(!live_blocks && clears == 2);
    return 0;
}
