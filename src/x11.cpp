#include "x11.h"

#include "log.h"

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <dlfcn.h>

namespace X11 {

QStringList getScreens() {
  Display *display = XOpenDisplay(nullptr);
  if (!display) {
    qDebug(x11Warn) << "Unable to open display.";
    return QStringList{};
  }

  int randrEventBase = 0;
  int errorBaseIgnored = 0;
  if (!XRRQueryExtension(display, &randrEventBase, &errorBaseIgnored)) {
    qDebug(x11Warn) << "X server does not support XRandR.";
    return QStringList{};
  }

  int majorVersion = 0;
  int minorVersion = 0;
  if (!XRRQueryVersion(display, &majorVersion, &minorVersion)) {
    qDebug(x11Warn) << "X server does not support XRandR.";
    return QStringList{};
  }

  if (majorVersion < 1 || (majorVersion == 1 && minorVersion < 5)) {
    qDebug(x11Warn) << "XRandR entension is older than v1.5.";
    return QStringList{};
  }

  typedef XRRMonitorInfo *(*GetMonitorsFunc)(Display *, Window, Bool, int *);
  GetMonitorsFunc getMonitors =
      reinterpret_cast<GetMonitorsFunc>(dlsym(RTLD_DEFAULT, "XRRGetMonitors"));
  typedef void (*FreeMonitorsFunc)(XRRMonitorInfo *);
  FreeMonitorsFunc freeMonitors = reinterpret_cast<FreeMonitorsFunc>(
      dlsym(RTLD_DEFAULT, "XRRFreeMonitors"));
  if (!getMonitors || !freeMonitors) {
    qDebug(x11Warn) << "Unable to link XRandR monitor functions.";
    return QStringList{};
  }

  Window rootWindow = RootWindow(display, DefaultScreen(display));
  if (rootWindow == BadValue) {
    qDebug(x11Warn) << "Unable to get the root window.";
    return QStringList{};
  }

  int numMonitors = 0;
  XRRMonitorInfo *monitors =
      getMonitors(display, rootWindow, true, &numMonitors);
  QStringList monitorNames;
  for (size_t i = 0; i < numMonitors; i++) {
    auto name = QString::number(monitors[i].name);
    monitorNames.append(name);
  }
  return monitorNames;
}

} // namespace X11
