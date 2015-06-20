/*
 * Cantata
 *
 * Copyright (c) 2011-2015 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "onlinedbwidget.h"
#include "widgets/itemview.h"
#include "support/action.h"
#include "support/localize.h"
#include "support/messagebox.h"
#include <QTimer>

OnlineDbWidget::OnlineDbWidget(OnlineDbService *s, QWidget *p)
    : SinglePageWidget(p)
    , srv(s)
{
    view->setModel(s);
    init();
    view->alwaysShowHeader();
    connect(view, SIGNAL(headerClicked(int)), SLOT(headerClicked(int)));
}

OnlineDbWidget::~OnlineDbWidget()
{
}

QStringList OnlineDbWidget::selectedFiles(bool allowPlaylists) const
{
    QModelIndexList selected = view->selectedIndexes();
    if (selected.isEmpty()) {
        return QStringList();
    }
    return srv->filenames(selected, allowPlaylists);
}

QList<Song> OnlineDbWidget::selectedSongs(bool allowPlaylists) const
{
    QModelIndexList selected = view->selectedIndexes();
    if (selected.isEmpty()) {
        return QList<Song>();
    }
    return srv->songs(selected, allowPlaylists);
}

void OnlineDbWidget::showEvent(QShowEvent *e)
{
    SinglePageWidget::showEvent(e);
    if (srv->previouslyDownloaded()) {
        srv->load();
    } else if (!srv->isDownloading()) {
        QTimer::singleShot(0, this, SLOT(firstTimePrompt()));
    }
}

void OnlineDbWidget::firstTimePrompt()
{
    if (MessageBox::No==MessageBox::questionYesNo(this, srv->averageSize()
                                                        ? i18n("The music listing needs to be downloaded, this can consume over %1Mb of disk space", srv->averageSize())
                                                        : i18n("Dowload music listing?"),
                                                   QString(), i18n("Download"), StdGuiItem::cancel())) {
        emit close();
    } else {
        srv->download(false);
    }
}

void OnlineDbWidget::headerClicked(int level)
{
    if (0==level) {
        emit close();
    }
}

void OnlineDbWidget::doSearch()
{
    srv->search(view->searchText());
}

// TODO: Cancel download?
void OnlineDbWidget::refresh()
{
    if (!srv->isDownloading() && MessageBox::Yes==MessageBox::questionYesNo(this, i18n("Re-download music listing?"), QString(), i18n("Download"), StdGuiItem::cancel())) {
        srv->download(true);
    }
}

void OnlineDbWidget::configure()
{
    srv->configure(this);
}