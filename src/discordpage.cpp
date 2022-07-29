#include "discordpage.h"
#include "virtmic.h"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDesktopServices>
#include <QFile>
#include <QList>
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

  connect(this, &QWebEnginePage::loadStarted, [=]() {
    runJavaScript(QString("window.discordScreenaudioVersion = '%1';")
                      .arg(QApplication::applicationVersion()));
  });

  settings()->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, true);
  settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);
  settings()->setAttribute(QWebEngineSettings::AllowRunningInsecureContent,
                           true);
  settings()->setAttribute(
      QWebEngineSettings::AllowWindowActivationFromJavaScript, true);
  settings()->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
  settings()->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture,
                           false);
  settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);

  setUrl(QUrl("https://discord.com/app"));

  injectScript(":/assets/userscript.js");
  injectVersion(QApplication::applicationVersion());

  connect(&m_streamDialog, &StreamDialog::requestedStreamStart, this,
          &DiscordPage::startStream);
}

void DiscordPage::injectScript(QString source) {
  qDebug() << "[main   ] Injecting " << source;

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

void DiscordPage::injectVersion(QString version) {
  QWebEngineScript script;

  auto code = QString("window.discordScreenaudioVersion = '%1';").arg(version);

  script.setSourceCode(code);
  script.setName("version.js");
  script.setWorldId(QWebEngineScript::MainWorld);
  script.setInjectionPoint(QWebEngineScript::DocumentCreation);
  script.setRunsOnSubFrames(false);

  scripts().insert(script);
}

void DiscordPage::featurePermissionRequested(const QUrl &securityOrigin,
                                             QWebEnginePage::Feature feature) {
  // Allow every permission asked
  setFeaturePermission(securityOrigin, feature,
                       QWebEnginePage::PermissionGrantedByUser);

  if (feature == QWebEnginePage::Feature::MediaAudioCapture) {
    if (m_virtmicProcess.state() == QProcess::NotRunning) {
      qDebug() << "[virtmic] Starting Virtmic with no target to make sure "
                  "Discord can find all the audio devices";
      m_virtmicProcess.start(QApplication::arguments()[0],
                             {"--virtmic", "None"});
    }
  }
}

bool DiscordPage::acceptNavigationRequest(const QUrl &url,
                                          QWebEnginePage::NavigationType type,
                                          bool isMainFrame) {
  if (type == QWebEnginePage::NavigationTypeLinkClicked) {
    QDesktopServices::openUrl(url);
    return false;
  }
  return true;
};

bool ExternalPage::acceptNavigationRequest(const QUrl &url,
                                           QWebEnginePage::NavigationType type,
                                           bool isMainFrame) {
  QDesktopServices::openUrl(url);
  deleteLater();
  return false;
}

QWebEnginePage *DiscordPage::createWindow(QWebEnginePage::WebWindowType type) {
  return new ExternalPage;
}

void DiscordPage::stopVirtmic() {
  if (m_virtmicProcess.state() == QProcess::Running) {
    qDebug() << "[virtmic] Stopping Virtmic";
    m_virtmicProcess.kill();
    m_virtmicProcess.waitForFinished();
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
    requestScreenCapture();
    // if (m_streamDialog.isHidden())
    //   m_streamDialog.setHidden(false);
    // else
    //   m_streamDialog.activateWindow();
    // m_streamDialog.updateTargets();
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

void DiscordPage::requestScreenCapture() {
  QDBusInterface interface("org.freedesktop.portal.Desktop",
                           "/org/freedesktop/portal/desktop",
                           "org.freedesktop.portal.ScreenCast");

  auto reply = interface.call(
      "CreateSession",
      QVariantMap{{"handle_token", "dsa"}, {"session_handle_token", "dsa"}});

  auto path = reply.arguments()[0].value<QDBusObjectPath>().path();

  if (path == "")
    qFatal("Failed to create screencast session");

  qDebug() << path;
  qDebug() << QDBusConnection::sessionBus().connect(
      "org.freedesktop.portal.Desktop", path, "org.freedesktop.portal.Request",
      "Response", this, SLOT(receiveSession(uint, QVariantMap)));

  // if (reply.type() != QDBusMessage::ReplyMessage)
  //   qFatal("Call failed");

  // if (reply.arguments().count() != 1 ||
  //     reply.arguments()[0].type() != QVariant::List)
  //   qFatal("Unexpected reply");

  // qDebug() << reply.arguments()[0].toList();
}

void DiscordPage::receiveSession(uint response, QVariantMap results) {
  auto session = QDBusObjectPath(results["session_handle"].toString());

  QDBusInterface interface("org.freedesktop.portal.Desktop",
                           "/org/freedesktop/portal/desktop",
                           "org.freedesktop.portal.ScreenCast");

  auto respo = interface.call("SelectSources", session, QVariantMap{});

  qDebug() << respo;
}
