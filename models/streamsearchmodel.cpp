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

#include "streamsearchmodel.h"
#include "widgets/icons.h"
#include "roles.h"
#include "support/localize.h"
#include "playqueuemodel.h"
#include "network/networkaccessmanager.h"
#include "gui/stdactions.h"
#include "gui/settings.h"
#include "gui/plurals.h"
#include <QString>
#include <QVariant>
#include <QXmlStreamReader>
#include <QMimeData>
#include <QLocale>
#include <QUrl>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#endif

static QIcon getIcon(const QString &name)
{
    QIcon icon;
    icon.addFile(":"+name);
    return icon.isNull() ? Icons::self()->streamCategoryIcon : icon;
}

StreamSearchModel::StreamSearchModel(QObject *parent)
    : ActionModel(parent)
    , root(new StreamsModel::CategoryItem(QString(), "root"))
{
    // ORDER *MUST* MATCH Category ENUM!!!!!
    root->children.append(new StreamsModel::CategoryItem("http://opml.radiotime.com/Search.ashx", i18n("TuneIn"), root, getIcon("tunein")));
    root->children.append(new StreamsModel::CategoryItem(QLatin1String("http://")+StreamsModel::constShoutCastHost+QLatin1String("/legacy/genrelist"), i18n("ShoutCast"), root, getIcon("shoutcast")));
    root->children.append(new StreamsModel::CategoryItem(QLatin1String("http://")+StreamsModel::constDirbleHost+QLatin1String("/v2/search/"), i18n("Dirble"), root, getIcon("dirble")));
}

StreamSearchModel::~StreamSearchModel()
{
    clear();
    delete root;
}

QModelIndex StreamSearchModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    const StreamsModel::CategoryItem * p = parent.isValid() ? static_cast<StreamsModel::CategoryItem *>(parent.internalPointer()) : root;
    const StreamsModel::Item * c = row<p->children.count() ? p->children.at(row) : 0;
    return c ? createIndex(row, column, (void *)c) : QModelIndex();
}

QModelIndex StreamSearchModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    StreamsModel::Item * parent = toItem(index)->parent;

    if (!parent || parent == root || !parent->parent) {
        return QModelIndex();
    }
    return createIndex(static_cast<const StreamsModel::CategoryItem *>(parent->parent)->children.indexOf(parent), 0, parent);
}

QVariant StreamSearchModel::headerData(int /*section*/, Qt::Orientation /*orientation*/, int /*role*/) const
{
    return QVariant();
}

int StreamSearchModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        StreamsModel::Item *item = toItem(parent);
        return item->isCategory() ? static_cast<StreamsModel::CategoryItem *>(item)->children.count() : 0;
    }
    return root->children.count();
}

int StreamSearchModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant StreamSearchModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        switch (role) {
        case Cantata::Role_TitleText:
            return i18n("Stream Search");
        case Cantata::Role_SubText:
            return i18n("Search for radio streams");
        case Qt::DecorationRole:
            return Icon("edit-find");
        }
        return QVariant();
    }

    const StreamsModel::Item *item = toItem(index);

    switch (role) {
    case Qt::DecorationRole:
        if (item->parent==root && item->isCategory()) {
            return static_cast<const StreamsModel::CategoryItem *>(item) ->icon;
        }
        return item->isCategory() ? Icons::self()->streamCategoryIcon : Icons::self()->streamListIcon;
    case Qt::DisplayRole:
        return item->name;
    case Qt::ToolTipRole:
        if (!Settings::self()->infoTooltips()) {
            return QVariant();
        }
        return item->isCategory() ? item->name : (item->name+QLatin1String("<br><small><i>")+item->url+QLatin1String("</i></small>"));
    case Cantata::Role_SubText:
        if (item->isCategory()) {
            const StreamsModel::CategoryItem *cat=static_cast<const StreamsModel::CategoryItem *>(item);
            switch (cat->state) {
            case StreamsModel::CategoryItem::Initial:
                return root==item->parent ? i18n("Enter string to search") : i18n("Not Loaded");
            case StreamsModel::CategoryItem::Fetching:
                return i18n("Loading...");
            default:
                return Plurals::entries(cat->children.count());
            }
        } else {
            return item->subText.isEmpty() ? QLatin1String("-") : item->subText;
        }
        break;
    case Cantata::Role_Actions:
        if (item->isCategory()){
            if (static_cast<const StreamsModel::CategoryItem *>(item)->canBookmark) {
                QVariant v;
                v.setValue<QList<Action *> >(QList<Action *>() << StreamsModel::self()->addBookmarkAct());
                return v;
            }
        } else {
            QVariant v;
            v.setValue<QList<Action *> >(QList<Action *>() << StdActions::self()->replacePlayQueueAction << StreamsModel::self()->addToFavouritesAct());
            return v;
        }
        break;
    default:
        break;
    }
    return QVariant();
}

