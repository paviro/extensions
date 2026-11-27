/* SPDX-License-Identifier: GPL-3.0-only */

/* The Album Artists list reuses the stock Artists screen on a browser of the
 * extension's own: a child of the shared music browser whose artist rows this
 * file builds, grouped by the Album Artist of the fields table with the Artist
 * as the fallback. Apple's readers of the rows, the artist filter, the album
 * grouping and subtitle, the artwork source and the destructor are hooked and
 * answer for the owned browsers only; every other browser goes to Apple.
 *
 * Up to four browsers share the registry; one context per slot remembers
 * which are owned. Rows, tokens, jump table and the per-row artwork ids come
 * from api->alloc and are freed here before Apple's own clear, which frees
 * them with Heap_Free otherwise.
 *
 * Adapted from RepriseOS payload/album_artists.c and album_artists_index.h
 * (bfedd72) by lgreitis. */

#include "browser.h"
#include "fields.h"
#include "shared.h"
#include "osos-2.0.5.h"
#include "rv_api.h"
#include "rv_media.h"
#include <stddef.h>
#include <stdint.h>

/* An artist row of the browser's artists vector, Apple's layout. */
struct group {
    uint32_t token, display_token;
    uint32_t *artwork; /* up to ARTWORK_IDS artwork ids */
    uint32_t artwork_count, albums;
};

_Static_assert(offsetof(struct group, artwork) == ArtistGroup_artwork,
               "ArtistGroup layout");
_Static_assert(sizeof(struct group) == ArtistGroup_bytes, "ArtistGroup layout");
#define ARTWORK_IDS 10

/* An album row of the browser's albums vector, Apple's layout. */
struct album {
    uint32_t token, artist, display_artist, artwork, tracks, flags;
};

_Static_assert(sizeof(struct album) == AlbumGroup_bytes, "AlbumGroup layout");

/* One eligible track while the rows are built: its artist token, then the
 * group it fell into, and its position in the list order. */
struct track {
    union {
        uint32_t token;
        uint32_t group_index;
    };

    uint32_t order;
};

struct context {
    void *browser, *track_data;
    void *model_reference; /* non-owning; the model's destructor clears the context */
};
static struct context contexts[4];

/* The artwork model the album source of an open list reads through, with a
 * reference that keeps the owned model alive for as long as Apple's artwork
 * cache holds the provider. */
struct artwork_model {
    uint8_t native[LibraryBrowser_bytes];
    void *owner_reference;
};

#define ARTWORK_MODEL_OWNERSHIP 0x80 /* ours; Apple's ownership values stay below */
static void *artwork_model_reference;
static unsigned custom_artwork_providers;

static const uint16_t unknown[] = {'U', 'n', 'k', 'n', 'o', 'w', 'n'};

static void *zalloc(uint32_t bytes)
{
    return api->alloc(self, bytes);
}

static void release(void *block)
{
    if (block)
        api->free(self, block);
}

static struct context *context_for(void *browser)
{
    for (unsigned i = 0; i < 4; i++)
        if (browser && contexts[i].browser == browser)
            return &contexts[i];
    return NULL;
}

static void *browser_of(void *model)
{
    return RV_FIELD(model, LibraryBrowser_browser, void *);
}

static void *track_data_of(void *browser)
{
    return RV_FIELD(MediaBrowser_GetOwner(browser), MediaBrowser_track_data, void *);
}

static void *artist_cache(void *track_data)
{
    return (uint8_t *)track_data + TrackData_artist_cache;
}

static uint32_t token_view(void *track_data, uint32_t token, const uint16_t **text)
{
    uint32_t bytes = 0;
    *text = NULL;
    if (!token || StringCache_GetTokenView(artist_cache(track_data), token,
                                           (void **)text, &bytes))
        return 0;
    return bytes / 2;
}

/* The group a record belongs to: its Album Artist, else its Artist. */
static uint32_t resolved_artist_token(const void *record)
{
    const uint16_t *text;
    void *track_data = RV_FIELD(record, 0, void *);
    uint32_t token = fields_token(record);
    if (token && token_view(track_data, token, &text))
        return token;
    token = RV_FIELD(record, MediaRecord_artist, uint32_t);
    return token && token_view(track_data, token, &text) ? token : 0;
}

static uint32_t artist_name_view(void *track_data, uint32_t token,
                                 const uint16_t **text)
{
    uint32_t units = token_view(track_data, token, text);
    if (!units) {
        *text = unknown;
        units = sizeof unknown / sizeof *unknown;
    }
    return units;
}

