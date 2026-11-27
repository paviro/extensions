/* SPDX-License-Identifier: GPL-3.0-only */
/* OSOS 2.0.5 player and file bindings used by the RepriseOS playback logger. */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "osos-2.0.5.h"

struct osos_media_list;

struct osos_media_entry {
    struct osos_media_list *list;
    struct osos_media_entry *parent, *next, *previous;
    uint32_t id, reserved[2];
    uint8_t flags[4];

    union {
        struct {
            struct osos_media_record *record;
            uint32_t shuffle_rank;
            struct osos_media_entry *next_record_entry;
        } song;

        struct {
            struct osos_media_entry *first, *last;
            uint32_t name;
        } group;
    };

    /* The core widens the track count to this word and keeps the container
     * count in the upper half of `flags`. */
    uint32_t tracks;
};

struct osos_media_record {
    uint8_t reserved[0x24];
    struct osos_media_entry *entries;
};

struct osos_media_item {
    uintptr_t *vtable;
    struct osos_media_entry *entry;
    struct osos_media_record *record;
    uint32_t reserved[3];
};

struct osos_playback_list {
    uintptr_t *vtable;
    struct osos_media_list *list;
    uint32_t reserved[3];
};

struct osos_player {
    uintptr_t *vtable;
    uint8_t reserved[0x50];
    void *current, *prepared;
    int current_index;
    struct osos_playback_list playback;
    uint8_t reserved_74[0x8d4];
    void *playback_source;
};

struct osos_native_string {
    uintptr_t *vtable;
    char *data;
};

struct osos_path {
    uint16_t volume;
    char text[258];
};

enum osos_media_text {
    OSOS_MEDIA_TITLE = 0x50,
    OSOS_MEDIA_ALBUM = 0x54,
    OSOS_MEDIA_ARTIST = 0x58,
    OSOS_MEDIA_GENRE = 0x60,
    OSOS_MEDIA_COMPOSER = 0x64,
};

static inline uintptr_t osos_method(void *object, unsigned int offset)
{
    return (*(uintptr_t **)object)[offset / 4];
}

static inline struct osos_player *osos_player_get(void)
{
    typedef struct osos_player *(*get_instance)(void);
    return ((get_instance)ADDR_MediaPlayer_GetInstance)();
}

static inline int osos_media_item_valid(struct osos_media_item *item)
{
    typedef int (*fn)(struct osos_media_item *);
    return ((fn)ADDR_MediaItem_IsValid)(item);
}

static inline struct osos_media_entry *osos_list_root(struct osos_media_list *list)
{
    return *(struct osos_media_entry **)((uint8_t *)list + 0x40);
}

static inline int osos_playback_list_valid(struct osos_playback_list *list)
{
    typedef int (*fn)(struct osos_playback_list *);
    return ((fn)ADDR_MediaListView_IsValid)(list);
}

static inline void osos_native_string_init(struct osos_native_string *string)
{
    typedef struct osos_native_string *(*fn)(struct osos_native_string *);
    ((fn)ADDR_NativeString_InitEmpty)(string);
}

static inline void osos_native_string_destroy(struct osos_native_string *string)
{
    typedef struct osos_native_string *(*fn)(struct osos_native_string *);
    ((fn)ADDR_NativeString_Destroy)(string);
}

static inline const char *osos_native_string_data(struct osos_native_string *string)
{
    typedef char *(*fn)(struct osos_native_string *);
    return ((fn)ADDR_NativeString_GetUtf8)(string);
}

static inline void osos_native_string_assign_utf16(struct osos_native_string *string,
                                                   const uint16_t *text, int32_t units)
{
    typedef void (*fn)(struct osos_native_string *, const uint16_t *, int32_t);
    ((fn)ADDR_NativeString_AssignUtf16)(string, text, units);
}

static inline void osos_media_item_text(struct osos_media_item *item,
                                        enum osos_media_text field,
                                        struct osos_native_string *text)
{
    typedef void (*fn)(struct osos_media_item *, struct osos_native_string *);
    ((fn)osos_method(item, field))(item, text);
}

