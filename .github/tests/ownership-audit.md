# Native string ownership audit

## Immutable evidence and scope

Baseline: `3e926aa8fe50eb57faf7dbea6d608e6655a8f6b3`.
Guidance: `miyako/skills`, `4d-skills/skills/4dplugin/SKILL.md`,
`e6b492fcb81e5e1a7f7db723594b420e63bf6eb9` (including conditional transfer and
scalar/callback guidance). No children or subagents were used.

Bundled SDK tree: `90937ac308a76cb8a38ed9e9fe4078186d7b0c64`.
The complete compiled `4DPluginAPI.c` is Git blob
`29788ed041effaf67269ccd93f971e8aee2ce5f8` (pinned by the regression driver);
header `9054b45ffc7ff6f5b8d57529a3788a55d977b314`,
`EntryPoints.h` `730f2f77ff2713fb2fe4b7d6562baeedf5f664cb`,
`PrivateTypes.h` `77053dc0c0a3718d2d3b6f158371a6af78c6ae66`,
`PublicTypes.h` `2bb392e73e9c556c274fe0a75b3a6b83e310b77c`,
`Flags.h` `10a83185333dac06a62dda61b9f9428243d1e2ee`.
The implementation's v14 and header's v13 comments are not verified upstream
SDK identities. No SDK engine implementation, ABI header or third-party code
was changed.

The reusable harness was retrieved using GitHub contents/tree APIs from
`miyako/4d-plugin-jpegoptim` at
`e49adcd32bd5500cf3de680faa3649b32463ff5e`,
`.github/tests/{ownership_host.mm,test_ownership.py}`. Its target SDK hash was
not reused: the target implementation and layouts were inspected independently.
The test adapts that engine-boundary shim to this complete SDK and adds the
actual Mail dispatcher with synthetic SQLite input.

## Complete tracked native and build inventory

| Files | Build/include and ownership disposition |
|---|---|
| `4DPlugin.cpp`, `4DPlugin.h` | Sole command declaration and dispatcher: `PluginMain` -> `CommandDispatcher` selector 1 -> `Mail_Get_message_properties`; helper calls `Mail_Get_message_properties_json`, `json_stringify`, `json_string_value`, schema helpers, `SQL::toString` (dormant), SQLite statement/connection guards. Input `ARRAY_LONGINT` copies integers. SQLite text is copied immediately into Foundation strings before statement stepping/finalization; dictionaries/arrays retain values across inner pools. Serialized JSON is copied into `C_TEXT` before the temporary NSString and outer pool are released; scalar return is a content setter. No SDK JSON getters or JSONParse commands are called. |
| `4D Plugin API/Classes/ARRAY_{BOOLEAN,DATE,INTEGER,LONGINT,REAL,TEXT,TIME}.{cpp,h}` | All seven pairs are compiled, not just the manifest's longint input. All headers included from `4DPluginAPI.h`. Non-text arrays contain values, not string descriptors. `ARRAY_TEXT` copies borrowed UTF-16 descriptors into independent C++ strings on input; output creates a fresh descriptor for each element and transfers it without post-insertion disposal. Normal replacement/shrink/clear and rejected SDK destinations are covered. Allocation/resize failure boundaries are explicitly deferred below. |
| `4D Plugin API/Classes/C_{BLOB,DATE,INTEGER,LONGINT,PICTURE,POINTER,REAL,TEXT,TIME}.{cpp,h}` | All nine pairs compiled and included from `4DPluginAPI.h`. Numeric/date/time wrappers store scalar values. CBytes copies byte content, with non-string failure concerns out of scope. C_TEXT owns independent C++ UTF-16 storage, copies parameters, uses content setters for outputs and paired CF/NSString release for conversions. C_PICTURE/CPicture is compiled despite being unused by the exported command; its path, copy, grayscale, dimensions, thumbnail, retain/return, byte and native-image helper paths were inspected. C_POINTER's metadata construction is deferred rather than blindly clearing a possibly special descriptor. |
| `4D Plugin API/Classes/C_TYPES.h` | Shared package/UTF string types and Foundation/Windows includes; reached through text/longint headers. No runtime owner. |
| `4D Plugin API/{4DPluginAPI.c,4DPluginAPI.h,EntryPoints.h,Flags.h,PrivateTypes.h,PublicTypes.h,4DPluginAPI.def}` | Complete bundled engine boundary, platform entry point `FourDPackex`, descriptor and EngineBlock layout, kinds and selector definitions. All string-returning SDK families inventoried: create/getter/field/array/scalar, application/structure/preferences/folder paths, localization, detokenize, dialog text, function/method/command returns and pointer values. SDK-only functions are not treated as first-party callsites or modified. |
| `4D Plugin_Prefix.pch` | Carbon prefix include, no runtime owner. |
| `Mail (SQLite).xcodeproj/project.pbxproj`, both `.vcxproj` and `.vcxproj.filters` pairs | Xcode and both Windows project source lists include the dispatcher, all 16 wrapper implementations and SDK implementation. Xcode marks the dispatcher and SDK as Objective-C++; umbrella headers reach all wrappers. The Xcode source list compiles picture helpers regardless of manifest usage. Xcode phases are headers/resources/copy/sources/frameworks/Rez, not a harness target; no Xcode build was run. Windows projects are historical and do not supply a Windows Cocoa implementation. |
| Root and bundled `manifest.json`, `constants.xlf`, `Info.plist`, `documentation.htm` and bundled documentation | Single `Mail Get message properties(&Y):T`; longint array input and scalar text output, not proof of helper safety. Manifest/resources/projects/prebuilt binaries and 4D data files are untouched. No 4D data file was opened. |
| `.github/workflows/{release,bump-version}.yml`, release scripts and pre-push helper | Existing workflows are manual-only. Release builds/signs/notarizes/packages/publishes; none dispatched. New narrowly scoped unsigned ownership CI only compiles and runs the synthetic harness on `macos-latest`, with read-only permissions. |

