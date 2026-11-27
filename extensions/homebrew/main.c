/* SPDX-License-Identifier: GPL-3.0-only */

/* Packages under /Reprise/Games/ and /Reprise/Apps/ in Apple's Games list. The
 * games service builds its one list once, so the build entry clears it whenever
 * what it holds is not what is wanted (games, or apps while the Apps screen is
 * open) and Apple scans again before a screen draws it; Apps scans before its
 * screen is pushed. Opening Apps sets a built games list aside instead and
 * closing it puts the list back, so Games is not scanned again for a visit to
 * Apps. The manifest reader is handed no signature path for a package, which is
 * how Apple itself reads a manifest it does not verify.
 *
 * Adapted from RepriseOS payload/homebrew.c (fd5d367) by lgreitis. */

#include <stddef.h>

#include "osos-2.0.5.h"
#include "rv_api.h"
#include "rv_libc.h"
#include "rv_menus.h"
#include "rv_sites.h"
#include "rv_ui.h"

#define PATH_BYTES 258 /* the text of Apple's legacy path, as the core's file layer */

static const struct rv_api *api;
static rv_extension_handle self;

/* A root as the games service scans it, as the manifest's content folder, and
 * where the packages under it keep their data: one folder under `data` for each
 * of Apple's UserData, Stats and SharedData, named as on Apple's disk for games
 * and with app in place of game for apps. */
struct root {
    const char *path;
    const char *content;
    const char *data;
    const char *user_data, *stats, *shared_data;
};
static const struct root games_root = {"/Reprise/Games/", "Games", "/Reprise/Data/Games/",
                                       "gamedata_RW/", "gamestats_WO/", "gamedata_ShareRW/"};
static const struct root apps_root = {"/Reprise/Apps/", "Apps", "/Reprise/Data/Apps/",
                                      "appdata_RW/", "appstats_WO/", "appdata_ShareRW/"};

/* The root the last scan listed and its ids, NUL separated, ending in an empty
 * name; what GameFiles_BuildPath redirects. */
static const struct root *listed_root;
static char *listed_ids;

/* What the list is wanted as, and what it holds: games, or apps while the Apps
 * screen is open. A list built before the extension loaded is dropped at the
 * first build after, so it is scanned again with the hooks in place. */
enum listing { GAMES, APPS };
static enum listing wanted, built;
static int fresh = 1;
/* The games service, from its first list build through the hooks; NULL until then. */
static void *service;
/* The games list set aside while the Apps screen is open: the words of Apple's
 * list that hold its entries, and its ids. Every reader of the list indexes it
 * afresh, so exchanging these words swaps the whole list. */
static const uint32_t list_words[] = {GameList_count, GameList_storage, GameList_capacity};
static uint32_t kept_words[RV_COUNT(list_words)];
static char *kept_ids;
static int kept;
/* The apps scans since the games list was kept. */
static unsigned apps_scans;
/* The packages under /Reprise/Apps/, counted again whenever a list build reads
 * the disk anyway, so the Extras pane does not; -1 until first counted. */
static int apps_count = -1;

/* 1 for the `n` bytes of a folder name that is one package id: not empty, not
 * starting with a dot, no path or drive separator. */
static int is_package_id(const char *name, unsigned n)
{
    if (!n || name[0] == '.')
        return 0;
    for (unsigned i = 0; i < n; i++)
        if (name[i] == '/' || name[i] == '\\' || name[i] == ':')
            return 0;
    return 1;
}

/* 1 for root + id + "/Manifest.plist" under one of the roots. */
static int is_homebrew_manifest(const char *path)
{
    const struct root *const roots[] = {&games_root, &apps_root};

    if (!path)
        return 0;
    for (unsigned r = 0; r < RV_COUNT(roots); r++) {
        unsigned n = strlen(roots[r]->path);
        if (memcmp(path, roots[r]->path, n) != 0)
            continue;
        const char *id = path + n, *end = id;
        while (*end && *end != '/')
            end++;
        return is_package_id(id, end - id) && strcmp(end, "/Manifest.plist") == 0;
    }
    return 0;
}

static uint32_t on_manifest_reader(void *reader, const char *manifest, uint32_t volume,
                                   const char *signature, const void *next)
{
    if (volume == FileVolume_Disk && is_homebrew_manifest(manifest))
        signature = 0;
    return api->hook_next(next, (uint32_t)reader, (uint32_t)manifest, volume,
                          (uint32_t)signature, 0, 0);
}