static int compare_artist_names(void *track_data, uint32_t a, uint32_t b)
{
    if (a == b)
        return 0;
    const uint16_t *left, *right;
    uint32_t left_units = artist_name_view(track_data, a, &left);
    uint32_t right_units = artist_name_view(track_data, b, &right);
    return SongText_CompareUtf16(NULL, left, left_units, right, right_units);
}

/* The text a group sorts by: its Sort As, else its name. */
static uint32_t artist_sort_view(void *track_data, const void *record,
                                 const uint16_t **text)
{
    uint32_t token = fields_token(record);
    int has_album_artist = token && token_view(track_data, token, text);
    uint32_t units =
        token_view(track_data, fields_sort_token(record, has_album_artist), text);
    if (units)
        return units;
    return artist_name_view(track_data, resolved_artist_token(record), text);
}

/* Sorting of tracks, with the incoming order as the final tie-breaker. */

typedef int (*track_compare)(void *, const struct track *, const struct track *);

static int compare_tracks(const struct track *a, const struct track *b,
                          track_compare compare, void *context)
{
    int result = compare(context, a, b);
    return result ? result : (a->order > b->order) - (a->order < b->order);
}

static void sift(struct track *tracks, uint32_t root, uint32_t count,
                 track_compare compare, void *context)
{
    while (root < count / 2) {
        uint32_t child = root * 2 + 1;
        if (child + 1 < count &&
            compare_tracks(&tracks[child], &tracks[child + 1], compare, context) < 0)
            child++;
        if (compare_tracks(&tracks[root], &tracks[child], compare, context) >= 0)
            break;
        struct track swap = tracks[root];
        tracks[root] = tracks[child];
        tracks[child] = swap;
        root = child;
    }
}

static void sort_tracks(struct track *tracks, uint32_t count, track_compare compare,
                        void *context)
{
    for (uint32_t i = count / 2; i; i--)
        sift(tracks, i - 1, count, compare, context);
    for (uint32_t i = count; i > 1; i--) {
        struct track swap = tracks[0];
        tracks[0] = tracks[i - 1];
        tracks[i - 1] = swap;
        sift(tracks, 0, i - 1, compare, context);
    }
}

/* Puts rows into the order `order` lists them, in place. */
static void reorder(struct group *rows, struct track *order, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        struct group saved = rows[i];
        uint32_t current = i;
        while (order[current].group_index != i) {
            uint32_t next = order[current].group_index;
            rows[current] = rows[next];
            order[current].group_index = current;
            current = next;
        }
        rows[current] = saved;
        order[current].group_index = current;
    }
}

/* Apple's jump table: a count, then (letter, first index) pairs, then a
 * (0, UINT32_MAX) end marker. */
static int valid_jumps(const uint32_t *jump, uint32_t tracks)
{
    if (!jump || !jump[0] || jump[0] > tracks || jump[2] != 0)
        return 0;
    for (uint32_t i = 0; i < jump[0]; i++)
        if (jump[i * 2 + 2] >= tracks || (i && jump[i * 2 + 2] <= jump[i * 2]))
            return 0;
    return 1;
}

static uint16_t jump_letter(const uint32_t *jump, uint32_t index)
{
    uint32_t low = 0, high = jump[0];
    while (low + 1 < high) {
        uint32_t middle = low + (high - low) / 2;
        if (jump[middle * 2 + 2] <= index)
            low = middle;
        else
            high = middle;
    }
    return (uint16_t)jump[low * 2 + 1];
}

static int compare_track_artists(void *track_data, const struct track *a,
                                 const struct track *b)
{
    return compare_artist_names(track_data, a->token, b->token);
}

/* Where the tracks come from: the list, through Apple's native Album Artist
 * order when the import built one, else the album order, which keeps an
 * album's tracks together so each album is one run. */
struct source {
    struct MediaList *list;
    uint8_t *order; /* a MediaOrder, or NULL */
};

static struct MediaEntry *source_entry(const struct source *source, uint32_t index)
{
    if (source->order)
        return ((struct MediaEntry **)(source->order + MediaOrder_entries))[index];
    return MediaList_GetEntryByOrderIndex(source->list, MediaOrderMode_Album, 0, index);
}

static const void *entry_record(const struct MediaEntry *entry)
{
    return RV_FIELD(entry, MediaEntry_song_record, const void *);
}