No other tracked native implementations or platform-specific SDK copies exist
in the baseline inventory. This is a source/build audit, not a claim that every
SDK-only function has a proven host allocation contract.

## Contracts from this exact implementation

Line references here refer to the unchanged baseline SDK implementation.

| API | Verified local behavior |
|---|---|
| `PA_CreateUnistring`, `PA_DisposeUnistring`, `PA_SetUnistring` (244-270) | Engine-backed create returns a descriptor; dispose forwards it then zeros the caller descriptor; content setter forwards old descriptor plus UTF-16 source and writes back the result. Shim models successful independent content allocation, allocated/null empties and matching disposal. It does not certify host OOM behavior. |
| `PA_ReturnString` (2260), C_TEXT outputs | Calls `PA_SetUnistring`, not the array/scalar shallow assignment. Tests keep output alive after wrapper destruction, input mutation, autorelease pool drain and allocator churn. |
| `PA_CreateVariable` (3523 onward) | Unistring and picture kinds create real initial empty owners. Overwriting them with shallow setters leaks them. Use content setter on the existing string; use Undefined for picture arguments immediately assigned a known owner/borrow. Output-only picture arguments retain their initial picture owner until the synchronous host procedure replaces or caller clears it. |
| `PA_SetStringVariable` (3826), `PA_SetPictureVariable` (3853) | Shallow assigns without disposing previous content. Caller tracks whether argument is owning or borrowing. Independent synchronous callback strings may be cleared once after the call. |
| `PA_GetStringInArray` (4266), `PA_SetStringInArray` (4444) | Getter aliases array-owned storage. Setter requires Unicode array, non-null handle, index `>= 0` and `<= fNbElements`, non-null source descriptor pointer. Accepted setter disposes old element then shallow assigns. Rejected setters do not consume the source. Index zero is accepted. No post-transfer disposal; copy borrowed getters before replacement. |
| `PA_ResizeArray` (3973) | Shrink disposes removed strings, successful grow zero-initializes new descriptors. It updates count even after failed handle resize: normal tests do not certify resize OOM. |
| Picture create/dispose/duplicate (380-455) | Retain flag 1 maps to engine duplicate param 0; independently retained references need independent disposal. `setPicture` retains. `setBytes` must release its created temporary; file output must be retained before clearing original argument. Returned copy/gray/thumbnail pictures remain owning handoffs. Borrowed size/thumbnail inputs are detached before clearing args. |
| `PA_ConvertCharsetToCharset` (4776) | Explicitly returns bytes, forwarded in `fParam3`; UTF-16 units are returned bytes divided by `sizeof(PA_Unichar)`. Test uses actual byte capacity, reserves/verifies terminator space, and preserves supplementary surrogate pairs. |
| `PA_ExecuteCommandByID` (5257) | Synchronous args passed as addresses; successful procedures in use here have no owning function result. Handle-allocation failure leaves raw `fType == 0`, which `PA_GetVariableKind` normalizes as string, not Undefined. Never inspect/adopt/clear that failed sentinel. Method allocation failure is separately Undefined. |
| `PA_GetPictureData` (435) | Error set before caller uses descriptor. Failed poisoned output is not inspected/cleared. Successful returned-format string ownership is not stated by local source/header and remains deferred. |
| `PA_GetPointerValue` (2035), header 409 | Header asks caller to clear; engine returns via pointer, while C_POINTER copies definition metadata rather than normal values. Without engine/reference-variable evidence it is unsafe to add cleanup or relabel borrowed data. Deferred with this evidence. |

