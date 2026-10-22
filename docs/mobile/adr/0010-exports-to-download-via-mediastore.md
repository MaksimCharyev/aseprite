# ADR 0010: Exports go to Download/Aseprite via MediaStore

- Status: Accepted
- Date: 2026-10-22
- Owner: mobile-laf-backend (laf `os/android`, Java), app hooks
  reviewed by mobile-touch-ux
- Amends: ADR 0009 (Export no longer goes through the system picker)

## Context

Tablet feedback (2026-10-22): File > Export As → "…" → system picker
created an **empty** `Sprite-0001-export.png` in Download and nothing
was written to it; the user also could not tell where an export goes
and whether the export options survive picking a folder. Request:
"Export must always save into the Downloads folder and must work."

Root cause of the empty file: the picker returned the cache path
`<cache>/documents/<hash B>/Sprite-0001-export.png` (ADR 0009), but the
Export window's `FilenameField` doesn't keep that path. With
`general.editFullPath = false` (the default) it stores the sprite
folder plus a path *relative* to it. The sprite had been saved through
the picker too, so its folder is `<cache>/documents/<hash A>`, and
`fullFilename()` returned
`<cache>/documents/<hash A>/../<hash B>/Sprite-0001-export.png`.
`FileOp` wrote that file (it resolves to the right cache file), but
`android_document_to_provider()` compared the string with the
registered path exactly, found nothing, and skipped the copy to the
URI. Save/Save As don't go through `FilenameField`, so they worked.

## Decision

1. **On Android, exports always go to `Download/Aseprite/`** in the
   public Downloads collection, through `MediaStore.Downloads`
   (API 29+, our minSdk). The app owns the entries it inserts, so no
   permission is needed (no `WRITE_EXTERNAL_STORAGE`, the manifest
   still has no storage/INTERNET permission).
2. **Cache mirror + publish hook** (same idea as ADR 0009): Aseprite
   writes exports to `<cacheDir>/downloads/Aseprite/<name>`
   (`os::android_downloads_cache_path()` + `"Aseprite"`,
   `app::android_exports_path()` in `src/app/file/android_exports.h`).
   After each file is written, `os::android_publish_download(path)`
   (laf `os/android/downloads.cpp`) maps
   `<cacheDir>/downloads/<dir>/<name>` to RELATIVE_PATH
   `Download/<dir>/` + DISPLAY_NAME `<name>` and calls
   `AsepriteActivity.publishDownload()` (JNI, method id cached,
   `ExceptionCheck` after every call, `JniThreadEnv` on the save worker
   thread):
   - query our entry by `RELATIVE_PATH` + `DISPLAY_NAME` (pending
     entries included on API 30+, `QUERY_ARG_MATCH_PENDING`);
   - found: `IS_PENDING=1`, write with `openOutputStream(uri, "wt")`,
     `IS_PENDING=0` → re-exporting **replaces** the file instead of
     creating `name (1).png`;
   - not found: insert `DISPLAY_NAME`, `MIME_TYPE` (from the extension
     with `MimeTypeMap`, `application/octet-stream` when unknown, which
     keeps the name as typed), `RELATIVE_PATH`, `IS_PENDING=1`; write;
     `IS_PENDING=0`. A failed write deletes the new entry.
   - The cache file is deleted after a successful publish.
3. **Hooks** (all `#if LAF_ANDROID`):
   - `FileOp::saveToAndroidDocument()` (`src/app/file/file.cpp`), already
     called after every saved file (ADR 0009), now also calls
     `android_publish_download()`. This covers File > Export As (single
     file, frame sequences `name01.png`…, GIF/WebP animations) and the
     sprite sheet image (written with `save_document()`).
   - `DocExporter::exportSheet()` (`src/app/doc_exporter.cpp`) closes and
     publishes the sprite sheet JSON data file (written with
     `std::ofstream`, not `FileOp`).
4. **UI**:
   - Export window: `FilenameField::setFixedPath()` (Android only) — the
     entry edits only the file name (the extension selects the format,
     a typed `/` becomes `_`), the "…" button is hidden; a new label
     "Saved to: Download/Aseprite/<name>" (`saved_to` in
     `export_file.xml`, hidden on desktop) follows the name. All other
     options and `saveCopy.*` doc prefs work as before (`saveCopy.filename`
     now holds the cache path, only its name is used).
   - Export Sprite Sheet: both output fields get the fixed folder.
   - After an export: status bar tip "Exported to Download/Aseprite/<name>"
     (3 s), for sequences "… <first> ... <last>", for sprite sheets the
     image and data names.
   - Exports are not added to the recent files (their cache copy is
     deleted).
5. **Robustness fix in ADR 0009's registry**: `findByPath()` also
   matches the normalized path (`base::normalize_path`), so a document
   path written in another form is still copied back.

## Alternatives

- **Keep the SAF picker for Export** and only fix the path comparison.
  The fix is included (5), but the user explicitly wants a fixed,
  predictable place, and sequences/sprite-sheet side files can't go
  through a single picked URI (ADR 0009 known limitation).
- **`ACTION_OPEN_DOCUMENT_TREE`** on Download and write with
  `DocumentsContract`: one more picker the first time, a persisted tree
  grant to manage, and Download's root can't be picked as a tree on
  API 30+.
- **`WRITE_EXTERNAL_STORAGE` / `MANAGE_EXTERNAL_STORAGE`** and real
  paths: new permissions (ADR 0004 says none); `WRITE_EXTERNAL_STORAGE`
  has no effect for targetSdk ≥ 30 anyway.
- **App-specific external dir** (`Android/data/<pkg>/files`): invisible
  in the Files app on HarmonyOS/Android 11+ and removed on uninstall.

## Consequences

- No folder choice for exports on Android (by design). Users who want
  another place use Save As (picker) or move the file in the Files app.
- Overwrite only works for entries **owned by this app**: a file with the
  same name created by another app, by the Files app, or by a previous
  installation of Aseprite (uninstall drops ownership) is not visible
  to our query; MediaStore then inserts `name (1).png`.
- The "overwrite files?" alert of Export (desktop behaviour) doesn't
  appear on Android: the cache copy no longer exists, re-exporting
  replaces silently. It appears only if the previous publish failed and
  left the cache file.
- `.aseprite-data` side files are not published (FileOp writes one only
  if it already exists next to the output, which never happens in the
  export cache folder).
- Scripts/CLI that call `SaveFileCopyAs` with `ui=false` and an explicit
  path write exactly there (no publishing), as before.
- Images in `Download/Aseprite` are indexed by MediaStore, so Gallery
  apps show them.