struct group_sort {
    void *track_data;
    struct group *rows;
    struct source source;
};

static int compare_group_order(void *opaque, const struct track *a,
                               const struct track *b)
{
    struct group_sort *sort = opaque;
    if (sort->source.order)
        return (a->order > b->order) - (a->order < b->order);
    const uint16_t *left, *right;
    uint32_t left_units = artist_sort_view(
        sort->track_data, entry_record(source_entry(&sort->source, a->order)), &left);
    uint32_t right_units = artist_sort_view(
        sort->track_data, entry_record(source_entry(&sort->source, b->order)), &right);
    int result = SongText_CompareUtf16(NULL, left, left_units, right, right_units);
    return result ? result
                  : compare_artist_names(sort->track_data,
                                         sort->rows[a->group_index].token,
                                         sort->rows[b->group_index].token);
}

static uint16_t artist_letter(void *track_data, const void *record)
{
    const uint16_t *text;
    uint32_t units = artist_sort_view(track_data, record, &text);
    while (units && *text <= ' ') {
        text++;
        units--;
    }
    if (!units)
        return '#';
    for (uint16_t letter = 'A'; letter <= 'Z'; letter++)
        if (!SongText_CompareUtf16(NULL, text, 1, &letter, 1))
            return letter;
    return *text < 0x80 || (*text >= 0xd800 && *text <= 0xdfff) ? '#' : *text;
}

static struct group *groups_of(void *browser)
{
    return RV_FIELD(browser, MediaBrowser_artists, struct group *);
}

static uint32_t group_count(void *browser)
{
    return (uint32_t)(RV_FIELD(browser, MediaBrowser_artists + sizeof(void *),
                               struct group *) -
                      groups_of(browser));
}

/* Frees the rows and their arrays of an owned browser and leaves the words
 * Apple's clear would free empty. */
static void drop_rows(void *browser)
{
    struct group *rows = groups_of(browser);
    uint32_t count = group_count(browser);
    for (uint32_t i = 0; i < count; i++)
        release(rows[i].artwork);
    release(rows);
    for (unsigned i = 0; i < 3; i++)
        RV_FIELD(browser, MediaBrowser_artists + i * sizeof(void *), void *) = NULL;
    release(RV_FIELD(browser, MediaBrowser_artist_tokens, void *));
    RV_FIELD(browser, MediaBrowser_artist_tokens, void *) = NULL;
    release(RV_FIELD(browser, MediaBrowser_artist_jump, void *));
    RV_FIELD(browser, MediaBrowser_artist_jump, void *) = NULL;
    RV_FIELD(browser, MediaBrowser_artist_count, uint32_t) = 0;
}

static void clear_groups(void *browser)
{
    drop_rows(browser);
    MediaBrowser_ClearArtists(browser);
    MediaBrowser_ClearAlbums(browser);
    MediaBrowser_ClearSongs(browser);
    /* Apple's clear keeps the albums vector's capacity; an owned browser
     * releases it too. */
    Heap_Free(RV_FIELD(browser, MediaBrowser_albums, void *));
    for (unsigned i = 0; i < 3; i++)
        RV_FIELD(browser, MediaBrowser_albums + i * sizeof(void *), void *) = NULL;
}

void browser_invalidate(void *track_data)
{
    for (unsigned i = 0; i < 4; i++) {
        struct context *context = &contexts[i];
        if (!context->browser || context->track_data != track_data)
            continue;
        clear_groups(context->browser);
        /* The TrackData's destruction has unlinked its lists already. */
        RV_FIELD(context->browser, MediaBrowser_list, void *) = NULL;
        context->track_data = NULL;
    }
}

/* Building the rows */

static void build_group_rows(void *browser, void *track_data, struct track *tracks,
                             uint32_t used, struct group *rows,
                             const struct source *source)
{
    uint32_t group = 0, last_album = 0, previous_token = 0;
    struct group *row = NULL;
    void *owner = MediaBrowser_GetOwner(browser);
    for (uint32_t i = 0; i < used; i++) {
        struct track track = tracks[i];
        if (!i || compare_artist_names(track_data, previous_token, track.token)) {
            row = &rows[group];
            *row = (struct group){track.token, 0, NULL, 0, 0};
            /* One representative per group, in the consumed track slots. */
            tracks[group] = (struct track){.group_index = group, .order = track.order};
            group++;
            last_album = 0;
        }
        previous_token = track.token;
        const void *record = entry_record(source_entry(source, track.order));
        uint32_t album = RV_FIELD(record, MediaRecord_album, uint32_t);
        if (!album || album == last_album)
            continue;
        last_album = album;
        row->albums++;
        if (row->artwork_count == ARTWORK_IDS)
            continue;
        uint32_t artwork = MediaBrowser_GetRecordArtworkId(owner, (void *)record);
        if (artwork && !row->artwork)
            row->artwork = zalloc(ARTWORK_IDS * sizeof *row->artwork);
        if (artwork && row->artwork)
            row->artwork[row->artwork_count++] = artwork;
    }
}

