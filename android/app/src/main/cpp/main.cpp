// Aseprite
// Copyright (C) 2026  Maksim Charyev
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

// M0.5 "hello" stub: logs what the device reports and clears the window to
// a solid colour with EGL + glClear each frame. Replaced by the laf Android
// backend in M1.
//
// M0.6 input probe (always on in this stub): every motion and key event the
// glue receives is logged with the tag "AsepriteProbe", one line per event
// plus one line per pointer, and the clear colour shows the last tool type
// (see probeColor()). Input devices are described once per device id.

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/configuration.h>
#include <android/log.h>
#include <android/native_window.h>
#include <jni.h>

#include <game-activity/GameActivity.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>
#include <unordered_set>

#define LOG_TAG "Aseprite"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define PROBE_TAG "AsepriteProbe"
#define PLOG(...) __android_log_print(ANDROID_LOG_INFO, PROBE_TAG, __VA_ARGS__)

namespace {

// What the input probe remembers for the on-screen feedback.
struct Probe {
  int64_t motionSeq = 0;
  int64_t keySeq = 0;
  int32_t lastToolType = -1;   // AMOTION_EVENT_TOOL_TYPE_*, -1 = nothing yet
  bool hovering = false;
  int64_t cancelNs = 0;        // last ACTION_CANCEL
  int64_t keyNs = 0;           // last key event
  std::unordered_set<int32_t> knownDevices;
};

struct State {
  EGLDisplay display = EGL_NO_DISPLAY;
  EGLContext context = EGL_NO_CONTEXT;
  EGLSurface surface = EGL_NO_SURFACE;
  bool resumed = false;
  bool glInfoLogged = false;
  int64_t fpsStartNs = 0;
  int fpsFrames = 0;
  int64_t totalFrames = 0;
  Probe probe;
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

struct Color {
  float r, g, b;
};

// Clear colour for the probe: flashes for cancel/key events, otherwise the
// last tool type (stylus blue, hovering stylus light blue, finger green,
// palm red, eraser orange, mouse purple, unknown grey).
Color probeColor(const Probe& probe)
{
  const int64_t flashNs = 400000000LL;
  const int64_t now = nowNs();
  if (probe.cancelNs && now - probe.cancelNs < flashNs)
    return { 0.85f, 0.10f, 0.85f };   // magenta: ACTION_CANCEL
  if (probe.keyNs && now - probe.keyNs < flashNs)
    return { 0.95f, 0.85f, 0.10f };   // yellow: key event

  switch (probe.lastToolType) {
    case AMOTION_EVENT_TOOL_TYPE_STYLUS:
      return (probe.hovering ? Color{ 0.50f, 0.70f, 1.00f }
                             : Color{ 0.10f, 0.25f, 0.90f });
    case AMOTION_EVENT_TOOL_TYPE_FINGER:  return { 0.10f, 0.65f, 0.20f };
    case AMOTION_EVENT_TOOL_TYPE_PALM:    return { 0.85f, 0.10f, 0.10f };
    case AMOTION_EVENT_TOOL_TYPE_ERASER:  return { 0.95f, 0.50f, 0.05f };
    case AMOTION_EVENT_TOOL_TYPE_MOUSE:   return { 0.50f, 0.30f, 0.70f };
    case AMOTION_EVENT_TOOL_TYPE_UNKNOWN: return { 0.50f, 0.50f, 0.50f };
    default:                              return { 0.16f, 0.45f, 0.62f };
  }
}

void drawFrame(State& st)
{
  if (st.surface == EGL_NO_SURFACE)
    return;

  const Color c = probeColor(st.probe);
  glClearColor(c.r, c.g, c.b, 1.0f);
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

    // Only wake the loop up, the events themselves are read by drainInput().
    case APP_CMD_KEY_EVENT:
    case APP_CMD_TOUCH_EVENT:
      break;

    default:
      LOGI("APP_CMD %d", cmd);
      break;
  }
}

// ---------------------------------------------------------------------------
// M0.6 input probe

const char* motionActionName(int32_t action)
{
  switch (action & AMOTION_EVENT_ACTION_MASK) {
    case AMOTION_EVENT_ACTION_DOWN:           return "DOWN";
    case AMOTION_EVENT_ACTION_UP:             return "UP";
    case AMOTION_EVENT_ACTION_MOVE:           return "MOVE";
    case AMOTION_EVENT_ACTION_CANCEL:         return "CANCEL";
    case AMOTION_EVENT_ACTION_OUTSIDE:        return "OUTSIDE";
    case AMOTION_EVENT_ACTION_POINTER_DOWN:   return "POINTER_DOWN";
    case AMOTION_EVENT_ACTION_POINTER_UP:     return "POINTER_UP";
    case AMOTION_EVENT_ACTION_HOVER_MOVE:     return "HOVER_MOVE";
    case AMOTION_EVENT_ACTION_SCROLL:         return "SCROLL";
    case AMOTION_EVENT_ACTION_HOVER_ENTER:    return "HOVER_ENTER";
    case AMOTION_EVENT_ACTION_HOVER_EXIT:     return "HOVER_EXIT";
    case AMOTION_EVENT_ACTION_BUTTON_PRESS:   return "BUTTON_PRESS";
    case AMOTION_EVENT_ACTION_BUTTON_RELEASE: return "BUTTON_RELEASE";
    default:                                  return "?";
  }
}

const char* toolTypeName(int32_t toolType)
{
  switch (toolType) {
    case AMOTION_EVENT_TOOL_TYPE_UNKNOWN: return "UNKNOWN";
    case AMOTION_EVENT_TOOL_TYPE_FINGER:  return "FINGER";
    case AMOTION_EVENT_TOOL_TYPE_STYLUS:  return "STYLUS";
    case AMOTION_EVENT_TOOL_TYPE_MOUSE:   return "MOUSE";
    case AMOTION_EVENT_TOOL_TYPE_ERASER:  return "ERASER";
    case AMOTION_EVENT_TOOL_TYPE_PALM:    return "PALM";
    default:                              return "?";
  }
}

const char* axisName(int axis)
{
  static const char* names[] = {
    "X", "Y", "PRESSURE", "SIZE", "TOUCH_MAJOR", "TOUCH_MINOR", "TOOL_MAJOR",
    "TOOL_MINOR", "ORIENTATION", "VSCROLL", "HSCROLL", "Z", "RX", "RY", "RZ",
    "HAT_X", "HAT_Y", "LTRIGGER", "RTRIGGER", "THROTTLE", "RUDDER", "WHEEL",
    "GAS", "BRAKE", "DISTANCE", "TILT", "SCROLL", "RELATIVE_X", "RELATIVE_Y",
  };
  if (axis >= 0 && axis < int(sizeof(names) / sizeof(names[0])))
    return names[axis];
  if (axis >= AMOTION_EVENT_AXIS_GENERIC_1 && axis <= AMOTION_EVENT_AXIS_GENERIC_16) {
    static char generic[16][12];
    char* buf = generic[axis - AMOTION_EVENT_AXIS_GENERIC_1];
    snprintf(buf, sizeof(generic[0]), "GENERIC_%d", axis - AMOTION_EVENT_AXIS_GENERIC_1 + 1);
    return buf;
  }
  return "?";
}

struct BitName {
  uint32_t bits;
  const char* name;
};

// Joins the names of all entries whose bits are fully set in "value".
std::string bitNames(uint32_t value, const BitName* table, size_t n)
{
  std::string out;
  for (size_t i = 0; i < n; ++i) {
    if ((value & table[i].bits) == table[i].bits) {
      if (!out.empty())
        out += '|';
      out += table[i].name;
    }
  }
  return (out.empty() ? std::string("-") : out);
}

std::string sourceNames(uint32_t source)
{
  static const BitName table[] = {
    { AINPUT_SOURCE_KEYBOARD,         "KEYBOARD" },
    { AINPUT_SOURCE_DPAD,             "DPAD" },
    { AINPUT_SOURCE_GAMEPAD,          "GAMEPAD" },
    { AINPUT_SOURCE_TOUCHSCREEN,      "TOUCHSCREEN" },
    { AINPUT_SOURCE_MOUSE,            "MOUSE" },
    { AINPUT_SOURCE_STYLUS,           "STYLUS" },
    { AINPUT_SOURCE_BLUETOOTH_STYLUS, "BLUETOOTH_STYLUS" },
    { AINPUT_SOURCE_TRACKBALL,        "TRACKBALL" },
    { AINPUT_SOURCE_MOUSE_RELATIVE,   "MOUSE_RELATIVE" },
    { AINPUT_SOURCE_TOUCHPAD,         "TOUCHPAD" },
    { AINPUT_SOURCE_TOUCH_NAVIGATION, "TOUCH_NAVIGATION" },
    { AINPUT_SOURCE_JOYSTICK,         "JOYSTICK" },
    { AINPUT_SOURCE_HDMI,             "HDMI" },
    { AINPUT_SOURCE_ROTARY_ENCODER,   "ROTARY_ENCODER" },
  };
  return bitNames(source, table, sizeof(table) / sizeof(table[0]));
}

std::string motionFlagNames(uint32_t flags)
{
  // Values from android.view.MotionEvent; FLAG_CANCELED (0x20) is API 33,
  // logged anyway in case HarmonyOS sets it.
  static const BitName table[] = {
    { 0x1,  "WINDOW_IS_OBSCURED" },
    { 0x2,  "WINDOW_IS_PARTIALLY_OBSCURED" },
    { 0x8,  "IS_GENERATED_GESTURE" },
    { 0x20, "CANCELED" },
  };
  return bitNames(flags, table, sizeof(table) / sizeof(table[0]));
}

std::string buttonNames(uint32_t buttons)
{
  static const BitName table[] = {
    { AMOTION_EVENT_BUTTON_PRIMARY,          "PRIMARY" },
    { AMOTION_EVENT_BUTTON_SECONDARY,        "SECONDARY" },
    { AMOTION_EVENT_BUTTON_TERTIARY,         "TERTIARY" },
    { AMOTION_EVENT_BUTTON_BACK,             "BACK" },
    { AMOTION_EVENT_BUTTON_FORWARD,          "FORWARD" },
    { AMOTION_EVENT_BUTTON_STYLUS_PRIMARY,   "STYLUS_PRIMARY" },
    { AMOTION_EVENT_BUTTON_STYLUS_SECONDARY, "STYLUS_SECONDARY" },
  };
  return bitNames(buttons, table, sizeof(table) / sizeof(table[0]));
}

const char* keyActionName(int32_t action)
{
  switch (action) {
    case AKEY_EVENT_ACTION_DOWN:     return "DOWN";
    case AKEY_EVENT_ACTION_UP:       return "UP";
    case AKEY_EVENT_ACTION_MULTIPLE: return "MULTIPLE";
    default:                         return "?";
  }
}

std::string keyFlagNames(uint32_t flags)
{
  static const BitName table[] = {
    { AKEY_EVENT_FLAG_SOFT_KEYBOARD,      "SOFT_KEYBOARD" },
    { AKEY_EVENT_FLAG_FROM_SYSTEM,        "FROM_SYSTEM" },
    { AKEY_EVENT_FLAG_CANCELED,           "CANCELED" },
    { AKEY_EVENT_FLAG_VIRTUAL_HARD_KEY,   "VIRTUAL_HARD_KEY" },
    { AKEY_EVENT_FLAG_LONG_PRESS,         "LONG_PRESS" },
    { AKEY_EVENT_FLAG_CANCELED_LONG_PRESS, "CANCELED_LONG_PRESS" },
    { AKEY_EVENT_FLAG_FALLBACK,           "FALLBACK" },
  };
  return bitNames(flags, table, sizeof(table) / sizeof(table[0]));
}

// Clears a pending Java exception; returns true if there was one.
bool jniFailed(JNIEnv* env)
{
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    return true;
  }
  return false;
}

