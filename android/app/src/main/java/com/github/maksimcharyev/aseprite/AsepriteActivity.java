// Aseprite
// Copyright (C) 2026  Maksim Charyev
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

package com.github.maksimcharyev.aseprite;

import android.os.Bundle;
import android.view.View;

import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;

import com.google.androidgamesdk.GameActivity;

public class AsepriteActivity extends GameActivity {
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