static void build_group_jumps(void *track_data, const struct group *rows,
                              const struct track *tracks, uint32_t groups,
                              const uint32_t *native_jump, uint32_t *tokens,
                              uint32_t *jump, const struct source *source)
{
    jump[0] = 0;
    for (uint32_t i = 0; i < groups; i++) {
        tokens[i] = rows[tracks[i].group_index].token;
        uint16_t letter =
            native_jump
                ? jump_letter(native_jump, tracks[i].order)
                : artist_letter(track_data,
                                entry_record(source_entry(source, tracks[i].order)));
        if (!jump[0] || (uint16_t)jump[jump[0] * 2 - 1] != letter) {
            jump[jump[0] * 2 + 1] = letter;
            jump[jump[0] * 2 + 2] = i;
            jump[0]++;
        }
    }
    jump[jump[0] * 2 + 1] = 0;
    jump[jump[0] * 2 + 2] = UINT32_MAX;
}

/* One track per artist and album run; its first track gives the artwork and
 * the sort order. Counts when `tracks` is NULL. */
static uint32_t collect_album_runs(void *browser, const struct source *source,
                                   uint32_t count, struct track *tracks,
                                   uint32_t capacity)
{
    uint32_t used = 0, previous_artist = 0, previous_album = 0;
    for (uint32_t i = 0; i < count; i++) {
        struct MediaEntry *entry = source_entry(source, i);
        const void *record = entry ? entry_record(entry) : NULL;
        if (!record || !MediaBrowser_EvaluateEntryFilters(browser, entry, NULL))
            continue;
        uint32_t artist = resolved_artist_token(record);
        uint32_t album = RV_FIELD(record, MediaRecord_album, uint32_t);
        if (used && artist == previous_artist && album == previous_album)
            continue;
        if (tracks) {
            if (used == capacity)
                return 0;
            tracks[used] = (struct track){.token = artist, .order = i};
        }
        previous_artist = artist;
        previous_album = album;
        used++;
    }
    return used;
}

static uint32_t build(void *browser)
{
    struct context *context = context_for(browser);
    if (RV_FIELD(browser, MediaBrowser_artist_tokens, void *))
        return group_count(browser);
    void *track_data = track_data_of(browser);
    context->track_data = track_data;
    struct MediaList *list = MediaBrowser_GetCurrentList(browser);
    if (!track_data || !list || !RV_FIELD(list, MediaList_root, void *))
        return 0;
    /* The core widens the root's track count to a word at MediaEntry_groups,
     * so a library may pass 65,535 tracks. */
    uint32_t count = RV_FIELD(RV_FIELD(list, MediaList_root, uint8_t *),
                              MediaEntry_groups, uint32_t);
    if (!count)
        return 0;
    /* Apple's Album Artist order exists only when the import built it; there
     * is no comparator to rebuild it, so it is consumed when complete. */
    uint8_t *order =
        RV_FIELD(list, MediaList_order_index + AlbumArtist_order_mode * sizeof(void *),
                 uint8_t *);
    if (order && (RV_FIELD(order, MediaOrder_count, uint32_t) != count ||
                  RV_FIELD(order, MediaOrder_capacity, uint32_t) < count))
        order = NULL;
    const uint32_t *native_jump =
        order ? RV_FIELD(list,
                         MediaList_jump_index + AlbumArtist_order_mode * sizeof(void *),
                         uint32_t *)
              : NULL;
    if (!valid_jumps(native_jump, count))
        native_jump = NULL;
    TrackData_AcquireStringCaches(track_data);
    struct source source = {list, order};
    uint32_t runs = collect_album_runs(browser, &source, count, NULL, 0);
    struct track *tracks = runs ? zalloc(runs * sizeof *tracks) : NULL;
    uint32_t used =
        tracks ? collect_album_runs(browser, &source, count, tracks, runs) : 0;
    uint32_t groups = 0;
    struct group *rows = NULL;
    uint32_t *tokens = NULL, *jump = NULL;
    if (!used)
        goto done;
    sort_tracks(tracks, used, compare_track_artists, track_data);
    for (uint32_t i = 0; i < used; i++)
        if (!i ||
            compare_artist_names(track_data, tracks[i - 1].token, tracks[i].token))
            groups++;
    rows = zalloc(groups * sizeof *rows);
    tokens = zalloc(groups * sizeof *tokens);
    jump = zalloc((groups * 2 + 3) * sizeof *jump);
    if (!rows || !tokens || !jump) {
        release(rows);
        release(tokens);
        release(jump);
        groups = 0;
        goto done;
    }
    build_group_rows(browser, track_data, tracks, used, rows, &source);
    struct group_sort sort = {track_data, rows, source};
    sort_tracks(tracks, groups, compare_group_order, &sort);
    build_group_jumps(track_data, rows, tracks, groups, native_jump, tokens, jump,
                      &source);
    reorder(rows, tracks, groups);
    drop_rows(browser);
    RV_FIELD(browser, MediaBrowser_artists, struct group *) = rows;
    RV_FIELD(browser, MediaBrowser_artists + sizeof(void *), struct group *) =
        rows + groups;
    RV_FIELD(browser, MediaBrowser_artists + 2 * sizeof(void *), struct group *) =
        rows + groups;
    RV_FIELD(browser, MediaBrowser_artist_tokens, uint32_t *) = tokens;
    RV_FIELD(browser, MediaBrowser_artist_jump, uint32_t *) = jump;
    RV_FIELD(browser, MediaBrowser_artist_count, uint32_t) = groups;
done:
    TrackData_ReleaseStringCaches(track_data);
    release(tracks);
    return groups;
}

