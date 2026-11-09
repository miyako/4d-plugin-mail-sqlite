# 4d-plugin-mail-sqlite

Mail (SQLite) reads message metadata from Apple Mail's own database. Mail keeps an index of every message in an SQLite file named `Envelope Index` under `~/Library/Mail/`; the plugin opens that file with the system `libsqlite3` and returns, for the message IDs you pass, the subject, snippet, sender, recipients, attachment names, mailbox URL, size, flags and dates as JSON `Text`. Mail doesn't need to be running, and no AppleScript is involved. The result depends entirely on the layout of Mail's private database, so treat the plugin as **experimental**.

| Command | Returns | Purpose |
|---|---|---|
| [Mail Get message properties](#mail-get-message-properties) | `Text` (JSON) | Read the metadata of Mail messages, by message ID |

**Platforms:** macOS (Apple silicon and Intel)

<img src="https://cloud.githubusercontent.com/assets/1725068/18940649/21945000-8645-11e6-86ed-4a0f800e5a73.png" width="32" height="32" /> <img src="https://cloud.githubusercontent.com/assets/1725068/18940648/2192ddba-8645-11e6-864d-6d5692d55717.png" width="32" height="32" />

### Releases

[All releases](https://github.com/miyako/4d-plugin-mail-sqlite/releases). New releases are tagged `vX.Y.Z`; the original release is [1.0](https://github.com/miyako/4d-plugin-mail-sqlite/releases/tag/1.0).

**Bump version** is manual: explicitly select the source branch and choose
`patch`, `minor` or `major`. It checks out the immutable dispatch SHA, validates
clean source metadata, and commits only the source version fields. Initial,
final and executable pre-push checks require that branch to still advertise
the exact selected SHA. The push must be exactly one nonforce update from a
single-parent child commit, with tag following disabled. A moved, rewound,
deleted or already-updated branch fails closed; dispatch again after inspecting
the branch. Bump creates no tag, release or automatic Build.

**Build** (`release.yml`) is a separate manual dispatch with **zero inputs**.
Select the branch/commit to build; every checkout uses its immutable
`github.sha`. Source metadata must contain one strict `X.Y.Z`. Both version keys
in the three Xcode configurations are coordinated with source `Info.plist` and
`English.lproj/InfoPlist.strings`. The original workflow header and modernization
commit `78361da` promised this coordination; bump `3303693` updated only
marketing. The migration repairs the stale current/localized values to the
existing `1.0.1`, without incrementing the version. Existing supported Xcode
references and absent `CFBundleVersion` are preserved. Prebuilt bundles, 4D
databases, manifest/schema versions, localization counters and dependencies
are not source-version authority and are never bumped.

Publication is serialized across all branches, without cancelling running
Builds. Early and final checks require successful exact remote-tag queries and
authenticated pagination of **all releases, including drafts**; API errors
(including 404), malformed responses and existing tags/releases stop publication.
Only after the universal binary/resource, codesign verification, zip/dmg
notarization and DMG stapling gates succeed does Build repeat availability
checks, create a new lightweight `vX.Y.Z` tag at the selected SHA, verify it
remotely, POST a new published non-prerelease with empty notes (the original
policy), then upload `Mail (SQLite)-X.Y.Z.zip` and `.dmg` separately to its new
release ID. There is no update, overwrite, rollback or retry.

**Partial publication recovery:** any newly created tag, release and earlier
uploaded assets remain if a later operation fails. Inspect the Actions error
and remote state before deciding on manual recovery. Do not rerun Build for the
same version expecting it to repair an existing tag/release: it deliberately
refuses. Resolve the partial publication manually, or bump a new source version
on the intended branch and dispatch Build separately.

Native release prerequisites remain unchanged: macOS 11.5 deployment target,
universal arm64/x86_64 output, Cocoa/AppKit/CoreGraphics and SDK `libsqlite3.tbd`,
resource checks and Developer ID/notary credentials. The historical Windows64
binary, QuickLook and other committed bundle resources ship as-is; no Windows
job or native-gate bypass is introduced. Required secret **names** are
`APPLE_DEVELOPER_ID_CERTIFICATE`, `APPLE_DEVELOPER_ID_CERTIFICATE_PASSWORD`,
`KEYCHAIN_PASSWORD`, `NOTARYTOOL_APPLE_ID`, `NOTARYTOOL_TEAM_ID` and
`NOTARYTOOL_PASSWORD`. Local lifecycle regressions do not execute native builds,
signing, notarization or publication:

```sh
python3 -B -m unittest discover -s .github/tests -v
actionlint
```

---

## Requirements & platform notes

- **macOS only.** The plugin is written against Cocoa and `libsqlite3`. The bundle carries a `Windows64` binary from the original release, but the source has no Windows implementation, so don't rely on it.
- **4D needs Full Disk Access.** `~/Library/Mail` is protected by macOS privacy controls (macOS 10.14 and later). Unless the application running the plugin (4D, 4D Server, or your merged application) is listed under **System Settings > Privacy & Security > Full Disk Access**, the database can't be opened and the command returns `{}`. macOS doesn't prompt for this permission; you have to add the application yourself.
- **The plugin reads the newest Mail database it finds.** Mail keeps its data in a versioned folder whose number changes with macOS releases (`V8` on Big Sur, `V9` on Monterey, `V10` on Ventura and later, according to community reverse-engineering; Apple doesn't document it). An upgraded Mac can still hold the folders of earlier versions. The plugin looks in `~/Library/Mail/` for the highest-numbered `V<n>` folder that contains `MailData/Envelope Index` and opens that one. If there is none, it falls back to `~/Library/Mail/Envelope Index` (Mail before OS X 10.7). Release 1.0 instead mapped the Darwin version to a fixed folder and stopped at `V6`, so on macOS 10.15 and later it read a missing or stale file. This is the behaviour of the current source (2026 fix). See [Error handling & troubleshooting](#error-handling--troubleshooting).
- **The plugin adapts to Mail's schema.** Mail's database layout is private and has changed between versions. Before querying, the plugin checks which tables and columns exist: it reads the snippet from `messages.snippet` (V6 and earlier) or from the `summaries` table (later versions), accepts both the old (`message_id`, `address_id`) and new (`message`, `address`) column names in `recipients` and `attachments`, and leaves out any number property whose column is missing (for example `date_created` on recent versions). Tested against mock databases built with both layouts, not against a real Mail database on current macOS (2026 fix).
- **Failure is silent, not a 4D error.** No permission, a missing database, or a schema the queries don't match all give you a result with fewer keys, down to `{}`. The command never raises a 4D error, so check the keys before you use them.
- **The command reads, it never writes.** The database is opened without permission to create the file and with SQLite's `query_only` mode on, so the plugin can't create an empty `Envelope Index` or change Mail's data. This is the behaviour of the current source (2026 review fixes); release 1.0 opened the file read-write and could create an empty database at a wrong path.
- **It runs in the calling process and blocks it until the queries finish.** If Mail holds a lock on the database, the command waits up to 1 second for it (current source; release 1.0 didn't wait and could return a partial result). The manifest doesn't declare the command thread-safe, so expect 4D to refuse it in preemptive processes.

---

## Mail Get message properties

### Syntax

```
Mail Get message properties ( messages ) → properties
```

Parameter | Type | Description
------------|------------|----
`messages` | `ARRAY LONGINT` | Mail message IDs (the `ROWID` of the `messages` table in `Envelope Index`). Element 0 is ignored.
Result | `Text` | JSON object with the database `UUID` and a `messages` array; `{}` if the database can't be opened

### Description

The command opens Mail's `Envelope Index` database (see [Requirements & platform notes](#requirements--platform-notes) for which file), reads the database `UUID`, then looks up every ID in `messages` and returns one object per message it finds.

The IDs are the row numbers Mail uses in its database. The original README describes them as the message `id` you see in AppleScript, which is what the sibling plugin [4d-plugin-mail-applescript](https://github.com/miyako/4d-plugin-mail-applescript) returns for the messages selected in Mail.

What you get back:

- **IDs that are not in the database are skipped.** A message whose sender, subject or mailbox row is missing is still returned, with empty text for the missing parts (since the 2026 fix; release 1.0 skipped it). The `messages` collection can be shorter than your array, and nothing tells you which IDs were not found; compare the `ID` properties.
- **The order of `messages` doesn't necessarily follow your array.** The query has no `ORDER BY`, so SQLite returns rows in whatever order its query plan produces (inferred from the SQL, not tested on a real Mail database).
- **An empty array, or an array that is not `ARRAY LONGINT`,** gives `"messages":[]`. Only a Longint array is read; any other array type is treated as empty.
- **Text columns that are NULL** become `""`.
- **Numbers are returned as Mail stores them.** The plugin doesn't decode `flags`, `color` or `type`, and doesn't convert the dates. The date columns look like Unix timestamps (seconds since 1970-01-01 UTC), but that is Mail's private format and the plugin doesn't confirm it; check against a known message before relying on it.
- **JSON key order is not fixed**, and `/` and non-ASCII characters are written unescaped (since the bundled JSON library was replaced with the system one in 1.0.0). `JSON Parse` reads both forms the same.

#### The returned object

Property | Type | Description
------------|------------|----
`UUID` | `Text` | The database's identifier (from its `properties` table). The original README describes it as the folder name used inside each `.mbox`.
`messages` | `Collection` | One message object per ID found. Missing if the message query failed (for example because the schema differs).

#### The message object

Property | Type | Description
------------|------------|----
`ID` | `Number` | The message ID, as passed in `messages`
`subject` | `Text` | Subject line
`snippet` | `Text` | Short preview of the message body, as Mail stores it. On recent versions this comes from the `summaries` table, which Mail fills for only some messages, so it is often `""`. Missing if the database has neither source.
`mailbox` | `Text` | URL-style identifier of the mailbox that holds the message
`sender` | `Object` | `ID` (`Number`), `address` (`Text`), `name` (`Text`, display name)
`recipients` | `Collection` | One object per recipient: `ID` (`Number`), `address` (`Text`), `name` (`Text`, display name)
`attachments` | `Collection` | One object per attachment: `ID` (`Number`), `name` (`Text`, file name)
`size` | `Number` | Message size, as stored by Mail
`flags` | `Number` | Mail's raw flag bits
`flagged` | `Number` | Flagged state, as stored by Mail
`color` | `Number` | Color, as stored by Mail
`type` | `Number` | Message type, as stored by Mail
`date_sent` | `Number` | Date sent, raw value (see above)
`date_received` | `Number` | Date received, raw value
`date_created` | `Number` | Date the entry was created, raw value

The number properties from `size` to `date_created` are returned only when the database has that column; recent Mail versions have no `date_created`, for example. A column that exists but holds no value is returned as `0`.

The recipient objects don't say whether a recipient is To, Cc or Bcc: the plugin reads that column but doesn't return it.

### Example

From the plugin's original README:

```4d
ARRAY LONGINT($values;3)

$values{1}:=1238253
$values{2}:=1238259
$values{3}:=1238271

$json:=Mail Get message properties ($values)

SET TEXT TO PASTEBOARD($json)

C_OBJECT($o)
$o:=JSON Parse($json)
```

Checking the result before using it, then going through each message:

```4d
var $o; $message : Object
var $json : Text

$json:=Mail Get message properties ($values)
$o:=JSON Parse($json)

If ($o.messages=Null)
	ALERT("Mail's database could not be read (Full Disk Access? database path?)")
Else
	For each ($message; $o.messages)
		// $message.ID, $message.subject, $message.sender.address, ...
	End for each
End if
```

---

## Error handling & troubleshooting

- **The result is `{}`.** The database file could not be opened. Most often 4D doesn't have Full Disk Access: without it the plugin can't even list `~/Library/Mail/`, so it finds no `V<n>` folder. Otherwise Mail has never been set up for this user account.
- **The result has `UUID` but no `messages`.** The database opened, but the message query failed. The plugin adapts to the schema differences it knows about, so this means Mail's schema has changed in a new way.
- **Some IDs are missing from `messages`.** They are not in the database. Mail's IDs belong to one database: after an upgrade to a new `V<n>` folder, or after Mail rebuilds its index, old IDs no longer match. Compare the returned `ID` values with your array.
- **`attachments` or `recipients` is missing from a message.** That sub-query failed to prepare (schema difference); the rest of the message is still returned.
- **The command is refused in a preemptive process.** It isn't declared thread-safe; call it from a cooperative process.

---

## Quick reference

```4d
ARRAY LONGINT($ids;0)
APPEND TO ARRAY($ids;1238253)
$o:=JSON Parse(Mail Get message properties ($ids))
// $o.UUID, $o.messages[0].subject, $o.messages[0].sender.address, $o.messages[0].attachments
```
