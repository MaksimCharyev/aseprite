// Aseprite
// Copyright (C) 2026  Maksim Charyev
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_FILE_ANDROID_EXPORTS_H_INCLUDED
#define APP_FILE_ANDROID_EXPORTS_H_INCLUDED
#pragma once

#if LAF_ANDROID

  #include "base/fs.h"
  #include "os/android/android_app.h"

  #include <string>

namespace app {

// On Android exports always go to the public "Download/Aseprite"
// folder (ADR 0010): they are written to this folder in the app cache
// and FileOp publishes each written file to MediaStore.Downloads
// (os::android_publish_download()).
inline std::string android_exports_path()
{
  return base::join_path(os::android_downloads_cache_path(), "Aseprite");
}

// "Download/Aseprite/<name>" for a file name or a path in
// android_exports_path(), as the user sees it in the Files app.
inline std::string android_export_display_name(const std::string& filename)
{
  return "Download/Aseprite/" + base::get_file_name(filename);
}

// True if "filename" is a file in android_exports_path().
inline bool is_android_export(const std::string& filename)
{
  return !filename.empty() && base::normalize_path(base::get_file_path(filename)) ==
                                base::normalize_path(android_exports_path());
}

} // namespace app

#endif // LAF_ANDROID

#endif
