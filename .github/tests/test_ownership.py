"""Compile actual sources and the complete SDK with discovery removed before compilation."""

import os
import hashlib
import sqlite3
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path(os.environ.get("OWNERSHIP_SOURCE_ROOT", ROOT)).resolve()


@unittest.skipUnless(sys.platform == "darwin", "macOS framework shim")
class OwnershipTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(
            prefix="mail-sqlite-ownership-", dir=os.environ.get("OWNERSHIP_TMPDIR"),
        )
        cls.binary = Path(cls.temp.name) / "ownership"
        api = SOURCE / "4D Plugin API"
        sdk = (api / "4DPluginAPI.c").read_bytes()
        blob = hashlib.sha1(f"blob {len(sdk)}\0".encode() + sdk).hexdigest()
        if blob != "29788ed041effaf67269ccd93f971e8aee2ce5f8":
            cls.temp.cleanup()
            raise AssertionError("Re-audit the changed bundled SDK before updating its pinned hash")
        # Never compile real platform/path discovery into the runnable test.
        plugin = (SOURCE / "4DPlugin.cpp").read_text()
        start = plugin.index("NSURL *get_library_url()")
        end = plugin.index("#pragma mark SQL", start)
        safe_plugin = Path(cls.temp.name) / "privacy_plugin.mm"
        safe_plugin.write_text(
            plugin[:start]
            + 'NSURL *get_library_url() { std::abort(); }\n'
            + 'NSString *get_sqlite3_path_for_mail() {\n'
            + ' return [NSString stringWithUTF8String:std::getenv("OWNERSHIP_FIXTURE")];\n'
            + '}\n' + plugin[end:]
        )
        cls.fixture = Path(cls.temp.name) / "synthetic.sqlite"
        with sqlite3.connect(cls.fixture) as db:
            db.executescript("""
                CREATE TABLE properties(key TEXT, value TEXT);
                CREATE TABLE messages(sender INTEGER, subject INTEGER, mailbox INTEGER, snippet TEXT);
                CREATE TABLE addresses(address TEXT, comment TEXT);
                CREATE TABLE subjects(subject TEXT);
                CREATE TABLE mailboxes(url TEXT);
                CREATE TABLE attachments(message INTEGER, name TEXT);
                CREATE TABLE recipients(message INTEGER, address INTEGER, type INTEGER, position INTEGER);
            """)
            for text in ("ASCII", "\u65e5", "\U0001f600", ""):
                cursor = db.execute("INSERT INTO subjects VALUES (?)", (text,))
                rowid = cursor.lastrowid
                db.execute("INSERT INTO messages VALUES (NULL, ?, NULL, ?)", (rowid, text))
        command = [
            "clang++", "-std=c++14", "-x", "objective-c++",
            "-include", "Carbon/Carbon.h", "-I", str(api),
            "-I", str(SOURCE), "-I", cls.temp.name,
            "-I", str(api / "Classes"), "-fsanitize=address,undefined",
            "-fno-sanitize=alignment", "-fno-omit-frame-pointer", "-g",
            "-Wno-deprecated-declarations", "-Wno-return-type-c-linkage",
            "-Wno-ignored-attributes", "-Wno-writable-strings",
            str(api / "4DPluginAPI.c"),
            *map(str, sorted((api / "Classes").glob("*.cpp"))),
            str(ROOT / ".github/tests/ownership_host.mm"),
            "-framework", "Cocoa", "-framework", "Carbon",
            "-lsqlite3",
            "-o", str(cls.binary),
        ]
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode:
            cls.temp.cleanup()
            raise AssertionError(result.stdout + result.stderr)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def run_case(self, case, text="", empty="allocated"):
        result = subprocess.run(
            [str(self.binary), case, text, empty],
            env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0",
                 "OWNERSHIP_FIXTURE": str(self.fixture),
                 "UBSAN_OPTIONS": "halt_on_error=1"},
            capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("strings=0 pictures=0 handles=0", result.stdout)

    def test_path_single_disposal_and_retained_output(self):
        for text in ("ASCII", "\u65e5", "\U0001f600", ""):
            for empty in ("allocated", "null"):
                with self.subTest(text=text, empty=empty):
                    self.run_case("path", text, empty)

    def test_setbytes_temporary_release(self):
        self.run_case("bytes")

    def test_copy_byte_unit_boundary(self):
        for text in (".png", "\u65e5", "\U0001f600"):
            with self.subTest(text=text):
                self.run_case("copy", text, "null")

    def test_copy_initial_empty_owners(self):
        self.run_case("copy", "")

    def test_grayscale_initial_empty_owner(self):
        self.run_case("gray")

    def test_size_borrowed_input_and_empty_owner(self):
        self.run_case("size")

    def test_thumbnail_borrowed_input_and_owning_return(self):
        self.run_case("thumbnail")

    def test_array_transfers_copies_replacement_shrink_clear_rejection(self):
        for empty in ("allocated", "null"):
            with self.subTest(empty=empty):
                self.run_case("arrays", "", empty)

    def test_scalar_borrowed_copy_and_content_setter_returns(self):
        for text in ("ASCII", "\u65e5", "\U0001f600", ""):
            for empty in ("allocated", "null"):
                with self.subTest(text=text, empty=empty):
                    self.run_case("scalar", text, empty)

    def test_command_handle_failure_preserves_argument_owners(self):
        for case in ("path", "copy", "gray", "size", "thumbnail"):
            with self.subTest(case=case):
                self.run_case(case + "-handle-failure", "")

    def test_synthetic_sqlite_dispatch_scalar_return(self):
        for empty in ("allocated", "null"):
            with self.subTest(empty=empty):
                self.run_case("mail", "", empty)


if __name__ == "__main__":
    unittest.main()
