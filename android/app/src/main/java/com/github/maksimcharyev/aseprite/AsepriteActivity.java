// Aseprite
// Copyright (C) 2026  Maksim Charyev
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

package com.github.maksimcharyev.aseprite;

import android.os.Bundle;
import android.util.Log;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;

import com.google.androidgamesdk.GameActivity;

public class AsepriteActivity extends GameActivity {
  // M0.6 input probe (only the `probe` native variant logs keys): same tag as the native probe in main.cpp.
  private static final String PROBE_TAG = "AsepriteProbe";

  static {
    System.loadLibrary("aseprite");
  }

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    // Draw edge to edge, we handle the insets ourselves.
    WindowCompat.setDecorFitsSystemWindows(getWindow(), false);
    super.onCreate(savedInstanceState);
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