/* Hooks */

/* MediaBrowser_BuildArtists on an owned browser builds here. The slot takes
 * the model, the call sites the browser. */
uint32_t build_artists(uint32_t browser, const void *next)
{
    if (!context_for(ptr(browser)))
        return api->hook_next(next, browser, 0, 0, 0, 0, 0);
    return build(ptr(browser));
}

uint32_t build_artists_slot(uint32_t model, const void *next)
{
    void *browser = browser_of(ptr(model));
    if (!context_for(browser))
        return api->hook_next(next, model, 0, 0, 0, 0, 0);
    return build(browser);
}

/* Apple's clear of an owned browser's rows: ours go first, so its Heap_Free
 * sees nothing of them. */
uint32_t clear_artists(uint32_t browser, const void *next)
{
    if (context_for(ptr(browser)))
        drop_rows(ptr(browser));
    return api->hook_next(next, browser, 0, 0, 0, 0, 0);
}

/* A group's name as a counted UTF-16 string of at most 255 units, Apple's
 * status: 0, or -50 for an index outside the rows. */
static int group_name(void *browser, int index, uint16_t *counted)
{
    counted[0] = 0;
    if (index < 0 || (uint32_t)index >= group_count(browser))
        return -50;
    void *track_data = track_data_of(browser);
    TrackData_AcquireStringCaches(track_data);
    const uint16_t *text;
    uint32_t units =
        artist_name_view(track_data, groups_of(browser)[index].token, &text);
    if (units > 255)
        units = 255;
    if (units && text[units - 1] >= 0xd800 && text[units - 1] <= 0xdbff)
        units--;
    counted[0] = (uint16_t)units;
    for (uint32_t i = 0; i < units; i++)
        counted[i + 1] = text[i];
    TrackData_ReleaseStringCaches(track_data);
    return 0;
}

static int group_letter(void *browser, int index, uint16_t *counted)
{
    counted[0] = 0;
    if (index < 0 || (uint32_t)index >= group_count(browser))
        return -50;
    counted[0] = 1;
    counted[1] = jump_letter(
        RV_FIELD(browser, MediaBrowser_artist_jump, const uint32_t *), (uint32_t)index);
    return 0;
}

/* Whether the entry is outside the selected group, with the caches pinned by
 * Apple's filter evaluation. */
static int outside_selected_group(void *browser, const struct MediaEntry *entry)
{
    int index = RV_FIELD(browser, MediaBrowser_selected_artist, int);
    if (index < 0)
        return 0;
    const void *record = entry_record(entry);
    if ((uint32_t)index >= group_count(browser) || !record)
        return 1;
    return compare_artist_names(track_data_of(browser), groups_of(browser)[index].token,
                                resolved_artist_token(record)) != 0;
}

