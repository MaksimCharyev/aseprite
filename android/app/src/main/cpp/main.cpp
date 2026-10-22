// Aseprite
// Copyright (C) 2026  Maksim Charyev
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

// M0.5 "hello" stub: logs what the device reports and clears the window to
// a solid colour with EGL + glClear each frame. Replaced by the laf Android
// backend in M1.

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/configuration.h>
#include <android/log.h>
#include <android/native_window.h>
#include <jni.h>

#include <game-activity/GameActivity.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

#include <cstdint>
#include <ctime>

#define LOG_TAG "Aseprite"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

struct State {
  EGLDisplay display = EGL_NO_DISPLAY;
  EGLContext context = EGL_NO_CONTEXT;
  EGLSurface surface = EGL_NO_SURFACE;
  bool resumed = false;
  bool glInfoLogged = false;
  int64_t fpsStartNs = 0;
  int fpsFrames = 0;
  int64_t totalFrames = 0;
};

int64_t nowNs()
{
  timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return int64_t(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
}

// Display.getRefreshRate() through JNI; 0 if it cannot be obtained.
float getRefreshRate(android_app* app)
{
  JNIEnv* env = nullptr;
  JavaVM* vm = app->activity->vm;
  if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env)
    return 0.0f;

  float rate = 0.0f;
  jobject activity = app->activity->javaGameActivity;
  jclass activityClass = env->GetObjectClass(activity);
  jmethodID getWM = env->GetMethodID(activityClass, "getWindowManager",
                                     "()Landroid/view/WindowManager;");
  jobject wm = getWM ? env->CallObjectMethod(activity, getWM) : nullptr;
  if (wm) {
    jclass wmClass = env->GetObjectClass(wm);
    jmethodID getDisplay = env->GetMethodID(wmClass, "getDefaultDisplay",
                                            "()Landroid/view/Display;");
    jobject display = getDisplay ? env->CallObjectMethod(wm, getDisplay) : nullptr;
    if (display) {
      jclass displayClass = env->GetObjectClass(display);
      jmethodID getRate = env->GetMethodID(displayClass, "getRefreshRate", "()F");
      if (getRate)
        rate = env->CallFloatMethod(display, getRate);
      env->DeleteLocalRef(displayClass);
      env->DeleteLocalRef(display);
    }
    env->DeleteLocalRef(wmClass);
    env->DeleteLocalRef(wm);
  }
  env->DeleteLocalRef(activityClass);
  if (env->ExceptionCheck())
    env->ExceptionClear();
  return rate;
}

void logConfiguration(android_app* app, const char* what)
{
  AConfiguration* cfg = app->config;
  LOGI("%s: sdk=%d density=%d orientation=%d screenWidthDp=%d screenHeightDp=%d "
       "smallestScreenWidthDp=%d keyboard=%d navigation=%d uiModeType=%d",
       what,
       AConfiguration_getSdkVersion(cfg),
       AConfiguration_getDensity(cfg),
       AConfiguration_getOrientation(cfg),
       AConfiguration_getScreenWidthDp(cfg),
       AConfiguration_getScreenHeightDp(cfg),
       AConfiguration_getSmallestScreenWidthDp(cfg),
       AConfiguration_getKeyboard(cfg),
       AConfiguration_getNavigation(cfg),
       AConfiguration_getUiModeType(cfg));
}

