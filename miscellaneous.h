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

#ifndef MISCELLANEOUS_H
#define MISCELLANEOUS_H

#include <QToolButton>

class QAction;

namespace QMediathekView
{

class Model;

class UrlButton : public QToolButton
{
    Q_OBJECT
    Q_DISABLE_COPY(UrlButton)

public:
    UrlButton(const Model& model, QWidget* parent);

signals:
    void defaultTriggered();
    void smallTriggered();
    void largeTriggered();

public:
    void currentChanged(const QModelIndex& current, const QModelIndex& previous);

private:
    const Model& m_model;

    QAction* m_defaultAction;
    QAction* m_smallAction;
    QAction* m_largeAction;

};

} // QMediathekView

#endif // MISCELLANEOUS_H
