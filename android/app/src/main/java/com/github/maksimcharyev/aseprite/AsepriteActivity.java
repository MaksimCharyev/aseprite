// Aseprite
// Copyright (C) 2026  Maksim Charyev
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

package com.github.maksimcharyev.aseprite;

import android.os.Build;
import android.os.Bundle;
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

public class AsepriteActivity extends GameActivity {
  // M0.6 input probe (only the `probe` native variant logs keys): same tag as the native probe in main.cpp.
  private static final String PROBE_TAG = "AsepriteProbe";
  // Same tag as the native code in main.cpp.
  private static final String TAG = "Aseprite";

  // Highest refresh rate we ask for (the panel goes up to 144 Hz). The
  // "debug.laf.frame_rate" system property (also read by laf) overrides
  // it, 0 = leave the display mode to the system.
  private static final float DEFAULT_FRAME_RATE = 144.0f;

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