static inline int osos_file_open(const struct osos_path *path, uint32_t mode,
                                 void **handle)
{
    typedef int (*fn)(const struct osos_path *, uint32_t, uint32_t, void **);
    return ((fn)ADDR_File_OpenLegacyPath)(path, 0x64617461, mode, handle);
}

static inline int osos_file_read(void *handle, void *data, uint32_t bytes,
                                 uint32_t *read)
{
    typedef int (*fn)(void *, void *, uint32_t, uint32_t, uint32_t *);
    return ((fn)ADDR_File_ReadLegacyHandle)(handle, data, bytes, 0, read);
}

static inline int osos_file_write(void *handle, const void *data, uint32_t bytes,
                                  uint32_t *written)
{
    typedef int (*fn)(void *, const void *, uint32_t, uint32_t, uint32_t *);
    return ((fn)ADDR_File_WriteLegacyHandle)(handle, data, bytes, 1, written);
}

static inline int osos_file_seek(void *handle, uint64_t offset, int origin)
{
    typedef int (*fn)(void *, uint64_t, int);
    return ((fn)ADDR_File_SeekLegacyHandle64)(handle, offset, origin);
}

static inline void osos_file_close(void *handle)
{
    typedef int (*fn)(void *);
    ((fn)ADDR_File_CloseLegacyHandle)(handle);
}

static inline uint32_t osos_media_item_sample_rate(const struct osos_media_item *item)
{
    if (!item->record)
        return 0;
    uint32_t bits = *(const uint32_t *)((const uint8_t *)item->record + 0x74);
    int exponent = (int)((bits >> 23) & 0xff) - 127;
    if (bits >> 31 || exponent < 0 || exponent > 23)
        return 0;
    return ((bits & 0x7fffff) | 0x800000) >> (23 - exponent);
}

static inline void osos_path_init(struct osos_path *path, const char *name)
{
    unsigned i = 0;
    path->volume = 0;
    for (; i < sizeof(path->text) - 1 && name[i]; i++)
        path->text[i] = name[i];
    path->text[i] = 0;
}

static inline void osos_clock_read(void *clock)
{
    typedef void (*fn)(void *);
    ((fn)ADDR_RTC_ReadDateTime)(clock);
}

static inline uint32_t osos_clock_seconds(void *clock)
{
    typedef uint32_t (*fn)(void *, int);
    return ((fn)0x08088dfc)(clock, 1);
}

static inline uint32_t osos_clock_mac_seconds(void *clock)
{
    typedef uint32_t (*fn)(void *, int);
    return ((fn)0x0809ff40)(clock, 1);
}

static inline uint32_t osos_playback_clock(void)
{
    typedef uint32_t (*fn)(void);
    return ((fn)0x0802d368)();
}

static inline int osos_player_is_shuffled(struct osos_player *player)
{
    typedef int (*fn)(struct osos_player *);
    return ((fn)0x081730d0)(player);
}

static inline int osos_player_is_repeat_one(struct osos_player *player)
{
    typedef int (*fn)(struct osos_player *);
    return ((fn)ADDR_MediaPlayer_IsRepeatOne)(player);
}

static inline int osos_player_is_repeat_all(struct osos_player *player)
{
    typedef int (*fn)(struct osos_player *);
    return ((fn)ADDR_MediaPlayer_IsRepeatAll)(player);
}

#define OSOS_MEDIA_GETTER(name, type, slot)                                            \
    static inline type osos_media_item_##name(struct osos_media_item *item)            \
    {                                                                                  \
        typedef type (*fn)(struct osos_media_item *);                                  \
        return ((fn)osos_method(item, slot))(item);                                    \
    }
OSOS_MEDIA_GETTER(duration, uint32_t, 0xa0)
OSOS_MEDIA_GETTER(bitrate, uint16_t, 0xb0)
OSOS_MEDIA_GETTER(year, uint16_t, 0xb4)
OSOS_MEDIA_GETTER(rating, int32_t, 0xb8)
OSOS_MEDIA_GETTER(track, uint16_t, 0xbc)
OSOS_MEDIA_GETTER(disc, uint16_t, 0xc0)
OSOS_MEDIA_GETTER(play_count, uint32_t, 0xc4)
OSOS_MEDIA_GETTER(skip_count, uint32_t, 0xcc)

#undef OSOS_MEDIA_GETTER