/* The subtitle of an album row: its artist's name. */
static void album_artist_subtitle(void *browser, int index, struct NativeString *out)
{
    struct album *begin = RV_FIELD(browser, MediaBrowser_albums, struct album *);
    struct album *end = RV_FIELD(browser, MediaBrowser_albums_end, struct album *);
    if (index < 0 || index >= end - begin)
        return;
    void *track_data = track_data_of(browser);
    TrackData_AcquireStringCaches(track_data);
    const uint16_t *text;
    uint32_t units = artist_name_view(track_data, begin[index].artist, &text);
    NativeString_AssignUtf16(out, text, (int)units);
    TrackData_ReleaseStringCaches(track_data);
}

uint32_t read_artist_name(uint32_t browser, uint32_t index, uint32_t out,
                          const void *next)
{
    if (!context_for(ptr(browser)))
        return api->hook_next(next, browser, index, out, 0, 0, 0);
    return (uint32_t)group_name(ptr(browser), (int)index, ptr(out));
}

uint32_t read_artist_jump_label(uint32_t browser, uint32_t index, uint32_t out,
                                const void *next)
{
    if (!context_for(ptr(browser)))
        return api->hook_next(next, browser, index, out, 0, 0, 0);
    return (uint32_t)group_letter(ptr(browser), (int)index, ptr(out));
}

/* The artist filter's compare in MediaBrowser_EvaluateEntryFilters: an owned
 * browser matches the entry against the selected group instead of the artist
 * text. Nonzero rejects the entry. */
uint32_t artist_filter_compare(uint32_t browser, uint32_t left, uint32_t left_units,
                               uint32_t right, const void *next, uint32_t right_units,
                               uint32_t entry)
{
    if (!context_for(ptr(browser)))
        return api->hook_next(next, 0, left, left_units, right, right_units, entry);
    return (uint32_t)outside_selected_group(ptr(browser), ptr(entry));
}

/* MediaBrowser_BuildAlbums' accumulator, through the literal it loads: an
 * owned browser groups its albums by the album token, with the resolved
 * artist for the subtitle. ARM: AlbumGroupVector_Append ends in Apple's
 * memcpy, which returns with mov pc, lr. */
RV_ARM int album_group(void *browser, void *callback, uint32_t field_low,
                       uint32_t field_high, uint32_t index, struct MediaEntry *entry,
                       void *record, int first)
{
    if (!context_for(browser))
        return MediaBrowser_AccumulateAlbumGroup(
            browser, callback, field_low, field_high, index, entry, record, first);
    struct album *begin = RV_FIELD(browser, MediaBrowser_albums, struct album *);
    struct album *end = RV_FIELD(browser, MediaBrowser_albums_end, struct album *);
    if (first && begin != end)
        return 0;
    uint32_t album = RV_FIELD(record, MediaRecord_album, uint32_t);
    if (!album)
        return 1;
    if (begin == end || end[-1].token != album) {
        struct album row = {
            album,
            resolved_artist_token(record),
            0,
            MediaBrowser_GetRecordArtworkId(MediaBrowser_GetOwner(browser), record),
            0,
            ((RV_FIELD(record, MediaRecord_flags_15, uint8_t) &
              MediaRecordFlags15_Compilation)
                 ? 1
                 : 0) |
                ((RV_FIELD(record, MediaRecord_flags_8f, uint8_t) &
                  MediaRecordFlags8F_PlaceholderAlbum)
                     ? 2
                     : 0)};
        AlbumGroupVector_Append((uint8_t *)browser + MediaBrowser_albums, &row);
        end = RV_FIELD(browser, MediaBrowser_albums_end, struct album *);
    }
    end[-1].tracks++;
    return 1;
}

uint32_t album_subtitle(uint32_t model, uint32_t index, uint32_t out, const void *next)
{
    void *browser = browser_of(ptr(model));
    if (!context_for(browser))
        return api->hook_next(next, model, index, out, 0, 0, 0);
    album_artist_subtitle(browser, (int)index, ptr(out));
    return 0;
}

/* Artwork: the album source of an owned list reads through a borrowed
 * model of its own, so Apple's artwork cache can outlive the screen. */

static int release_artwork(void)
{
    if (custom_artwork_providers) {
        void *session = ListArtworkSession_Get();
        if (session) {
            void *previous = artwork_model_reference;
            artwork_model_reference = NULL;
            ArtworkSession_Stop(session);
            ListArtworkSession_SetSource(session, AlbumArtwork_Selector);
            artwork_model_reference = previous;
        }
    }
    return !custom_artwork_providers;
}

