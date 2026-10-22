# ADR 0009: System file picker (SAF) — documents as cache files mapped to content URIs

- Status: Accepted (Export amended by ADR 0010: on Android exports go to
  Download/Aseprite through MediaStore, not through the picker)
- Date: 2026-10-22
- Owner: mobile-laf-backend (laf `os/android`, `dlgs`, Java), app hooks
  reviewed by mobile-touch-ux

## Context

User feedback (2026-10-22): saved files are invisible (Aseprite writes to
`$HOME` = `/data/user/0/<pkg>/files`, app-private) and the file name
cannot be typed. Android's answer without storage permissions is the
Storage Access Framework (SAF): `ACTION_OPEN_DOCUMENT` /
`ACTION_CREATE_DOCUMENT` open the system picker (any folder, Downloads,
Pictures, cloud providers; the name is typed with the system keyboard)
and return `content://` URIs. No permission is needed, the app keeps
having no `<uses-permission>` at all.

Aseprite works with file paths everywhere: `FileOp` opens files with
`base::open_file()` (`FILE*`), detects the format from the header and
the extension, writes sequences (`name01.png`…) and side files
(`.aseprite-data`), keeps the path as the document file name, in the
recent files list and in the window title. laf's `dlgs::FileDialog`
returns paths too (`laf/dlgs/file_dialog.h`).

Options:

1. **fd-based I/O**: open the URI with
   `ContentResolver.openFileDescriptor()` and give Aseprite
   `/proc/self/fd/<n>` as the path. Fragile: the fd must stay open for
   the whole load/save, the "path" is meaningless after that (Save,
   recent files, title, extension-based format detection), and
   `FileOp`'s temporary files/sequences don't work.
2. **Copy to/from a cache file** (chosen): the picked document is
   copied to a real file with the same display name in the app cache;
   Aseprite only ever sees that path; after Aseprite writes the path,
   the bytes are copied back to the URI.
3. Request `MANAGE_EXTERNAL_STORAGE` and use real paths: a new
   permission, not wanted (ADR 0004, no permissions) and not working for
   cloud providers.

## Decision

1. **laf `dlgs::FileDialog` for Android** (`laf/dlgs/file_dialog_android.cpp`,
   `FileDialog::makeAndroid()`), a thin adapter over
   `os::android_pick_documents()` (`laf/os/android/android_app.h`,
   implemented in `laf/os/android/documents.cpp`). `laf-dlgs` links
   `laf-os` on Android only. `Type::OpenFolder` returns
   `Result::Error`, so the app falls back to its own file selector.
2. **Java** (`AsepriteActivity`) starts the intents on the UI thread and
   delivers the result to native code through a JNI native method that is
   registered with `RegisterNatives()` (no exported `Java_…` symbol).
   Open: `ACTION_OPEN_DOCUMENT`, `*/*` (`.aseprite` has no MIME type),
   `EXTRA_ALLOW_MULTIPLE` for `OpenFiles`. Save: `ACTION_CREATE_DOCUMENT`
   with the suggested name in `EXTRA_TITLE` and always the MIME type
   `application/octet-stream`: with it, providers keep the typed name
   exactly (`FileUtils.splitFileName()`), so the user chooses the format
   by typing the extension, as in desktop native dialogs. A specific
   type (e.g. `image/png`) makes providers append its extension when the
   typed one differs (`foo.aseprite` → `foo.aseprite.png`). File managers
   and MediaStore derive the type from the extension anyway.
   `EXTRA_INITIAL_URI` is the URI of the current document when there is
   one, so the picker opens its folder.
   Persistable grants are taken (`takePersistableUriPermission()`), so
   Save keeps working after a restart.
3. **Modal wait on the glue thread.** `android_pick_documents()` blocks
   like the native dialogs of the other platforms, but keeps calling
   `AppAndroid::pollLooper()`: `PAUSE`, `TERM_WINDOW`, `STOP`, `START`,
   `RESUME`, `INIT_WINDOW` are processed while the picker is in front
   (the window detaches/attaches its surface as usual, events are queued
   for later). The Java callback wakes the looper (`ALooper_wake`). No
   polling timeout: the wait is woken only by commands, input or the
   result. `APP_CMD_DESTROY` cancels the wait. After the result, the wait
   continues up to 1.5 s until the activity is resumed and has a window,
   so the app paints the loaded document on a live surface.
4. **Cache file per URI.** `<cacheDir>/documents/<FNV-1a 64 of the URI>/<display name>`
   (display name with `/` replaced). Same URI → same path, so reopening
   a document that is already open is detected by Aseprite as usual.
   If a created document has no extension, the provider document is
   renamed to `<name>.<default extension>` (`DocumentsContract.renameDocument`,
   best effort) and the path always gets the extension (Aseprite picks
   the format from it).
5. **Registry URI ↔ path**, most recently used first, max 64 entries,
   persisted on every change to `<filesDir>/saf-documents.txt`
   (`uri<TAB>path` per line; the process can be SIGKILLed at any time,
   ADR 0007). An evicted entry releases its persistable grant.
6. **Copy back on save (app hook, `#if LAF_ANDROID`).**
   `FileOp::operate()` (`src/app/file/file.cpp`) calls
   `os::android_document_to_provider(path, error)` after each file it
   saved; if the path is in the registry, the file is copied to the URI
   with `ContentResolver.openOutputStream(uri, "wt")` (fallback
   `openFileDescriptor(uri, "rwt")`). A failure becomes a normal FileOp
   error (console message, the document stays modified). It runs on the
   save worker thread (JNI attaches the thread for the call). Covers
   Save, Save As, Save Copy As and Export.
7. **Refresh on open (app hook, `#if LAF_ANDROID`).**
   `FileOp::createLoadDocumentOperation()` calls
   `os::android_document_from_provider(path)` before checking that the
   file exists: if the path is in the registry and was not copied in
   this process yet (e.g. opened from the recent files list after a
   restart, or the cache was cleared), it is copied again from the URI.
   If that fails (grant lost, document deleted), the old cache copy is
   used if it exists.
8. **Default**: `experimental.useNativeFileDialog` is `true` on Android
   (`src/app/pref/preferences.cpp`); users can switch back to Aseprite's
   own selector in Edit > Preferences > Experimental.

## Consequences

- Two app hooks in `src/app/file/file.cpp`, both under `#if LAF_ANDROID`.
- Each document exists twice (provider + cache). Cache files may be
  removed by the system; the next open copies them again (7). Recent
  files whose cache copy was removed are hidden by Aseprite's recent
  list (`recent_files.cpp` checks `is_file`) until opened again from the
  picker.
- Sequences (`name01.png`…) and `.aseprite-data` side files are written
  only to the cache folder unless that exact path is in the registry
  (only the picked name is); Export of sequences through the picker is
  a known limitation.
- Changes made to the provider document by another app while it is
  open in Aseprite are not seen (the cache copy is the authority during
  the process lifetime), and Save overwrites them.
- `ACTION_CREATE_DOCUMENT` creates an empty document immediately; if
  the save is then cancelled or fails, an empty file stays in the
  chosen folder.
- A document created with an extension that Aseprite cannot write
  (e.g. `foo.txt`) fails in Aseprite as on desktop; the empty document
  stays.