struct folders {
    uint32_t bytes;
    int ids;
    char *out;
};

static void count_folder(const char *name, int is_folder, void *arg)
{
    struct folders *f = arg;
    if (is_folder && is_package_id(name, strlen(name))) {
        f->bytes += strlen(name) + 1;
        f->ids++;
    }
}

static void copy_folder(const char *name, int is_folder, void *arg)
{
    struct folders *f = arg;
    if (!is_folder || !is_package_id(name, strlen(name)))
        return;
    unsigned n = strlen(name) + 1;
    memcpy(f->out, name, n);
    f->out += n;
}

static void count_package(const char *name, int is_folder, void *arg)
{
    if (is_folder && is_package_id(name, strlen(name)))
        ++*(int *)arg;
}

/* A pane that showed another count of /Reprise/Apps/ reads it again. */
static void set_apps_count(int count)
{
    int shown = apps_count;
    apps_count = count;
    if (shown >= 0 && count != shown)
        api->ui_pane_refresh(self);
}

static void count_apps(void)
{
    int count = 0;
    api->content_list(self, apps_root.content, count_package, &count);
    set_apps_count(count);
}

/* Remembers the ids under `root`, then lets Apple scan it. Without memory for
 * the ids the root is not scanned, but the list still counts as built from it. */
static void scan_root(void *games, void *list, const struct root *root)
{
    struct folders f = {1, 0, NULL};
    api->content_list(self, root->content, count_folder, &f);
    if (root == &apps_root)
        set_apps_count(f.ids);
    char *ids = api->alloc(self, f.bytes);
    if (listed_ids)
        api->free(self, listed_ids);
    listed_ids = ids;
    listed_root = root;
    if (!ids)
        return;
    f.out = ids;
    api->content_list(self, root->content, copy_folder, &f);
    *f.out = 0;
    GamesService_ScanRoot(games, list, FileVolume_Disk, root->path);
}

static uint32_t on_build_list(void *games, const void *next)
{
    service = games;
    if (fresh || built != wanted) {
        GamesService_ClearList(games);
        built = wanted;
        fresh = 0;
    }
    return api->hook_next(next, (uint32_t)games, 0, 0, 0, 0, 0);
}

static uint32_t on_scan_built_in_root(void *games, void *list, uint32_t volume,
                                      const char *root, const void *next)
{
    if (wanted == APPS)
        return 0;
    return api->hook_next(next, (uint32_t)games, (uint32_t)list, volume, (uint32_t)root, 0, 0);
}

static uint32_t on_scan_disk_root(void *games, void *list, uint32_t volume, const char *root,
                                  const void *next)
{
    if (wanted == APPS) {
        apps_scans++;
        scan_root(games, list, &apps_root);
        return 0;
    }
    uint32_t result =
        api->hook_next(next, (uint32_t)games, (uint32_t)list, volume, (uint32_t)root, 0, 0);
    scan_root(games, list, &games_root);
    count_apps();
    return result;
}

static int listed(const char *id)
{
    if (!listed_ids)
        return 0;
    for (const char *name = listed_ids; *name; name += strlen(name) + 1)
        if (strcmp(name, id) == 0)
            return 1;
    return 0;
}

/* The folder under the listed root's data folder for UserData, Stats or
 * SharedData. */
static const char *data_folder(uint32_t kind)
{
    switch (kind) {
    case GameDirectory_UserData:
        return listed_root->user_data;
    case GameDirectory_Stats:
        return listed_root->stats;
    default:
        return listed_root->shared_data;
    }
}

/* Appends `text` at `*at`, leaving room for the NUL before `end`; 0 when it
 * does not fit. */
static int append(char **at, char *end, const char *text)
{
    unsigned n = strlen(text);
    if (n >= (unsigned)(end - *at))
        return 0;
    memcpy(*at, text, n);
    *at += n;
    return 1;
}

/* A listed id's package is <root><id>/ and its data <data><folder><id>/, the
 * shared folder without the id as on Apple's disk; Apple's games and a path
 * that does not fit stay Apple's. */
