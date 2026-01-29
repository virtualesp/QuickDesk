#pragma once

#include <QObject>

class ConfigViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(int darkTheme READ darkTheme WRITE setDarkTheme NOTIFY darkThemeChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(int accessCodeRefreshInterval READ accessCodeRefreshInterval WRITE setAccessCodeRefreshInterval NOTIFY accessCodeRefreshIntervalChanged)

public:
    ConfigViewModel(QObject* parent = nullptr);
    virtual ~ConfigViewModel();

    int darkTheme();
    void setDarkTheme(int value);
    
    QString language();
    void setLanguage(const QString& value);
    
    int accessCodeRefreshInterval();
    void setAccessCodeRefreshInterval(int value);

signals:
    void darkThemeChanged(int value);
    void languageChanged(const QString& value);
    void accessCodeRefreshIntervalChanged(int value);
};