Qt::ItemFlags StreamSearchModel::flags(const QModelIndex &index) const
{
    if (index.isValid()) {
        if (toItem(index)->isCategory()) {
            return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
        } else {
            return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
        }
    } else {
        return Qt::NoItemFlags;
    }
}

bool StreamSearchModel::hasChildren(const QModelIndex &index) const
{
    return index.isValid() ? toItem(index)->isCategory() : true;
}

bool StreamSearchModel::canFetchMore(const QModelIndex &index) const
{
    if (index.isValid()) {
        StreamsModel::Item *item = toItem(index);
        return item->isCategory() && StreamsModel::CategoryItem::Initial==static_cast<StreamsModel::CategoryItem *>(item)->state && !item->url.isEmpty();
    } else {
        return false;
    }
}

void StreamSearchModel::fetchMore(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }

    StreamsModel::Item *item = toItem(index);
    if (item->isCategory() && !item->url.isEmpty()) {
        StreamsModel::CategoryItem *cat=static_cast<StreamsModel::CategoryItem *>(item);
        NetworkJob *job=NetworkAccessManager::self()->get(cat->url);
        if (jobs.isEmpty()) {
            emit loading();
        }
        jobs.insert(job, cat);
        connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
        cat->state=StreamsModel::CategoryItem::Fetching;
        emit dataChanged(index, index);
    }
}

QStringList StreamSearchModel::filenames(const QModelIndexList &indexes, bool addPrefix) const
{
    QStringList fnames;
    foreach(QModelIndex index, indexes) {
        StreamsModel::Item *item=static_cast<StreamsModel::Item *>(index.internalPointer());
        if (!item->isCategory() && !fnames.contains(item->url)) {
            fnames << StreamsModel::modifyUrl(item->url, addPrefix, item->name);
        }
    }

    return fnames;
}

QMimeData * StreamSearchModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    PlayQueueModel::encode(*mimeData, PlayQueueModel::constFileNameMimeType, filenames(indexes, true));
    return mimeData;
}

QStringList StreamSearchModel::mimeTypes() const
{
    QStringList types;
    types << PlayQueueModel::constFileNameMimeType;
    return types;
}

void StreamSearchModel::clear()
{
    cancelAll();
    foreach (StreamsModel::Item *item, root->children) {
        StreamsModel::CategoryItem *cat=static_cast<StreamsModel::CategoryItem *>(item);
        QModelIndex index=createIndex(root->children.indexOf(cat), 0, (void *)cat);
        beginRemoveRows(index, 0, cat->children.count()-1);
        qDeleteAll(cat->children);
        cat->children.clear();
        endRemoveRows();
    }
    currentSearch=QString();
}

static int getParam(const QString &key, QString &query)
{
    int index=query.indexOf(" "+key+"=");
    int val=0;
    if (-1!=index) {
        int endPos=query.indexOf(" ", index+3);
        int end=endPos;
        if (-1==end) {
            end=query.length()-1;
        }
        int start=index+key.length()+2;
        val=query.mid(start, (end+1)-start).toInt();
        if (endPos>start) {
            query=query.left(index)+query.mid(endPos);
            query=query.trimmed();
        } else {
            query=query.left(index);
        }
    }
    return val;
}