static uint32_t on_build_path(struct NativeString *out, uint32_t kind, struct NativeString *id,
                              uint32_t storage, const void *next, const char *leaf)
{
    const char *name = id ? NativeString_GetUtf8(id) : NULL;
    char path[PATH_BYTES], *at = path, *end = path + sizeof path;
    int fits = 0;
    if (leaf && *leaf == '/')
        leaf++;
    if (kind <= GameDirectory_SharedData && storage == GameStorage_Disk && name &&
        listed(name)) {
        if (kind == GameDirectory_Package)
            fits = append(&at, end, listed_root->path) && append(&at, end, name) &&
                   append(&at, end, "/");
        else
            fits = append(&at, end, listed_root->data) &&
                   append(&at, end, data_folder(kind)) &&
                   (kind == GameDirectory_SharedData ||
                    (append(&at, end, name) && append(&at, end, "/")));
    }
    if (fits && append(&at, end, leaf ? leaf : "")) {
        *at = 0;
        NativeString_InitUtf8(out, path);
        return 0;
    }
    return api->hook_next(next, (uint32_t)out, kind, (uint32_t)id, storage, (uint32_t)leaf, 0);
}

/* The caption of a row while the list is apps: Apple's words for the play count
 * with Opened in place of Played, the last still formatted by Apple. */
static uint32_t on_played_string(void *resources, uint32_t id, const void *next)
{
    if (wanted == APPS) {
        if (id == RES_GamesMenu_NeverPlayed_String)
            return (uint32_t)"Never Opened";
        if (id == RES_GamesMenu_PlayedOnce_String)
            return (uint32_t)"Opened Once";
        if (id == RES_GamesMenu_PlayedTimes_String)
            return (uint32_t)"Opened %d Times";
    }
    return api->hook_next(next, (uint32_t)resources, id, 0, 0, 0, 0);
}

/* Apple keeps loaded row icons by artwork id, row + 1, across screens, so the
 * rows of the apps list take Apple's id plus APPS_ARTWORK_BASE and Games and
 * Apps never show each other's icons. */
#define APPS_ARTWORK_BASE 0x10000

static uint32_t on_artwork_id(void *games, uint32_t row, uint32_t *out_id, const void *next)
{
    uint32_t found = api->hook_next(next, (uint32_t)games, row, (uint32_t)out_id, 0, 0, 0);
    if (found && listed_root == &apps_root)
        *out_id += APPS_ARTWORK_BASE;
    return found;
}

/* Apple's artwork id, row + 1, for an artwork id of the list held; 0 for an id
 * of the other list. */
static uint32_t apple_artwork_id(uint32_t id)
{
    if (listed_root != &apps_root)
        return id < APPS_ARTWORK_BASE ? id : 0;
    return id > APPS_ARTWORK_BASE ? id - APPS_ARTWORK_BASE : 0;
}

static uint32_t on_artwork_path(void *games, uint32_t id, void *out, const void *next)
{
    id = apple_artwork_id(id);
    if (!id)
        return 0;
    return api->hook_next(next, (uint32_t)games, id, (uint32_t)out, 0, 0, 0);
}

static uint32_t on_artwork_storage(void *games, uint32_t id, const void *next)
{
    id = apple_artwork_id(id);
    if (!id)
        return 0;
    return api->hook_next(next, (uint32_t)games, id, 0, 0, 0, 0);
}

/* Empties Apple's list, which frees its entries, and forgets their ids. */
static void clear_list(void)
{
    GamesService_ClearList(service);
    if (listed_ids)
        api->free(self, listed_ids);
    listed_ids = NULL;
}

/* Exchanges the entries of Apple's list with the kept ones, ids included. */
static void swap_list(void)
{
    for (unsigned i = 0; i < RV_COUNT(list_words); i++) {
        uint32_t *word = &RV_FIELD(service, GamesService_list + list_words[i], uint32_t);
        uint32_t held = *word;
        *word = kept_words[i];
        kept_words[i] = held;
    }
    char *ids = listed_ids;
    listed_ids = kept_ids;
    kept_ids = ids;
}

/* Ends the keeping: Apple's clear frees the entries of the list it holds, the
 * apps, and the games come back; `restore` keeps them as built, otherwise they
 * are freed too and Games is scanned again. */
static void end_keeping(int restore)
{
    clear_list();
    swap_list();
    memset(kept_words, 0, sizeof kept_words);
    listed_root = &games_root;
    kept = 0;
    if (restore) {
        RV_FIELD(service, GamesService_list_built, uint32_t) = 1;
        built = GAMES;
        return;
    }
    clear_list();
}

