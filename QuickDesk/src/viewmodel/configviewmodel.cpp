#include "configviewmodel.h"

#include "core/localconfigcenter.h"

ConfigViewModel::ConfigViewModel(QObject* parent)
    : QObject(parent)
{
    connect(&core::LocalConfigCenter::instance(), &core::LocalConfigCenter::signalDarkThemeChanged, this, &ConfigViewModel::darkThemeChanged);
    connect(&core::LocalConfigCenter::instance(), &core::LocalConfigCenter::signalLanguageChanged, this, &ConfigViewModel::languageChanged);
    connect(&core::LocalConfigCenter::instance(), &core::LocalConfigCenter::signalAccessCodeRefreshIntervalChanged, this, &ConfigViewModel::accessCodeRefreshIntervalChanged);
}

ConfigViewModel::~ConfigViewModel()
{
}

int ConfigViewModel::darkTheme()
{
    return core::LocalConfigCenter::instance().darkTheme();
}

void ConfigViewModel::setDarkTheme(int value)
{
    core::LocalConfigCenter::instance().setDarkTheme(value);
}

QString ConfigViewModel::language()
{
    return core::LocalConfigCenter::instance().language();
}

void ConfigViewModel::setLanguage(const QString& value)
{
    core::LocalConfigCenter::instance().setLanguage(value);
}

int ConfigViewModel::accessCodeRefreshInterval()
{
    return core::LocalConfigCenter::instance().accessCodeRefreshInterval();
}

void ConfigViewModel::setAccessCodeRefreshInterval(int value)
{
    core::LocalConfigCenter::instance().setAccessCodeRefreshInterval(value);
}