uint32_t select_artwork_source(uint32_t selection, uint32_t category, const void *next)
{
    void *reference = (uint8_t *)ptr(selection) + MediaListSelection_model;
    void *model = LibraryModelRef_Get(reference);
    int custom =
        category == ArtworkCategory_Albums && model && context_for(browser_of(model));
    /* The provider and cache are made again even when both screens are album
     * categories. */
    if (custom || custom_artwork_providers)
        RV_FIELD(ptr(selection), MediaListSelection_artwork_category, uint8_t) = 0;
    if (!custom)
        release_artwork();
    void *previous = artwork_model_reference;
    artwork_model_reference = custom ? reference : NULL;
    uint32_t result = api->hook_next(next, selection, category, 0, 0, 0, 0);
    artwork_model_reference = previous;
    return result;
}

/* ARM: Runtime_OperatorNew returns with mov pc, lr. */
RV_ARM uint32_t create_artwork_provider(uint32_t kind, uint32_t selector,
                                        const void *next)
{
    uint32_t provider = api->hook_next(next, kind, selector, 0, 0, 0, 0);
    if (!provider || kind != ArtworkSource_SelectorKind ||
        selector != AlbumArtwork_Selector || !artwork_model_reference)
        return provider;
    void *owner = LibraryModelRef_Get(artwork_model_reference);
    struct artwork_model *model = Runtime_OperatorNew(sizeof *model);
    if (!model)
        return provider;
    LibraryBrowser_InitBorrowed(model, 2, browser_of(owner));
    model->owner_reference = NULL;
    LibraryModelRef_Assign(&model->owner_reference, artwork_model_reference);
    RV_FIELD(model, LibraryBrowser_ownership, uint8_t) = ARTWORK_MODEL_OWNERSHIP;
    custom_artwork_providers++;
    void *original = RV_FIELD(ptr(provider), AlbumArtworkProvider_model, void *);
    RV_FIELD(ptr(provider), AlbumArtworkProvider_model, void *) = model;
    typedef void (*destroy)(void *);
    RV_METHOD(original, LibraryBrowser_delete_method, destroy)(original);
    return provider;
}

/* LibraryBrowser_Destroy: the artwork model gives its reference back; an
 * owned model's browser drops its rows and context. Apple's body follows. */
uint32_t destroy_model(uint32_t model, const void *next)
{
    if (RV_FIELD(ptr(model), LibraryBrowser_ownership, uint8_t) ==
        ARTWORK_MODEL_OWNERSHIP) {
        if (custom_artwork_providers)
            custom_artwork_providers--;
        RV_FIELD(ptr(model), LibraryBrowser_ownership, uint8_t) =
            LibraryBrowserOwnership_Borrowed;
        LibraryModelRef_Release(&((struct artwork_model *)ptr(model))->owner_reference);
        return api->hook_next(next, model, 0, 0, 0, 0, 0);
    }
    void *browser = browser_of(ptr(model));
    struct context *context = context_for(browser);
    if (context && RV_FIELD(ptr(model), LibraryBrowser_ownership, uint8_t) ==
                       LibraryBrowserOwnership_Child) {
        if (context->track_data)
            MediaBrowser_InvalidateEntryMatches(browser);
        clear_groups(browser);
        *context = (struct context){0};
    }
    return api->hook_next(next, model, 0, 0, 0, 0, 0);
}

/* Opening the list */

static int owns_model(void *model)
{
    struct context *context = model ? context_for(browser_of(model)) : NULL;
    return context && context->model_reference &&
           LibraryModelRef_Get(&context->model_reference) == model;
}

/* The selection may still hold an owned model after its screens closed; when
 * it is the sole owner, a stock model takes its place so the registry slot
 * and the rows go. */
static void release_cached_selection(void)
{
    void *selection = MediaListSelection_GetInstance();
    void **reference = (void **)((uint8_t *)selection + MediaListSelection_model);
    void *model = LibraryModelRef_Get(reference);
    const uint32_t *block = *reference;
    if (!owns_model(model) ||
        RV_FIELD(model, LibraryBrowser_ownership, uint8_t) !=
            LibraryBrowserOwnership_Child ||
        !block || block[LibraryModelRefBlock_count / 4] != 1)
        return;
    void *stock = LibraryBrowser_Create(0);
    if (!stock)
        return;
    void *replacement = NULL;
    LibraryModelRef_Init(&replacement, stock, 0);
    MediaListSelection_SetList(selection, &replacement, 1, 0, 0);
    LibraryModelRef_Release(&replacement);
}