std::string jstringToStd(JNIEnv* env, jstring str)
{
  if (!str)
    return "(null)";
  const char* chars = env->GetStringUTFChars(str, nullptr);
  std::string out = (chars ? chars : "");
  if (chars)
    env->ReleaseStringUTFChars(str, chars);
  return out;
}

// Logs android.view.InputDevice.getDevice(id): name, sources, ids, whether
// it reports Huawei's pen double-tap key code 718 (expected, NOTES.md §3),
// and its motion ranges (axis min/max, useful for TOUCH_MAJOR on palms).
void describeDevice(JNIEnv* env, int32_t deviceId)
{
  jclass cls = env->FindClass("android/view/InputDevice");
  if (jniFailed(env) || !cls) {
    PLOG("device id=%d (InputDevice class not found)", deviceId);
    return;
  }
  jmethodID getDevice = env->GetStaticMethodID(cls, "getDevice", "(I)Landroid/view/InputDevice;");
  jmethodID getName = env->GetMethodID(cls, "getName", "()Ljava/lang/String;");
  jmethodID getSources = env->GetMethodID(cls, "getSources", "()I");
  jmethodID getVendorId = env->GetMethodID(cls, "getVendorId", "()I");
  jmethodID getProductId = env->GetMethodID(cls, "getProductId", "()I");
  jmethodID getKeyboardType = env->GetMethodID(cls, "getKeyboardType", "()I");
  jmethodID isVirtual = env->GetMethodID(cls, "isVirtual", "()Z");
  jmethodID hasKeys = env->GetMethodID(cls, "hasKeys", "([I)[Z");
  jmethodID getMotionRanges = env->GetMethodID(cls, "getMotionRanges", "()Ljava/util/List;");
  if (jniFailed(env) || !getDevice || !getName || !getSources || !getVendorId ||
      !getProductId || !getKeyboardType || !isVirtual || !hasKeys || !getMotionRanges) {
    PLOG("device id=%d (InputDevice methods not found)", deviceId);
    env->DeleteLocalRef(cls);
    return;
  }

  jobject dev = env->CallStaticObjectMethod(cls, getDevice, deviceId);
  if (jniFailed(env) || !dev) {
    PLOG("device id=%d name=(no InputDevice for this id)", deviceId);
    env->DeleteLocalRef(cls);
    return;
  }

  jstring name = (jstring)env->CallObjectMethod(dev, getName);
  std::string nameStr = (jniFailed(env) ? "?" : jstringToStd(env, name));
  if (name)
    env->DeleteLocalRef(name);
  const uint32_t sources = uint32_t(env->CallIntMethod(dev, getSources));
  const int vendor = env->CallIntMethod(dev, getVendorId);
  const int product = env->CallIntMethod(dev, getProductId);
  const int kbType = env->CallIntMethod(dev, getKeyboardType);
  const bool virt = env->CallBooleanMethod(dev, isVirtual);
  jniFailed(env);

  // hasKeys(718)
  int has718 = -1;
  jintArray keys = env->NewIntArray(1);
  if (keys) {
    const jint key = 718;
    env->SetIntArrayRegion(keys, 0, 1, &key);
    jbooleanArray result = (jbooleanArray)env->CallObjectMethod(dev, hasKeys, keys);
    if (!jniFailed(env) && result) {
      jboolean value = JNI_FALSE;
      env->GetBooleanArrayRegion(result, 0, 1, &value);
      has718 = (value ? 1 : 0);
      env->DeleteLocalRef(result);
    }
    env->DeleteLocalRef(keys);
  }

  PLOG("device id=%d name=\"%s\" sources=0x%x(%s) vendor=0x%04x product=0x%04x "
       "keyboardType=%d virtual=%d hasKey718=%d",
       deviceId, nameStr.c_str(), sources, sourceNames(sources).c_str(),
       vendor, product, kbType, virt ? 1 : 0, has718);

  jobject ranges = env->CallObjectMethod(dev, getMotionRanges);
  if (!jniFailed(env) && ranges) {
    jclass listCls = env->FindClass("java/util/List");
    jclass rangeCls = env->FindClass("android/view/InputDevice$MotionRange");
    if (!jniFailed(env) && listCls && rangeCls) {
      jmethodID size = env->GetMethodID(listCls, "size", "()I");
      jmethodID get = env->GetMethodID(listCls, "get", "(I)Ljava/lang/Object;");
      jmethodID rAxis = env->GetMethodID(rangeCls, "getAxis", "()I");
      jmethodID rSource = env->GetMethodID(rangeCls, "getSource", "()I");
      jmethodID rMin = env->GetMethodID(rangeCls, "getMin", "()F");
      jmethodID rMax = env->GetMethodID(rangeCls, "getMax", "()F");
      jmethodID rRes = env->GetMethodID(rangeCls, "getResolution", "()F");
      jmethodID rFuzz = env->GetMethodID(rangeCls, "getFuzz", "()F");
      if (!jniFailed(env) && size && get && rAxis && rSource && rMin && rMax && rRes && rFuzz) {
        const int n = env->CallIntMethod(ranges, size);
        for (int i = 0; i < n && !jniFailed(env); ++i) {
          jobject r = env->CallObjectMethod(ranges, get, i);
          if (jniFailed(env) || !r)
            break;
          const int axis = env->CallIntMethod(r, rAxis);
          const uint32_t src = uint32_t(env->CallIntMethod(r, rSource));
          PLOG("device id=%d range axis=%s(%d) source=0x%x min=%g max=%g resolution=%g fuzz=%g",
               deviceId, axisName(axis), axis, src,
               env->CallFloatMethod(r, rMin), env->CallFloatMethod(r, rMax),
               env->CallFloatMethod(r, rRes), env->CallFloatMethod(r, rFuzz));
          env->DeleteLocalRef(r);
        }
      }
    }
    if (listCls)
      env->DeleteLocalRef(listCls);
    if (rangeCls)
      env->DeleteLocalRef(rangeCls);
    env->DeleteLocalRef(ranges);
  }
  jniFailed(env);

  env->DeleteLocalRef(dev);
  env->DeleteLocalRef(cls);
}

