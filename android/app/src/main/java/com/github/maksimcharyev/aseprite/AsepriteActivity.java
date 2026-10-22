// Aseprite
// Copyright (C) 2026  Maksim Charyev
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

package com.github.maksimcharyev.aseprite;

import android.content.ActivityNotFoundException;
import android.content.ClipData;
import android.content.ContentResolver;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.provider.OpenableColumns;
import android.util.Log;
import android.view.Display;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;

import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;

import com.google.androidgamesdk.GameActivity;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;

public class AsepriteActivity extends GameActivity {
  // M0.6 input probe (only the `probe` native variant logs keys): same tag as the native probe in main.cpp.
  private static final String PROBE_TAG = "AsepriteProbe";
  // Same tag as the native code in main.cpp.
  private static final String TAG = "Aseprite";

  // Highest refresh rate we ask for (the panel goes up to 144 Hz). The
  // "debug.laf.frame_rate" system property (also read by laf) overrides
  // it, 0 = leave the display mode to the system.
  private static final float DEFAULT_FRAME_RATE = 144.0f;

  // Same tag as laf's os/android logs.
  private static final String TAG = "laf";

  // System document picker (Storage Access Framework, ADR 0009), used by
  // laf's os/android/documents.cpp. Picker types and results must match
  // the enums there.
  private static final int PICKER_OPEN = 0;
  private static final int PICKER_OPEN_MULTIPLE = 1;
  private static final int PICKER_SAVE = 2;
  private static final int PICK_ERROR = -1;
  private static final int PICK_CANCEL = 0;
  private static final int PICK_OK = 1;
  private static final int REQUEST_DOCUMENTS = 0x5af;

  // Native request id of the picker in front (UI thread only).
  private int mDocumentRequestId = 0;

  // Registered by laf with RegisterNatives() before the first picker.
  private native void nativeOnDocumentsPicked(int requestId, int result,
                                              String[] uris, String[] names);

