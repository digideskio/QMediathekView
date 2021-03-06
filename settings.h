/*

Copyright 2016 Adam Reichold

This file is part of QMediathekView.

QMediathekView is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

QMediathekView is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with QMediathekView.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef SETTINGS_H
#define SETTINGS_H

#include <QDateTime>
#include <QDir>
#include <QObject>
#include <QStringList>

#include "schema.h"

class QSettings;

namespace QMediathekView
{

class Settings : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Settings)

public:
    explicit Settings(QObject* parent = 0);
    ~Settings();

public:
    QString userAgent() const;
    QString fullListUrl() const;
    QString partialListUrl() const;

    QStringList fullListMirrors() const;
    void setFullListMirrors(const QStringList& mirrors);

    QStringList partialListMirrors() const;
    void setPartialListMirrors(const QStringList& mirrors);

    int mirrorsUpdateAfterDays() const;
    void setMirrorsUpdateAfterDays(int days);

    int databaseUpdateAfterHours() const;
    void setDatabaseUpdateAfterHours(int hours);

    QDateTime mirrorsUpdatedOn() const;
    void setMirrorsUpdatedOn();

    QDateTime databaseUpdatedOn() const;
    void setDatabaseUpdatedOn();

    QString playCommand() const;
    void setPlayCommand(const QString& command);

    QString downloadCommand() const;
    void setDownloadCommand(const QString& command);

    QDir downloadFolder() const;
    void setDownloadFolder(const QDir& folder);

    Url preferredUrl() const;
    void setPreferredUrl(const Url type);

    QByteArray mainWindowGeometry() const;
    void setMainWindowGeometry(const QByteArray& geometry);

    QByteArray mainWindowState() const;
    void setMainWindowState(const QByteArray& state);

private:
    QSettings* m_settings;

};

} // QMediathekView

#endif // SETTINGS_H