void termSurface(State& st)
{
  if (st.display == EGL_NO_DISPLAY)
    return;

  eglMakeCurrent(st.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  if (st.surface != EGL_NO_SURFACE)
    eglDestroySurface(st.display, st.surface);
  if (st.context != EGL_NO_CONTEXT)
    eglDestroyContext(st.display, st.context);
  eglTerminate(st.display);

  st.display = EGL_NO_DISPLAY;
  st.context = EGL_NO_CONTEXT;
  st.surface = EGL_NO_SURFACE;
  LOGI("EGL surface and context destroyed");
}

bool initSurface(android_app* app, State& st)
{
  ANativeWindow* window = app->window;
  if (!window)
    return false;

  st.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (st.display == EGL_NO_DISPLAY || !eglInitialize(st.display, nullptr, nullptr)) {
    LOGE("eglInitialize failed: 0x%x", eglGetError());
    st.display = EGL_NO_DISPLAY;
    return false;
  }

  const EGLint cfgAttribs[] = {
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 8,
    EGL_DEPTH_SIZE, 0,
    EGL_STENCIL_SIZE, 8,
    EGL_NONE
  };
  EGLConfig config = nullptr;
  EGLint numConfigs = 0;
  if (!eglChooseConfig(st.display, cfgAttribs, &config, 1, &numConfigs) || numConfigs < 1) {
    LOGE("eglChooseConfig failed: 0x%x (configs=%d)", eglGetError(), numConfigs);
    termSurface(st);
    return false;
  }

  EGLint format = 0;
  eglGetConfigAttrib(st.display, config, EGL_NATIVE_VISUAL_ID, &format);
  ANativeWindow_setBuffersGeometry(window, 0, 0, format);

  const EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
  st.context = eglCreateContext(st.display, config, EGL_NO_CONTEXT, ctxAttribs);
  st.surface = eglCreateWindowSurface(st.display, config, window, nullptr);
  if (st.context == EGL_NO_CONTEXT || st.surface == EGL_NO_SURFACE) {
    LOGE("EGL context/surface creation failed: 0x%x", eglGetError());
    termSurface(st);
    return false;
  }
  if (!eglMakeCurrent(st.display, st.surface, st.surface, st.context)) {
    LOGE("eglMakeCurrent failed: 0x%x", eglGetError());
    termSurface(st);
    return false;
  }
  eglSwapInterval(st.display, 1);

  EGLint w = 0, h = 0;
  eglQuerySurface(st.display, st.surface, EGL_WIDTH, &w);
  eglQuerySurface(st.display, st.surface, EGL_HEIGHT, &h);
  LOGI("window: %dx%d (ANativeWindow %dx%d) density=%d refresh=%.2f Hz",
       w, h,
       ANativeWindow_getWidth(window), ANativeWindow_getHeight(window),
       AConfiguration_getDensity(app->config),
       getRefreshRate(app));
  LOGI("GL_RENDERER: %s", (const char*)glGetString(GL_RENDERER));
  LOGI("GL_VENDOR:   %s", (const char*)glGetString(GL_VENDOR));
  LOGI("GL_VERSION:  %s", (const char*)glGetString(GL_VERSION));
  LOGI("GLSL:        %s", (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));
  st.glInfoLogged = true;
  st.fpsStartNs = nowNs();
  st.fpsFrames = 0;
  return true;
}

void drawFrame(State& st)
{
  if (st.surface == EGL_NO_SURFACE)
    return;

  glClearColor(0.16f, 0.45f, 0.62f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  if (!eglSwapBuffers(st.display, st.surface)) {
    EGLint err = eglGetError();
    LOGW("eglSwapBuffers failed: 0x%x", err);
    // The surface is gone or the context was lost; wait for the next
    // APP_CMD_INIT_WINDOW instead of spinning.
    if (err == EGL_BAD_SURFACE || err == EGL_BAD_NATIVE_WINDOW || err == EGL_CONTEXT_LOST)
      termSurface(st);
    return;
  }

  ++st.totalFrames;
  ++st.fpsFrames;
  int64_t now = nowNs();
  if (now - st.fpsStartNs >= 2000000000LL) {
    double secs = double(now - st.fpsStartNs) / 1e9;
    LOGI("%d frames in %.2f s = %.1f fps (total %lld)",
         st.fpsFrames, secs, st.fpsFrames / secs, (long long)st.totalFrames);
    st.fpsStartNs = now;
    st.fpsFrames = 0;
  }
}

void onAppCmd(android_app* app, int32_t cmd)
{
  State* st = static_cast<State*>(app->userData);
  switch (cmd) {
    case APP_CMD_START:         LOGI("APP_CMD_START"); break;
    case APP_CMD_RESUME:
      LOGI("APP_CMD_RESUME");
      st->resumed = true;
      st->fpsStartNs = nowNs();
      st->fpsFrames = 0;
      break;
    case APP_CMD_PAUSE:
      LOGI("APP_CMD_PAUSE");
      st->resumed = false;
      break;
    case APP_CMD_STOP:          LOGI("APP_CMD_STOP"); break;
    case APP_CMD_DESTROY:       LOGI("APP_CMD_DESTROY"); break;
    case APP_CMD_GAINED_FOCUS:  LOGI("APP_CMD_GAINED_FOCUS"); break;
    case APP_CMD_LOST_FOCUS:    LOGI("APP_CMD_LOST_FOCUS"); break;
    case APP_CMD_LOW_MEMORY:    LOGW("APP_CMD_LOW_MEMORY"); break;
    case APP_CMD_SAVE_STATE:    LOGI("APP_CMD_SAVE_STATE"); break;

    case APP_CMD_INIT_WINDOW:
      LOGI("APP_CMD_INIT_WINDOW");
      termSurface(*st);         // defensive, no-op normally
      initSurface(app, *st);
      break;

    case APP_CMD_TERM_WINDOW:
      // Must release the surface before returning: the Java side blocks
      // until this command is processed.
      LOGI("APP_CMD_TERM_WINDOW");
      termSurface(*st);
      break;

    case APP_CMD_WINDOW_RESIZED:
      if (app->window)
        LOGI("APP_CMD_WINDOW_RESIZED: window=%dx%d",
             ANativeWindow_getWidth(app->window), ANativeWindow_getHeight(app->window));
      break;

    case APP_CMD_CONFIG_CHANGED:
      logConfiguration(app, "APP_CMD_CONFIG_CHANGED");
      break;

    case APP_CMD_CONTENT_RECT_CHANGED:
      LOGI("APP_CMD_CONTENT_RECT_CHANGED: %d,%d - %d,%d",
           app->contentRect.left, app->contentRect.top,
           app->contentRect.right, app->contentRect.bottom);
      break;

    case APP_CMD_WINDOW_INSETS_CHANGED:
      LOGI("APP_CMD_WINDOW_INSETS_CHANGED");
      break;

    default:
      LOGI("APP_CMD %d", cmd);
      break;
  }
}

// Input is not handled in M0.5 (M0.6 adds the probe), but the buffers must be
// drained or the glue keeps them around.
void drainInput(android_app* app)
{
  android_input_buffer* ib = android_app_swap_input_buffers(app);
  if (!ib)
    return;
  if (ib->motionEventsCount > 0)
    LOGI("input: %u motion events (ignored)", (unsigned)ib->motionEventsCount);
  if (ib->keyEventsCount > 0)
    LOGI("input: %u key events (ignored)", (unsigned)ib->keyEventsCount);
  android_app_clear_motion_events(ib);
  android_app_clear_key_events(ib);
}

} // anonymous namespace

extern "C" void android_main(android_app* app)
{
  LOGI("android_main: start");

  State state;
  app->userData = &state;
  app->onAppCmd = onAppCmd;
  logConfiguration(app, "startup");

  for (;;) {
    // Block while there is nothing to draw, otherwise only poll.
    int timeoutMs = (state.resumed && state.surface != EGL_NO_SURFACE ? 0 : -1);

    for (;;) {
      int events = 0;
      android_poll_source* source = nullptr;
      int id = ALooper_pollOnce(timeoutMs, nullptr, &events, (void**)&source);
      if (id == ALOOPER_POLL_TIMEOUT || id == ALOOPER_POLL_ERROR)
        break;
      if (source)
        source->process(app, source);
      if (app->destroyRequested)
        break;
      timeoutMs = 0;
    }

    if (app->destroyRequested) {
      termSurface(state);
      LOGI("android_main: exit after %lld frames", (long long)state.totalFrames);
      return;
    }

    drainInput(app);
    if (state.resumed)
      drawFrame(state);
  }
}