/* Back to games once the Apps screen closed: the kept games list returns when
 * the apps were scanned once and are still listed. Anything else means Apple
 * cleared the list, as it does when disk mode ends; the kept list is out of date
 * then and goes. */
static void apps_closed(const struct rv_context *ctx)
{
    (void)ctx;
    wanted = GAMES;
    if (kept)
        end_keeping(apps_scans == 1 && RV_FIELD(service, GamesService_list_built, uint32_t));
}

/* The Apps row: the list is wanted as apps from before the push until the
 * screen closes. A games list built through the hooks is set aside for an empty
 * one (the kept words start zero), so Games is not scanned again after Apps.
 * The apps are scanned before the push: the screen starts Apple's artwork
 * session as it activates, before its first count, and the session loads the
 * icons of as many rows as the list holds then. */
static void open_apps(const struct rv_context *ctx)
{
    if (service && !fresh && built == GAMES &&
        RV_FIELD(service, GamesService_list_built, uint32_t)) {
        swap_list();
        listed_root = &apps_root;
        RV_FIELD(service, GamesService_list_built, uint32_t) = 0;
        built = APPS;
        apps_scans = 0;
        kept = 1;
    }
    wanted = APPS;
    GamesService_BuildList(GamesService_Get());
    if (api->ui_push(ctx->self, "apps", NULL) == RV_OK)
        return;
    wanted = GAMES;
    if (kept)
        end_keeping(1); /* the kept games list is still as built */
}

/* The Extras pane beside the Apps row: how many packages the folder holds. */
static const char *apps_caption(const struct rv_context *ctx)
{
    static char caption[16];
    (void)ctx;
    if (apps_count < 0)
        count_apps();
    rv_format_int(caption, sizeof caption, apps_count, apps_count == 1 ? " App" : " Apps");
    return caption;
}

/* Apple builds the list once, so what it holds follows the hooks that were bound
 * when it was built: dropped here so Apple builds it again on demand without
 * them. A list never built through the hooks holds nothing of ours. */
static void unload(void)
{
    if (kept)
        end_keeping(0);
    else if (service)
        clear_list();
}

static const struct rv_hook hooks[] = {
    RV_HOOK_FILTER(RV_SITE_GameManifest_InitReader_call, on_manifest_reader),
    RV_HOOK_FILTER(RV_SITE_GamesService_BuildList_entry, on_build_list),
    RV_HOOK_FILTER(RV_SITE_GamesService_ScanBuiltInRoot_call, on_scan_built_in_root),
    RV_HOOK_FILTER(RV_SITE_GamesService_ScanDiskRoot_call, on_scan_disk_root),
    RV_HOOK_FILTER(RV_SITE_GameFiles_BuildPath_entry, on_build_path),
    RV_HOOK_FILTER(RV_SITE_GamesService_PlayedString_call, on_played_string),
    RV_HOOK_FILTER(RV_SITE_GamesService_PlayedTimesString_call, on_played_string),
    RV_HOOK_FILTER(RV_SITE_GamesService_GetArtworkId_entry, on_artwork_id),
    RV_HOOK_FILTER(RV_SITE_GamesService_GetArtworkPath_entry, on_artwork_path),
    RV_HOOK_FILTER(RV_SITE_GamesService_GetArtworkStorage_entry, on_artwork_storage),
};

static const struct rv_screen screens[] = {
    RV_SCREEN_STOCK("apps", "Apps", RV_STOCK_SCREEN_GAMES, .closed = apps_closed),
};

static const struct rv_menu_entry entries[] = {
    RV_MENU_ENTRY(RV_MENU_EXTRAS, RV_EXTRAS_GAMES, RV_ROW_ACTION("Apps", open_apps),
                  .pane = RV_PANE(.caption_text = apps_caption, .bitmap = RV_BITMAP("apps"))),
};

static const struct rv_descriptor extension = {
    .size = sizeof extension,
    RV_HOOKS(hooks),
    RV_SCREENS(screens),
    RV_ENTRIES(entries),
    .unload = unload,
};

const struct rv_descriptor *rv_module_entry(const struct rv_api *core, rv_extension_handle handle)
{
    api = core;
    self = handle;
    return &extension;
}
