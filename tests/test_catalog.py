import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import catalog


class CatalogTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.out = Path(self.temp.name)
        self.sources = {
            k: v
            for k, v in catalog.registry(catalog.ROOT / "catalog/sources.toml").items()
            if v["repo"] == catalog.FIRST_PARTY
        }
        self.packages = catalog.ROOT / "build/Extensions"

    def preview(self, identifier, source, old):
        return catalog.preview(identifier, source, old, packages=self.packages)

    def entries(self):
        return {
            e["id"]: e
            for e in json.loads((self.out / "catalog-v1.json").read_text())[
                "extensions"
            ]
        }

    def test_preview_packages_and_referenced_images(self):
        self.assertEqual(catalog.generate(self.sources, self.out, self.preview), [])
        entries = self.entries()
        self.assertEqual(set(entries), set(self.sources))
        for entry in entries.values():
            assets = [entry["package"]]
            if entry["category"] != "library":
                for shot in entry["screenshots"]:
                    self.assertTrue(shot["alt"])
                    self.assertEqual(shot["width"], 320)
                assets += [entry["icon"], *entry["screenshots"]]
            for asset in assets:
                data = (self.out / asset["url"]).read_bytes()
                self.assertEqual(hashlib.sha256(data).hexdigest(), asset["sha256"])
                self.assertEqual(len(data), asset["size"])
        self.assertEqual(entries["crossfeed"]["requires"], ["dsp"])

    def test_remote_release_and_failed_update_retains_all_assets(self):
        source = self.sources["balance"]
        repo = source["repo"]
        folder = catalog.ROOT / source["path"]
        package = (self.packages / "balance.rvx").read_bytes()
        release = {
            "tag_name": "balance/v0.1.1",
            "published_at": "2026-11-24T00:00:00Z",
            "assets": [
                {"name": "balance.rvx", "size": len(package), "state": "uploaded"}
            ],
        }

        def download(url, limit):
            if url.endswith(".rvx"):
                return package
            base = f"https://raw.githubusercontent.com/{repo}/{'a' * 40}/extensions/balance/"
            self.assertTrue(url.startswith(base))
            return (folder / url.removeprefix(base)).read_bytes()

        sources = {"balance": source}
        with (
            patch.object(catalog, "releases", return_value=[release]),
            patch.object(catalog, "api_json", return_value={"sha": "a" * 40}),
            patch.object(catalog, "read_url", side_effect=download),
        ):
            self.assertEqual(catalog.generate(sources, self.out, catalog.remote), [])
            before = self.entries()
            release["tag_name"] = "balance/v0.1.2"
            self.assertTrue(catalog.generate(sources, self.out, catalog.remote))
            self.assertEqual(before, self.entries())
            self.assertTrue((self.out / before["balance"]["package"]["url"]).exists())

    def test_missing_dependency_and_unregistered_extension(self):
        self.assertEqual(catalog.generate(self.sources, self.out, self.preview), [])
        del self.sources["dsp"]
        self.assertTrue(catalog.generate(self.sources, self.out, self.preview))
        self.assertNotIn("crossfeed", self.entries())
        self.assertNotIn("dsp", self.entries())

    def test_newest_is_numeric_and_per_extension(self):
        releases = [
            {"tag_name": tag}
            for tag in (
                "balance/v0.9.0",
                "dsp/v3.0.0",
                "balance/v0.10.0",
                "balance/v1.0.0-beta",
            )
        ]
        with patch.object(catalog, "releases", return_value=releases):
            self.assertEqual(
                catalog.newest(self.sources["balance"])["tag_name"], "balance/v0.10.0"
            )

    def test_submission_template_requires_description_and_matching_links(self):
        body = (
            "- Extension and what it does: A listening timer.\n"
            "- Source repository: https://github.com/example/timer\n"
            "- Stable release with the .rvx: https://github.com/example/timer/releases/tag/v1.0.0"
        )
        catalog.submission_body(body, "example/timer")
        with self.assertRaisesRegex(ValueError, "source repository"):
            catalog.submission_body(body, "another/timer")
        with self.assertRaisesRegex(ValueError, "tagged release"):
            catalog.submission_body(body.replace("/tag/v1.0.0", ""), "example/timer")
        with self.assertRaisesRegex(ValueError, "description"):
            catalog.submission_body("<!-- " + body + " -->", "example/timer")

    def test_submission_checks_changed_external_sources_only(self):
        sources = {
            "existing": {"repo": "creator/existing"},
            "new": {"repo": "creator/new"},
        }
        previous = {"existing": sources["existing"]}
        with patch.object(catalog, "remote", return_value=None) as remote:
            with self.assertRaisesRegex(ValueError, "stable release"):
                catalog.review(sources, previous)
            remote.assert_called_once_with("new", sources["new"], None)
        with (
            patch.object(
                catalog, "remote", return_value=({"requires": ["missing"]}, {})
            ),
            self.assertRaisesRegex(ValueError, "unavailable libraries"),
        ):
            catalog.review(sources, previous)

    def test_submission_uses_published_library_categories(self):
        self.assertEqual(catalog.generate(self.sources, self.out, self.preview), [])
        sources = {**self.sources, "player": {"repo": "creator/player"}}
        entry = {
            "category": "media",
            "requires": ["balance"],
            "version": "1.0.0",
            "package": {"sha256": "verified"},
        }
        with patch.object(catalog, "remote", return_value=(entry, {})) as remote:
            index = self.out / "catalog-v1.json"
            with self.assertRaisesRegex(ValueError, "unavailable libraries.*balance"):
                catalog.review(sources, self.sources, index=index)
            entry["requires"] = ["dsp"]
            catalog.review(sources, self.sources, index=index)
            self.assertEqual(
                [call.args[0] for call in remote.call_args_list], ["player", "player"]
            )

    def test_submission_accepts_library_from_same_pr_regardless_of_order(self):
        sources = {"player": {"repo": "creator/player"}, "lib": {"repo": "creator/lib"}}

        def release(identifier, source, old):
            return (
                {
                    "category": "library" if identifier == "lib" else "media",
                    "requires": [] if identifier == "lib" else ["lib"],
                    "version": "1.0.0",
                    "package": {"sha256": "verified"},
                },
                {},
            )

        with patch.object(catalog, "remote", side_effect=release) as remote:
            catalog.review(sources, {})
            self.assertEqual(remote.call_count, 2)

    def test_corrupt_package_is_rejected(self):
        source = self.sources["balance"]
        folder = catalog.ROOT / source["path"]
        package = bytearray((self.packages / "balance.rvx").read_bytes())
        package[-1] ^= 1
        with self.assertRaisesRegex(ValueError, "checksum"):
            catalog.snapshot(
                "balance",
                source,
                "0.1.1",
                lambda path: (folder / path).read_bytes(),
                package,
                None,
            )

    def test_image_path_stays_in_release_source(self):
        for value in (
            "../another/file.png",
            "/etc/passwd",
            "https://elsewhere/icon.png",
        ):
            with self.assertRaises(ValueError):
                catalog.relative(value)