void StreamSearchModel::search(const QString &searchTerm, bool stationsOnly)
{
    if (searchTerm==currentSearch) {
        return;
    }
    clear();
    currentSearch=searchTerm;

    foreach (StreamsModel::Item *item, root->children) {
        QUrl searchUrl;
        #if QT_VERSION < 0x050000
        QUrl &query=searchUrl;
        #else
        QUrlQuery query;
        #endif
        switch (root->children.indexOf(item)) {
        case TuneIn: {
            searchUrl=QUrl(item->url);
            if (stationsOnly) {
                query.addQueryItem("types", "station");
            }
            query.addQueryItem("query", searchTerm);
            QString locale=QLocale::system().name();
            if (!locale.isEmpty()) {
                query.addQueryItem("locale", locale);
            }
            break;
        }
        case ShoutCast: {
            searchUrl=QUrl(item->url);
            QString search=searchTerm;
            int limit=getParam("limit", search);
            int bitrate=getParam("br", search);
            if (0==bitrate) {
                bitrate=getParam("bitrate", search);
            }
            query.addQueryItem("k", StreamsModel::constShoutCastApiKey);
            query.addQueryItem("search", search);
            query.addQueryItem("limit", QString::number(limit<1 ? 100 : limit));
            if (bitrate>=32 && bitrate<=512) {
                query.addQueryItem("br", QString::number(bitrate));
            }
            break;
        }
        case Dirble:
            searchUrl=QUrl(item->url+searchTerm);
            query.addQueryItem("token", StreamsModel::constDirbleApiKey);
            break;
        }

        #if QT_VERSION >= 0x050000
        searchUrl.setQuery(query);
        #endif
        NetworkJob *job=NetworkAccessManager::self()->get(searchUrl);
        if (jobs.isEmpty()) {
            emit loading();
        }
        jobs.insert(job, static_cast<StreamsModel::CategoryItem *>(item));
        connect(job, SIGNAL(finished()), this, SLOT(jobFinished()));
    }
}

void StreamSearchModel::cancelAll()
{
    if (!jobs.isEmpty()) {
        QList<NetworkJob *> jobList=jobs.keys();
        foreach (NetworkJob *j, jobList) {
            j->cancelAndDelete();
        }
        jobs.clear();
        emit loaded();
    }
}

void StreamSearchModel::jobFinished()
{
    NetworkJob *job=dynamic_cast<NetworkJob *>(sender());

    if (!job) {
        return;
    }

    job->deleteLater();
    if (jobs.contains(job)) {
        StreamsModel::CategoryItem *cat=jobs[job];
        cat->state=StreamsModel::CategoryItem::Fetched;
        jobs.remove(job);
        QModelIndex index=cat==root ? QModelIndex() : createIndex(cat->parent->children.indexOf(cat), 0, (void *)cat);
        if (job->ok()) {
            QList<StreamsModel::Item *> newItems;

            StreamsModel::Item *i=cat;
            while (i->parent && i->parent!=root) {
                i=i->parent;
            }
            switch(root->children.indexOf(i)) {
            case TuneIn:    newItems=StreamsModel::parseRadioTimeResponse(job->actualJob(), cat, true); break;
            case ShoutCast: newItems=StreamsModel::parseShoutCastSearchResponse(job->actualJob(), cat); break;
            case Dirble:    newItems=StreamsModel::parseDirbleStations(job->actualJob(), cat); break;
            default : break;
            }
            if (!newItems.isEmpty()) {
                beginInsertRows(index, cat->children.count(), (cat->children.count()+newItems.count())-1);
                cat->children+=newItems;
                endInsertRows();
            }
        }
        emit dataChanged(index, index);
        if (jobs.isEmpty()) {
            emit loaded();
        }
    }
}

QList<StreamsModel::Item *> StreamSearchModel::getStreams(StreamsModel::CategoryItem *cat)
{
    QList<StreamsModel::Item *> streams;
    if (cat) {
        foreach (StreamsModel::Item *i, cat->children) {
            if (i->isCategory()) {
                streams+=getStreams(static_cast<StreamsModel::CategoryItem *>(i));
            } else {
                streams.append(i);
            }
        }
    }
    return streams;
}