void noteDevice(android_app* app, Probe& probe, int32_t deviceId)
{
  if (!probe.knownDevices.insert(deviceId).second)
    return;

  JNIEnv* env = nullptr;
  if (app->activity->vm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) {
    PLOG("device id=%d (JNI attach failed)", deviceId);
    return;
  }
  describeDevice(env, deviceId);
}

// Describes every input device the system knows at startup, so that devices
// which never deliver an event to us (e.g. the pen's BT keyboard) are listed.
void listInputDevices(android_app* app, Probe& probe)
{
  JNIEnv* env = nullptr;
  if (app->activity->vm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env)
    return;

  jclass cls = env->FindClass("android/view/InputDevice");
  jmethodID getDeviceIds = (cls ? env->GetStaticMethodID(cls, "getDeviceIds", "()[I") : nullptr);
  jintArray ids = (getDeviceIds ? (jintArray)env->CallStaticObjectMethod(cls, getDeviceIds) : nullptr);
  if (jniFailed(env) || !ids) {
    PLOG("InputDevice.getDeviceIds() failed");
  }
  else {
    const jsize n = env->GetArrayLength(ids);
    PLOG("%d input devices at startup", int(n));
    jint* values = env->GetIntArrayElements(ids, nullptr);
    for (jsize i = 0; values && i < n; ++i)
      noteDevice(app, probe, values[i]);
    if (values)
      env->ReleaseIntArrayElements(ids, values, JNI_ABORT);
    env->DeleteLocalRef(ids);
  }
  if (cls)
    env->DeleteLocalRef(cls);
}