  static {
    System.loadLibrary("aseprite");
  }

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    // Draw edge to edge, we handle the insets ourselves.
    WindowCompat.setDecorFitsSystemWindows(getWindow(), false);
    super.onCreate(savedInstanceState);
    requestHighRefreshRate();
  }

  // HarmonyOS runs the panel at 60 Hz when the app renders on demand.
  // laf votes for 144 Hz with ANativeWindow_setFrameRate(); as a
  // fallback (in case the system ignores the vote) we also prefer the
  // display mode with the highest refresh rate <= the requested one and
  // the current resolution. It only applies while this activity is in
  // the foreground.
  private void requestHighRefreshRate() {
    float maxRate = frameRateProperty();
    if (maxRate <= 0.0f) {
      Log.i(TAG, "Display mode: not requested (debug.laf.frame_rate=0)");
      return;
    }

    Display display = (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R ?
                       getDisplay() : getWindowManager().getDefaultDisplay());
    if (display == null)
      return;

    Display.Mode current = display.getMode();
    Display.Mode best = null;
    StringBuilder modes = new StringBuilder();
    for (Display.Mode mode : display.getSupportedModes()) {
      modes.append(' ').append(mode.getModeId()).append(':')
           .append(mode.getPhysicalWidth()).append('x').append(mode.getPhysicalHeight())
           .append('@').append(Math.round(mode.getRefreshRate()));
      if (mode.getPhysicalWidth() != current.getPhysicalWidth() ||
          mode.getPhysicalHeight() != current.getPhysicalHeight() ||
          mode.getRefreshRate() > maxRate + 0.5f)
        continue;
      if (best == null || mode.getRefreshRate() > best.getRefreshRate())
        best = mode;
    }
    Log.i(TAG, "Display modes:" + modes + ", current " + current.getModeId() +
          " @" + Math.round(current.getRefreshRate()));
    if (best == null)
      return;

    WindowManager.LayoutParams params = getWindow().getAttributes();
    params.preferredDisplayModeId = best.getModeId();
    getWindow().setAttributes(params);
    Log.i(TAG, "Display mode: preferred mode " + best.getModeId() + " (" +
          best.getPhysicalWidth() + "x" + best.getPhysicalHeight() + " @" +
          Math.round(best.getRefreshRate()) + " Hz)");
  }

  // android.os.SystemProperties is hidden API, read it with reflection
  // (only a testing aid, any failure means "use the default").
  private static float frameRateProperty() {
    try {
      Class<?> props = Class.forName("android.os.SystemProperties");
      String value = (String)props.getMethod("get", String.class, String.class)
        .invoke(null, "debug.laf.frame_rate", "");
      if (value != null && !value.isEmpty())
        return Float.parseFloat(value);
    }
    catch (Exception e) {
      // Use the default
    }
    return DEFAULT_FRAME_RATE;
  }

  @Override
  protected void onCreateSurfaceView() {
    super.onCreateSurfaceView();

    // Hover events go through View.dispatchHoverEvent() first; GameActivity
    // only installs touch/generic-motion listeners and relies on the hover
    // falling through to the generic listener. Forward them explicitly so
    // ACTION_HOVER_* always reaches native code (exactly once: returning
    // true stops the fall-through).
    if (mSurfaceView != null) {
      mSurfaceView.setOnHoverListener((View v, MotionEvent event) -> {
        int action = event.getActionMasked();
        if (BuildConfig.INPUT_PROBE &&
            (action == MotionEvent.ACTION_HOVER_ENTER ||
             action == MotionEvent.ACTION_HOVER_EXIT)) {
          Log.i(PROBE_TAG, "java hover " + MotionEvent.actionToString(action) +
                " tool=" + event.getToolType(0) +
                " source=0x" + Integer.toHexString(event.getSource()) +
                " device=" + event.getDeviceId());
        }
        processMotionEvent(event);
        return true;
      });
    }
  }

  // Probe variant only (BuildConfig.INPUT_PROBE). Logs every key event that
  // reaches the activity, before GameActivity forwards it to native code, so
  // we can tell whether a key (e.g. the M-Pencil double-tap, evdev KEY_F20 /
  // scan 190) arrives at all.
  @Override
  public boolean dispatchKeyEvent(KeyEvent event) {
    if (!BuildConfig.INPUT_PROBE)
      return super.dispatchKeyEvent(event);

    InputDevice device = event.getDevice();
    Log.i(PROBE_TAG, "java key action=" + event.getAction() +
          " keyCode=" + event.getKeyCode() +
          " (" + KeyEvent.keyCodeToString(event.getKeyCode()) + ")" +
          " scanCode=" + event.getScanCode() +
          " metaState=0x" + Integer.toHexString(event.getMetaState()) +
          " repeatCount=" + event.getRepeatCount() +
          " flags=0x" + Integer.toHexString(event.getFlags()) +
          " source=0x" + Integer.toHexString(event.getSource()) +
          " device=" + event.getDeviceId() +
          " name=\"" + (device != null ? device.getName() : "?") + "\"");
    return super.dispatchKeyEvent(event);
  }

  // ---- Storage Access Framework (called from native code) ----

  // Called from the native glue thread; the intent is started on the UI
  // thread and the result is delivered with nativeOnDocumentsPicked().
  public void startDocumentPicker(final int requestId, final int type,
                                  final String mimeType, final String suggestedName,
                                  final String initialUri) {
    runOnUiThread(() -> {
      Intent intent;
      if (type == PICKER_SAVE) {
        intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.setType(mimeType);
        if (suggestedName != null)
          intent.putExtra(Intent.EXTRA_TITLE, suggestedName);
      }
      else {
        intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        // .aseprite files have no MIME type, show everything.
        intent.setType("*/*");
        if (type == PICKER_OPEN_MULTIPLE)
          intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
      }
      intent.addCategory(Intent.CATEGORY_OPENABLE);
      intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION |
                      Intent.FLAG_GRANT_WRITE_URI_PERMISSION |
                      Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
      if (initialUri != null) {
        try {
          intent.putExtra(DocumentsContract.EXTRA_INITIAL_URI, Uri.parse(initialUri));
        }
        catch (Exception e) {
          Log.w(TAG, "documents: invalid initial URI " + initialUri, e);
        }
      }

      mDocumentRequestId = requestId;
      try {
        startActivityForResult(intent, REQUEST_DOCUMENTS);
      }
      catch (ActivityNotFoundException | SecurityException e) {
        Log.e(TAG, "documents: cannot start " + intent.getAction(), e);
        mDocumentRequestId = 0;
        nativeOnDocumentsPicked(requestId, PICK_ERROR, null, null);
      }
    });
  }

  @Override
  protected void onActivityResult(int requestCode, int resultCode, Intent data) {
    if (requestCode != REQUEST_DOCUMENTS) {
      super.onActivityResult(requestCode, resultCode, data);
      return;
    }

    final int requestId = mDocumentRequestId;
    mDocumentRequestId = 0;

    ArrayList<Uri> uris = new ArrayList<>();
    if (resultCode == RESULT_OK && data != null) {
      ClipData clip = data.getClipData();
      if (clip != null) {
        for (int i = 0; i < clip.getItemCount(); ++i) {
          Uri uri = clip.getItemAt(i).getUri();
          if (uri != null)
            uris.add(uri);
        }
      }
      else if (data.getData() != null) {
        uris.add(data.getData());
      }
    }
    if (uris.isEmpty()) {
      nativeOnDocumentsPicked(requestId, PICK_CANCEL, null, null);
      return;
    }

    final int grantFlags = data.getFlags() & (Intent.FLAG_GRANT_READ_URI_PERMISSION |
                                              Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
    String[] uriStrings = new String[uris.size()];
    String[] names = new String[uris.size()];
    for (int i = 0; i < uris.size(); ++i) {
      Uri uri = uris.get(i);
      // Keep the access after a restart (recent files, Save).
      try {
        getContentResolver().takePersistableUriPermission(uri, grantFlags);
      }
      catch (Exception e) {
        Log.w(TAG, "documents: cannot persist the permission of " + uri + ": " + e);
      }
      uriStrings[i] = uri.toString();
      names[i] = queryDisplayName(uri);
    }
    nativeOnDocumentsPicked(requestId, PICK_OK, uriStrings, names);
  }

  private String queryDisplayName(Uri uri) {
    try (Cursor c = getContentResolver().query(
           uri, new String[] { OpenableColumns.DISPLAY_NAME }, null, null, null)) {
      if (c != null && c.moveToFirst() && !c.isNull(0))
        return c.getString(0);
    }
    catch (Exception e) {
      Log.w(TAG, "documents: cannot query the name of " + uri + ": " + e);
    }
    String name = uri.getLastPathSegment();
    if (name != null) {
      int slash = name.lastIndexOf('/');
      if (slash >= 0)
        name = name.substring(slash + 1);
    }
    return (name != null && !name.isEmpty() ? name : "document");
  }

  private static void copyStream(InputStream in, OutputStream out) throws java.io.IOException {
    byte[] buf = new byte[64 * 1024];
    int n;
    while ((n = in.read(buf)) > 0)
      out.write(buf, 0, n);
  }

  private static String errorMessage(Exception e) {
    String msg = e.getMessage();
    return e.getClass().getSimpleName() + (msg != null ? ": " + msg : "");
  }

  // Copies a document to a file (through "<path>.tmp", so the file is
  // never half written). Returns null or the error message. Any thread.
  public String copyDocumentToFile(String uriString, String path) {
    File dst = new File(path);
    File tmp = new File(path + ".tmp");
    try (InputStream in = getContentResolver().openInputStream(Uri.parse(uriString))) {
      if (in == null)
        return "the provider returned no stream";
      try (OutputStream out = new FileOutputStream(tmp)) {
        copyStream(in, out);
      }
      if (!tmp.renameTo(dst))
        return "cannot rename " + tmp + " to " + dst;
      return null;
    }
    catch (Exception e) {
      tmp.delete();
      return errorMessage(e);
    }
  }

  // Copies a file to a document, replacing its content. Returns null or
  // the error message. Any thread.
  public String copyFileToDocument(String path, String uriString) {
    Uri uri = Uri.parse(uriString);
    ContentResolver resolver = getContentResolver();
    String firstError;
    // "wt" truncates; plain "w" doesn't on every provider, so it's not
    // used (a shorter file would keep the old tail).
    try (InputStream in = new FileInputStream(path);
         OutputStream out = resolver.openOutputStream(uri, "wt")) {
      if (out == null)
        throw new java.io.IOException("the provider returned no stream");
      copyStream(in, out);
      return null;
    }
    catch (Exception e) {
      firstError = errorMessage(e);
    }
    // Some providers support only file descriptors.
    try (ParcelFileDescriptor pfd = resolver.openFileDescriptor(uri, "rwt");
         InputStream in = new FileInputStream(path)) {
      if (pfd == null)
        return firstError;
      try (OutputStream out = new FileOutputStream(pfd.getFileDescriptor())) {
        copyStream(in, out);
      }
      Log.i(TAG, "documents: \"wt\" failed (" + firstError + "), saved with \"rwt\"");
      return null;
    }
    catch (Exception e) {
      return firstError + "; " + errorMessage(e);
    }
  }

  // Renames a document, returns the new URI or null.
  public String renameDocument(String uriString, String displayName) {
    try {
      Uri uri = DocumentsContract.renameDocument(getContentResolver(),
                                                 Uri.parse(uriString), displayName);
      if (uri == null)
        return null;
      // The provider grants the new URI if the document id changed.
      try {
        getContentResolver().takePersistableUriPermission(
          uri, Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
      }
      catch (Exception e) {
        Log.w(TAG, "documents: cannot persist the permission of " + uri + ": " + e);
      }
      return uri.toString();
    }
    catch (Exception e) {
      Log.w(TAG, "documents: cannot rename " + uriString + " to " + displayName + ": " + e);
      return null;
    }
  }

  public void releaseDocumentPermission(String uriString) {
    try {
      getContentResolver().releasePersistableUriPermission(
        Uri.parse(uriString),
        Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
    }
    catch (Exception e) {
      Log.w(TAG, "documents: cannot release the permission of " + uriString + ": " + e);
    }
  }

  @Override
  public void onWindowFocusChanged(boolean hasFocus) {
    super.onWindowFocusChanged(hasFocus);
    if (hasFocus)
      hideSystemBars();
  }

  private void hideSystemBars() {
    View decor = getWindow().getDecorView();
    WindowInsetsControllerCompat controller =
      WindowCompat.getInsetsController(getWindow(), decor);
    controller.setSystemBarsBehavior(
      WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
    controller.hide(WindowInsetsCompat.Type.systemBars());
  }
}