/* A model over a child browser of the shared music browser, in two free
 * registry slots. */
static void *create_model(void **reference)
{
    if (!release_artwork())
        return NULL;
    void **registry = (void **)BrowserRegistry;
    unsigned available = 0;
    for (unsigned i = 0; i < 4; i++)
        available += registry[i] == NULL;
    if (available < 2)
        return NULL;
    void *model = LibraryBrowser_Create(0);
    if (!model)
        return NULL;
    void *browser = NULL;
    typedef void (*destroy)(void *);
    if (MediaBrowser_CreateChild(browser_of(model), &browser) || !browser) {
        RV_METHOD(model, LibraryBrowser_delete_method, destroy)(model);
        return NULL;
    }
    RV_FIELD(model, LibraryBrowser_browser, void *) = browser;
    RV_FIELD(model, LibraryBrowser_ownership, uint8_t) = LibraryBrowserOwnership_Child;
    LibraryModelRef_Init(reference, model, 0);
    return model;
}

static void *acquire_model(void **reference)
{
    /* The selection or the artwork cache may keep the last model. */
    for (unsigned i = 0; i < 4; i++)
        if (contexts[i].model_reference) {
            LibraryModelRef_Assign(reference, &contexts[i].model_reference);
            return LibraryModelRef_Get(reference);
        }
    void *model = create_model(reference);
    if (!model)
        return NULL;
    void *browser = browser_of(model);
    unsigned slot = RV_FIELD(browser, MediaBrowser_registry_slot, uint8_t);
    contexts[slot] = (struct context){browser, track_data_of(browser), *reference};
    return model;
}

/* The model's query as the stock Artists list configures it: reset, artist
 * sort, the kinds the Music menu includes (the kind masks take GameMediaKind
 * bits). */
static void prepare_artists(void *model)
{
    typedef void (*reset)(void *);
    typedef void (*configure)(void *, uint32_t, uint32_t);
    RV_METHOD(model, LibraryBrowser_reset_method, reset)(model);
    RV_METHOD(model, LibraryBrowser_artist_sort_method, reset)(model);
    RV_METHOD(model, LibraryBrowser_kind_masks_method,
              configure)(model, GameMediaKind_Music | GameMediaKind_MusicVideo, 0);
}

void browser_open(const struct rv_context *ctx)
{
    void *reference = NULL;
    /* The shared music browser's query, which a child browser starts from:
     * music only, as the stock Artists row sets it. */
    if (!MusicMenu_ConfigureLibraryBrowser(ctx->controller, MediaKind_Music, 0, 1,
                                           LibraryGrouping_Artists, LibrarySort_Artist,
                                           0))
        return;
    void *model = acquire_model(&reference);
    if (!model)
        return;
    if (!release_artwork())
        goto done;
    prepare_artists(model);
    if (!build(browser_of(model)))
        goto done;
    /* The stock Artists screen titles itself from the selection. */
    _Alignas(uint32_t) uint8_t storage[NativeString_bytes];
    struct NativeString *title =
        NativeString_InitUtf8((struct NativeString *)storage, "Album Artists");
    MediaListSelection_SetTitle(MediaListSelection_GetInstance(), title);
    NativeString_Destroy(title);
    void *screen = UI_CreateScreenController(
        UI_GetControllerRegistry(), RES_MediaLists_Artists_Screen,
        RES_MediaLists_Artists_Screen_Default, Transition_ForegroundPush);
    if (!screen)
        goto done;
    LibraryModelRef_Assign((uint8_t *)screen + MediaListController_model, &reference);
    RV_FIELD(screen, MediaListController_created_model, uint8_t) = 0;
    rv_controller_push(ctx->controller, screen);
done:
    LibraryModelRef_Release(&reference);
}

/* While the hooks are still bound: the artwork cache and the selection give
 * back the models over owned browsers, so none outlives the module. */
void browser_unload(void)
{
    release_artwork();
    release_cached_selection();
    for (unsigned i = 0; i < 4; i++) {
        struct context *context = &contexts[i];
        if (!context->browser)
            continue;
        if (context->track_data)
            MediaBrowser_InvalidateEntryMatches(context->browser);
        clear_groups(context->browser);
        *context = (struct context){0};
    }
}