## Confirmed fix and differential regression

Production changes are confined to `Classes/C_PICTURE.cpp`:
single path-string cleanup using an existing owner's content setter; retained
file output followed by original argument cleanup; release of setBytes
temporary; no overwritten initial string/picture owners; UTF-16 length units.
No new command, owner-returning procedure result, SDK change or release occurs.

The runner compiles all 16 actual wrappers, the complete unchanged SDK, and
the production dispatcher body. Before compilation it replaces **both**
library/Mail discovery functions in a temporary source copy: library discovery
aborts, and database discovery returns only the explicit synthetic fixture.
No real discovery implementation is present in the executable. Every
unexpected engine selector fails closed. Picture commands are shim callbacks,
not actual 4D commands or file access. Only the test's newly created SQLite
fixture is opened. No actual Mail paths, databases, messages, credentials,
accounts, address books, AppleEvents, AppleScript or UI are accessed.

ASan and UBSan run with explicit counters for strings, picture reference
counts and handles. macOS LeakSanitizer is unavailable, so leak detection uses
these counters; alignment checking is disabled for this SDK's packed ABI.
Tests cover ASCII, BMP, supplementary and empty text; allocated and null empty
representations; exact UTF-8 bytes and UTF-16 content/units; converter byte
capacity and terminator; independent outputs; borrowed input copies;
replacement/shrink/clear; every setter rejection predicate; synchronous
callback independence; picture handoff survival; poisoned failed picture-data
outputs; command allocation failure/raw sentinel versus method Undefined;
scalar content setters; and the actual dispatcher with synthetic SQLite.

Run the final harness against an immutable `git archive` of the baseline
native sources using `OWNERSHIP_SOURCE_ROOT`; keep the same final test driver
and shim. The baseline produces 21 failing invocations of 33 across 11 test
methods: path duplicate disposal (including handle failure), copy ASan
heap-buffer-overflow for `.png`, copy initial string/picture leaks, grayscale,
size, thumbnail and setBytes picture leaks. Arrays, scalar content controls
and synthetic dispatcher cases pass unchanged. The fixed source passes all
33 invocations with `strings=0 pictures=0 handles=0` after each.

## Deliberate limits and deferrals

- No actual 4D host or Windows execution, no Windows crash certification, no
  real Mail data or platform discovery tests.
- Initial engine string allocation failure and array allocation/resize failure
  are **not fixed or certified**. The SDK's create/string setter functions do
  not expose a reliable checked error channel here, and ARRAY_TEXT's void
  output boundary cannot safely publish a guessed descriptor or discard
  original caller bytes. No silent fallback or success-shaped OOM is added.
  Existing allocation-failure behavior is deferred pending a defined engine
  contract and explicit failure/preservation API; normal arrays are not OOM
  evidence.
- Picture/CBytes allocation failure and non-string buffer correctness remain
  out of scope. Only command argument-handle allocation failure is exercised.
- Successful SDK-only application/structure/preferences/folder, dialog,
  localization and picture-format strings require engine ownership evidence;
  no speculative disposal is added. Successful SDK JSON property/element
  owning returns and JSONParse input contracts are not present in this SDK
  or first-party native call graph.
- C_POINTER metadata requires reference-variable evidence before cleanup can
  be changed. No clear-then-relabel-borrowed workaround is used.
