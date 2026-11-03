# 4d-plugin-mail-sqlite

Mail (SQLite) reads message metadata from Apple Mail's own database. Mail keeps an index of every message in an SQLite file named `Envelope Index` under `~/Library/Mail/`; the plugin opens that file with the system `libsqlite3` and returns, for the message IDs you pass, the subject, snippet, sender, recipients, attachment names, mailbox URL, size, flags and dates as JSON `Text`. Mail doesn't need to be running, and no AppleScript is involved. The result depends entirely on the layout of Mail's private database, so treat the plugin as **experimental**.

| Command | Returns | Purpose |
|---|---|---|
| [Mail Get message properties](#mail-get-message-properties) | `Text` (JSON) | Read the metadata of Mail messages, by message ID |

**Platforms:** macOS (Apple silicon and Intel)

<img src="https://cloud.githubusercontent.com/assets/1725068/18940649/21945000-8645-11e6-86ed-4a0f800e5a73.png" width="32" height="32" /> <img src="https://cloud.githubusercontent.com/assets/1725068/18940648/2192ddba-8645-11e6-864d-6d5692d55717.png" width="32" height="32" />

### Releases

[All releases](https://github.com/miyako/4d-plugin-mail-sqlite/releases). New releases are tagged `vX.Y.Z`; the original release is [1.0](https://github.com/miyako/4d-plugin-mail-sqlite/releases/tag/1.0).

---

## Requirements & platform notes

- **macOS only.** The plugin is written against Cocoa and `libsqlite3`. The bundle carries a `Windows64` binary from the original release, but the source has no Windows implementation, so don't rely on it.
- **4D needs Full Disk Access.** `~/Library/Mail` is protected by macOS privacy controls (macOS 10.14 and later). Unless the application running the plugin (4D, 4D Server, or your merged application) is listed under **System Settings > Privacy & Security > Full Disk Access**, the database can't be opened and the command returns `{}`. macOS doesn't prompt for this permission; you have to add the application yourself.
- **The database path is chosen from the macOS version, and the table stops at macOS 10.13.** Mail keeps its data in a versioned folder (`V2`, `V3`, …). The plugin maps the Darwin kernel version to a folder: 11 to 14 (OS X 10.7 to 10.10) → `V2`, 15 (10.11) → `V3`, 16 (10.12) → `V4`, 17 (10.13) → `V5`, anything newer → `V6`, anything older → `~/Library/Mail/Envelope Index`. Mail on macOS 10.15 and later uses a folder numbered higher than `V6`, so on every macOS this build supports (11.5 and later) the command looks for `~/Library/Mail/V6/MailData/Envelope Index`. If that file doesn't exist you get `{}`. If it does exist, it is a copy left over from an older macOS and the data is stale. See [Error handling & troubleshooting](#error-handling--troubleshooting).
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

- **IDs that are not in the database are skipped.** So is a message whose sender, subject or mailbox row is missing, because the query joins those tables. The `messages` collection can be shorter than your array, and nothing tells you which IDs were not found; compare the `ID` properties.
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
`snippet` | `Text` | Short preview of the message body, as Mail stores it
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

- **The result is `{}`.** The database file could not be opened. Either 4D doesn't have Full Disk Access, or the file the plugin looks for doesn't exist. On macOS 10.15 and later the plugin looks in `~/Library/Mail/V6/MailData/`, which current versions of Mail don't use, and there is no setting to point it elsewhere.
- **The result has `UUID` but no `messages`.** The database opened, but the message query failed, most likely because Mail's schema has changed since the plugin was written (for Mail on macOS 10.13).
- **The data looks old.** On a Mac upgraded from macOS 10.14, `~/Library/Mail/V6/` can still hold the database from that time. The plugin reads it without knowing it is stale.
- **Some IDs are missing from `messages`.** They are not in the database, or their sender, subject or mailbox row is missing. Compare the returned `ID` values with your array.
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
