# Homebrew

Based on the RepriseOS core's [homebrew patch](https://github.com/RepriseOS/repriseos/blob/b9b1c62/payload/homebrew.c), by lgreitis.

Lists apps and games from folders under `/Reprise/` and lets Apple's game runtime start them unsigned. A package is a folder with the program, a `Manifest.plist` and an icon, built with the [eApp SDK](https://github.com/paviro/revola-eapp-sdk).

| Folder | Appears |
|---|---|
| `/Reprise/Games/<id>/` | Extras > Games, with the built-in games and those from iTunes |
| `/Reprise/Apps/<id>/` | Extras > Apps |

Apple builds one games list. Opening Apps sets it aside and builds an apps list from `/Reprise/Apps/`; leaving Apps frees that and restores the games list without a rescan. When disk mode ends, Apple rebuilds the games list from the stock folders and `/Reprise/Games/`. The Apps count in the Extras pane is recounted with each build. The list is never rebuilt while a package runs.

Only manifests under `/Reprise/Games/<id>/` and `/Reprise/Apps/<id>/` skip the signature check; games under `iPod_Control/` keep it. The id is one folder name, without `\` or `:` and not starting with a dot. A package with the id of a game under `iPod_Control/` replaces that game in the list.

| Apple's folder | Game | App |
|---|---|---|
| `iPod_Control/gamedata_RW/<id>/` | `/Reprise/Data/Games/gamedata_RW/<id>/` | `/Reprise/Data/Apps/appdata_RW/<id>/` |
| `iPod_Control/gamestats_WO/<id>/` | `/Reprise/Data/Games/gamestats_WO/<id>/` | `/Reprise/Data/Apps/appstats_WO/<id>/` |
| `iPod_Control/gamedata_ShareRW/` | `/Reprise/Data/Games/gamedata_ShareRW/` | `/Reprise/Data/Apps/appdata_ShareRW/` |

The shared folder has no id, so all games share one and all apps another.

On other firmware the same package uses Apple's folders like any game.

Turned off, the Apps row and both folders' packages disappear at the next list build.

| | |
|---|---|
| Default | on |
| Memory | image only; the ids of the listed folder while a list holds them, and the games list beside the apps one while Apps is open |
| Settings | none |