// Axes we enable in GameActivity (only X/Y are reported by default) and log
// by name; any other non-zero axis is appended as "other=" to find vendor
// specific data (e.g. a palm flag).
const int kNamedAxes[] = {
  AMOTION_EVENT_AXIS_X, AMOTION_EVENT_AXIS_Y, AMOTION_EVENT_AXIS_PRESSURE,
  AMOTION_EVENT_AXIS_SIZE, AMOTION_EVENT_AXIS_TOUCH_MAJOR,
  AMOTION_EVENT_AXIS_TOUCH_MINOR, AMOTION_EVENT_AXIS_TOOL_MAJOR,
  AMOTION_EVENT_AXIS_TOOL_MINOR, AMOTION_EVENT_AXIS_ORIENTATION,
  AMOTION_EVENT_AXIS_TILT, AMOTION_EVENT_AXIS_DISTANCE,
};

void enableAllAxes()
{
  for (int axis = 0; axis < GAME_ACTIVITY_POINTER_INFO_AXIS_COUNT; ++axis)
    GameActivityPointerAxes_enableAxis(axis);
}

void logMotionEvent(android_app* app, Probe& probe, const GameActivityMotionEvent& ev)
{
  noteDevice(app, probe, ev.deviceId);

  const int64_t seq = ++probe.motionSeq;
  const int32_t actionMasked = ev.action & AMOTION_EVENT_ACTION_MASK;
  const int actionIndex = (ev.action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                          AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

  PLOG("motion #%lld t=%lld down=%lld action=%s(0x%x) index=%d source=0x%x(%s) device=%d "
       "flags=0x%x(%s) buttonState=0x%x(%s) actionButton=0x%x(%s) metaState=0x%x "
       "classification=%d edgeFlags=0x%x pointerCount=%u historySize=%d",
       (long long)seq, (long long)ev.eventTime, (long long)ev.downTime,
       motionActionName(ev.action), ev.action, actionIndex,
       ev.source, sourceNames(ev.source).c_str(), ev.deviceId,
       ev.flags, motionFlagNames(ev.flags).c_str(),
       ev.buttonState, buttonNames(ev.buttonState).c_str(),
       ev.actionButton, buttonNames(ev.actionButton).c_str(),
       ev.metaState, ev.classification, ev.edgeFlags,
       ev.pointerCount, ev.historySize);

  for (uint32_t i = 0; i < ev.pointerCount; ++i) {
    const GameActivityPointerAxes& p = ev.pointers[i];
    auto axis = [&p](int a) { return GameActivityPointerAxes_getAxisValue(&p, a); };

    std::string other;
    for (int a = 0; a < GAME_ACTIVITY_POINTER_INFO_AXIS_COUNT; ++a) {
      bool named = false;
      for (int n : kNamedAxes)
        named |= (n == a);
      const float v = axis(a);
      if (!named && v != 0.0f) {
        char buf[48];
        snprintf(buf, sizeof(buf), " %s=%g", axisName(a), v);
        other += buf;
      }
    }

    PLOG("motion #%lld p%u id=%d tool=%s(%d) x=%.2f y=%.2f rawX=%.2f rawY=%.2f "
         "pressure=%.4f size=%.4f touchMajor=%.2f touchMinor=%.2f "
         "toolMajor=%.2f toolMinor=%.2f orientation=%.3f tilt=%.3f distance=%.3f%s%s",
         (long long)seq, i, p.id, toolTypeName(p.toolType), p.toolType,
         axis(AMOTION_EVENT_AXIS_X), axis(AMOTION_EVENT_AXIS_Y), p.rawX, p.rawY,
         axis(AMOTION_EVENT_AXIS_PRESSURE), axis(AMOTION_EVENT_AXIS_SIZE),
         axis(AMOTION_EVENT_AXIS_TOUCH_MAJOR), axis(AMOTION_EVENT_AXIS_TOUCH_MINOR),
         axis(AMOTION_EVENT_AXIS_TOOL_MAJOR), axis(AMOTION_EVENT_AXIS_TOOL_MINOR),
         axis(AMOTION_EVENT_AXIS_ORIENTATION), axis(AMOTION_EVENT_AXIS_TILT),
         axis(AMOTION_EVENT_AXIS_DISTANCE),
         other.empty() ? "" : " other:", other.c_str());
  }

  // On-screen feedback: a palm anywhere in the event wins, otherwise the
  // tool of the pointer the action refers to.
  int32_t tool = -1;
  for (uint32_t i = 0; i < ev.pointerCount; ++i) {
    if (ev.pointers[i].toolType == AMOTION_EVENT_TOOL_TYPE_PALM)
      tool = AMOTION_EVENT_TOOL_TYPE_PALM;
  }
  if (tool < 0 && ev.pointerCount > 0)
    tool = ev.pointers[uint32_t(actionIndex) < ev.pointerCount ? actionIndex : 0].toolType;
  if (tool >= 0)
    probe.lastToolType = tool;
  probe.hovering = (actionMasked == AMOTION_EVENT_ACTION_HOVER_ENTER ||
                    actionMasked == AMOTION_EVENT_ACTION_HOVER_MOVE);
  if (actionMasked == AMOTION_EVENT_ACTION_CANCEL)
    probe.cancelNs = nowNs();
}

void logKeyEvent(android_app* app, Probe& probe, const GameActivityKeyEvent& ev)
{
  noteDevice(app, probe, ev.deviceId);

  PLOG("key #%lld t=%lld down=%lld action=%s(%d) keyCode=%d scanCode=%d metaState=0x%x "
       "modifiers=0x%x repeatCount=%d flags=0x%x(%s) source=0x%x(%s) device=%d unicode=%d",
       (long long)++probe.keySeq, (long long)ev.eventTime, (long long)ev.downTime,
       keyActionName(ev.action), ev.action, ev.keyCode, ev.scanCode, ev.metaState,
       ev.modifiers, ev.repeatCount, ev.flags, keyFlagNames(ev.flags).c_str(),
       ev.source, sourceNames(ev.source).c_str(), ev.deviceId, ev.unicodeChar);

  probe.keyNs = nowNs();
}

void drainInput(android_app* app, Probe& probe)
{
  android_input_buffer* ib = android_app_swap_input_buffers(app);
  if (!ib)
    return;
  for (uint64_t i = 0; i < ib->motionEventsCount; ++i)
    logMotionEvent(app, probe, ib->motionEvents[i]);
  for (uint64_t i = 0; i < ib->keyEventsCount; ++i)
    logKeyEvent(app, probe, ib->keyEvents[i]);
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

  // Input probe setup (M0.6).
  enableAllAxes();
  // The probe wants everything: hover, stylus, mouse and all keys (the
  // default filters keep only touchscreen motion and drop volume/camera keys).
  android_app_set_motion_event_filter(app, [](const GameActivityMotionEvent*) { return true; });
  android_app_set_key_event_filter(app, [](const GameActivityKeyEvent*) { return true; });
  PLOG("input probe on: all motion sources, all keys, all %d axes",
       GAME_ACTIVITY_POINTER_INFO_AXIS_COUNT);
  listInputDevices(app, state.probe);

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
      // JNI calls (refresh rate, input device names) attached this thread.
      app->activity->vm->DetachCurrentThread();
      return;
    }

    drainInput(app, state.probe);
    if (state.resumed)
      drawFrame(state);
  }
}
