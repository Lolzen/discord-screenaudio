#pragma once

#include "streamdialog.h"
#include "virtmic.h"

#include <QProcess>
#include <QWebEngineFullScreenRequest>
#include <QWebEnginePage>

class DiscordPage : public QWebEnginePage {
  Q_OBJECT

public:
  explicit DiscordPage(QWidget *parent = nullptr);

private:
  StreamDialog m_streamDialog;
  QProcess m_virtmicProcess;
  bool acceptNavigationRequest(const QUrl &url,
                               QWebEnginePage::NavigationType type,
                               bool isMainFrame) override;
  QWebEnginePage *createWindow(QWebEnginePage::WebWindowType type) override;
  void
  javaScriptConsoleMessage(QWebEnginePage::JavaScriptConsoleMessageLevel level,
                           const QString &message, int lineNumber,
                           const QString &sourceID) override;
  void injectScript(QString source);
  void injectVersion(QString version);
  void stopVirtmic();
  void startVirtmic(QString target);
  void requestScreenCapture();

private Q_SLOTS:
  void featurePermissionRequested(const QUrl &securityOrigin,
                                  QWebEnginePage::Feature feature);
  void startStream(QString target, uint width, uint height, uint frameRate);
  void receiveSession(uint, QVariantMap);
};

// Will immediately get destroyed again but is needed for navigation to
// target="_blank" links, since QWebEnginePage::newWindowRequested is
// only available sinec Qt 6.3.
class ExternalPage : public QWebEnginePage {
  Q_OBJECT

private:
  bool acceptNavigationRequest(const QUrl &url,
                               QWebEnginePage::NavigationType type,
                               bool isMainFrame) override;
};
