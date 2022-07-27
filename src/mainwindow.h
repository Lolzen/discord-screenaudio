#pragma once

#include "discordpage.h"

#include <QMainWindow>
#include <QScopedPointer>
#include <QString>
#include <QVector>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);

private:
  void setupWebView();
  QWebEngineView *m_webView;
  QWebEngineProfile *prepareProfile();
  DiscordPage *m_discordPage;
  void closeEvent(QCloseEvent *event) override;
  bool m_wasMaximized;

private Q_SLOTS:
  void fullScreenRequested(QWebEngineFullScreenRequest fullScreenRequest);
};
