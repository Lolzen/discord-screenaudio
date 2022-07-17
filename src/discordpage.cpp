#include "discordpage.h"
#include "virtmic.h"

#include <QApplication>
#include <QDesktopServices>
#include <QFile>
#include <QTimer>
#include <QWebChannel>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>

DiscordPage::DiscordPage(QWidget *parent) : QWebEnginePage(parent) {
  setBackgroundColor(QColor("#202225"));
  m_virtmicProcess.setProcessChannelMode(QProcess::ForwardedChannels);

  connect(this, &QWebEnginePage::featurePermissionRequested, this,
          &DiscordPage::featurePermissionRequested);

  settings()->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, true);
  settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);
  settings()->setAttribute(QWebEngineSettings::AllowRunningInsecureContent,
                           true);
  settings()->setAttribute(
      QWebEngineSettings::AllowWindowActivationFromJavaScript, true);
  settings()->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
  settings()->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture,
                           false);

  setUrl(QUrl("https://discord.com/app"));

  injectScript(":/assets/userscript.js");

  connect(&m_streamDialog, &StreamDialog::requestedStreamStart, this,
          &DiscordPage::startStream);
}

void DiscordPage::injectScript(QString source) {
  qDebug() << "[main   ] Injecting" << source;

  QFile userscript(source);

  if (!userscript.open(QIODevice::ReadOnly)) {
    qFatal("Failed to load %s with error: %s", source.toLatin1().constData(),
           userscript.errorString().toLatin1().constData());
  } else {
    QByteArray userscriptJs = userscript.readAll();

    QWebEngineScript script;

    script.setSourceCode(userscriptJs);
    script.setName("userscript.js");
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setRunsOnSubFrames(false);

    scripts().insert(script);
  }
}

void DiscordPage::featurePermissionRequested(const QUrl &securityOrigin,
                                             QWebEnginePage::Feature feature) {
  // Allow every permission asked
  setFeaturePermission(securityOrigin, feature,
                       QWebEnginePage::PermissionGrantedByUser);
}

bool DiscordPage::acceptNavigationRequest(const QUrl &url,
                                          QWebEnginePage::NavigationType type,
                                          bool isMainFrame) {
  // if (type == QWebEnginePage::NavigationTypeLinkClicked) {
  //   QDesktopServices::openUrl(url);
  //   return false;
  // }
  return true;
};

void DiscordPage::stopVirtmic() {
  if (m_virtmicProcess.state() == QProcess::Running) {
    qDebug() << "[virtmic] Stopping Virtmic";
    m_virtmicProcess.kill();
  }
}

void DiscordPage::startVirtmic(QString target) {
  if (target != "None") {
    qDebug() << "[virtmic] Starting Virtmic with target" << target;
    m_virtmicProcess.start(QApplication::arguments()[0], {"--virtmic", target});
  }
}

void DiscordPage::javaScriptConsoleMessage(
    QWebEnginePage::JavaScriptConsoleMessageLevel level, const QString &message,
    int lineNumber, const QString &sourceID) {
  if (message == "!discord-screenaudio-start-stream") {
    if (m_streamDialog.isHidden())
      m_streamDialog.setHidden(false);
    else
      m_streamDialog.activateWindow();
    m_streamDialog.updateTargets();
  } else if (message == "!discord-screenaudio-stream-stopped") {
    stopVirtmic();
  } else {
    qDebug() << "[discord]" << message;
  }
}

void DiscordPage::startStream(QString target, uint width, uint height,
                              uint frameRate) {
  stopVirtmic();
  startVirtmic(target);
  // Wait a bit for the virtmic to start
  QTimer::singleShot(target == "None" ? 0 : 200, [=]() {
    runJavaScript(QString("window.discordScreenaudioStartStream(%1, %2, %3);")
                      .arg(width)
                      .arg(height)
                      .arg(frameRate));
  });
}
