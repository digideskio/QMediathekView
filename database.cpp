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

#include "database.h"

#include <fstream>
#include <vector>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/date_time/gregorian/formatters.hpp>
#include <boost/date_time/gregorian/greg_serialize.hpp>
#include <boost/date_time/posix_time/time_serialize.hpp>
#include <boost/serialization/vector.hpp>

#include <QDebug>
#include <QStandardPaths>

#include <QtConcurrentRun>

#include "settings.h"
#include "parser.h"

namespace QMediathekView
{

namespace
{

void collect(
    const std::vector< std::string >& showsByKey, const std::string& key,
    std::vector< quintptr >& id)
{
    for (std::size_t index = 0, count = showsByKey.size(); index < count; ++index)
    {
        if (showsByKey.at(index).find(key) != std::string::npos)
        {
            id.push_back(index);
        }
    }
}

void filter(
    const std::vector< std::string >& showsByKey, const std::string& key,
    std::vector< quintptr >& id)
{
    if (key.empty())
    {
        return;
    }

    id.erase(std::remove_if(id.begin(), id.end(), [&](const quintptr index)
    {
        return showsByKey.at(index).find(key) == std::string::npos;
    }), id.end());
}

template< typename Member >
void sort(
    Member member, const Database::SortOrder sortOrder,
    const std::vector< Show >& shows, std::vector< quintptr >& id)
{
    switch (sortOrder)
    {
    default:
    case Database::SortAscending:
        std::sort(id.begin(), id.end(), [&](const std::size_t lhs, const std::size_t rhs)
        {
            return member(shows.at(lhs)) < member(shows.at(rhs));
        });
        break;
    case Database::SortDescending:
        std::sort(id.begin(), id.end(), [&](const std::size_t lhs, const std::size_t rhs)
        {
            return member(shows.at(rhs)) < member(shows.at(lhs));
        });
        break;
    }
}

void chronologicalSort(
    const std::vector< std::string >& showsByKey, const Database::SortOrder sortOrder,
    const std::vector< Show >& shows, std::vector< quintptr >& id)
{
    switch (sortOrder)
    {
    default:
    case Database::SortAscending:
        std::sort(id.begin(), id.end(), [&](const std::size_t lhs, const std::size_t rhs)
        {
            const auto& lhs_ = shows.at(lhs);
            const auto& rhs_ = shows.at(rhs);

            return operator< (
                       std::tie(showsByKey.at(lhs), rhs_.date, rhs_.time),
                       std::tie(showsByKey.at(rhs), lhs_.date, lhs_.time));
        });
        break;
    case Database::SortDescending:
        std::sort(id.begin(), id.end(), [&](const std::size_t lhs, const std::size_t rhs)
        {
            const auto& lhs_ = shows.at(lhs);
            const auto& rhs_ = shows.at(rhs);

            return operator< (
                       std::tie(showsByKey.at(rhs), rhs_.date, rhs_.time),
                       std::tie(showsByKey.at(lhs), lhs_.date, lhs_.time));
        });
        break;
    }
}

QByteArray databasePath()
{
    const auto path = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    QDir().mkpath(path);
    return QFile::encodeName(QDir(path).filePath("database"));
}

} // anonymous

} // QMediathekView

namespace boost
{
namespace serialization
{

template< typename Archive >
void serialize(Archive& archive, QMediathekView::Show& show, const unsigned int /* version */)
{
    // *INDENT-OFF*

    archive
    & show.channel & show.topic & show.title
    & show.date & show.time
    & show.duration
    & show.description & show.website
    & show.url
    & show.urlSmallOffset & show.urlSmallSuffix
    & show.urlLargeOffset & show.urlLargeSuffix;

    // *INDENT-ON*
}

} // serialization
} // boost

namespace QMediathekView
{

struct Database::Data
{
    std::vector< Show > shows;

    std::vector< std::string > showsByChannel;
    std::vector< std::string > showsByTopic;
    std::vector< std::string > showsByTitle;

    std::vector< std::string > channels;
    std::vector< std::pair< std::string, std::string > > topics;

    void sort()
    {
        shows.shrink_to_fit();

        std::sort(shows.begin(), shows.end(), [](const Show& lhs, const Show& rhs)
        {
            return operator< (
                       std::tie(lhs.channel, rhs.date, rhs.time),
                       std::tie(rhs.channel, lhs.date, lhs.time));
        });
    }

    void index()
    {
        showsByChannel.clear();
        showsByChannel.reserve(shows.size());

        showsByTopic.clear();
        showsByTopic.reserve(shows.size());

        showsByTitle.clear();
        showsByTitle.reserve(shows.size());

        channels.clear();
        topics.clear();

        for (const auto& show : shows)
        {
            showsByTopic.push_back(boost::to_lower_copy(show.topic));
            showsByChannel.push_back(boost::to_lower_copy(show.channel));
            showsByTitle.push_back(boost::to_lower_copy(show.title));

            {
                const auto pos = std::lower_bound(channels.begin(), channels.end(), show.channel);
                if (pos == channels.end() || *pos != show.channel)
                {
                    channels.insert(pos, show.channel);
                }
            }

            {
                auto pair = std::make_pair(boost::to_lower_copy(show.channel), show.topic);
                const auto pos = std::lower_bound(topics.begin(), topics.end(), pair);
                if (pos == topics.end() || *pos != pair)
                {
                    topics.insert(pos, std::move(pair));
                }
            }
        }

        channels.shrink_to_fit();
        topics.shrink_to_fit();
    }

};

class Database::Transaction
{
public:
    Transaction()
        : m_data(std::make_shared< Data >())
    {
    }

    bool load(const char* path)
    {
        try
        {
            std::ifstream file(path, std::ios::binary);
            boost::archive::binary_iarchive archive(file);
            archive >> m_data->shows;

            return true;
        }
        catch (std::exception& exception)
        {
            qDebug() << exception.what();

            return false;
        }
    }

    bool save(const char* path)
    {
        try
        {
            std::ofstream file(path, std::ios::binary);
            boost::archive::binary_oarchive archive(file);
            archive << m_data->shows;

            return true;
        }
        catch (std::exception& exception)
        {
            qDebug() << exception.what();

            return false;
        }
    }

    DataPtr&& commit()
    {
        m_data->sort();
        m_data->index();

        return std::move(m_data);
    }

protected:
    DataPtr m_data;

};

struct Database::FullUpdate
    : public Database::Transaction
    , public Processor
{
    FullUpdate(const DataPtr& /* data */)
    {
    }

    void operator()(const Show& show) override
    {
        m_data->shows.push_back(show);
    }

};

struct Database::PartialUpdate
    : public Database::Transaction
    , public Processor
{
    PartialUpdate(const DataPtr& data)
    {
        m_data->shows = data->shows;
    }

    void operator()(const Show& newShow) override
    {
        const auto pos = std::find_if(m_data->shows.begin(), m_data->shows.end(), [&](const Show& oldShow)
        {
            return operator== (
                       std::tie(newShow.title, newShow.url, newShow.channel, newShow.topic),
                       std::tie(oldShow.title, oldShow.url, oldShow.channel, oldShow.topic));
        });

        if (pos != m_data->shows.end())
        {
            *pos = newShow;
        }
        else
        {
            m_data->shows.push_back(newShow);
        }
    }

};

Database::Database(Settings& settings, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_data(std::make_shared< Data >())
{
    connect(&m_update, &Update::resultReadyAt, this, &Database::updateReady);

    Transaction transaction;

    if (transaction.load(databasePath().constData()))
    {
        m_data = transaction.commit();
    }
}

Database::~Database()
{
    m_update.waitForFinished();
}

void Database::fullUpdate(const QByteArray& data)
{
    update< FullUpdate >(data);
}


void Database::partialUpdate(const QByteArray& data)
{
    update< PartialUpdate >(data);
}

template< typename Transaction >
void Database::update(const QByteArray& data)
{
    if (m_update.isRunning())
    {
        return;
    }

    m_update.setFuture(QtConcurrent::run([this, data]()
    {
        Transaction transaction(m_data);

        if (!parse(data, transaction))
        {
            return DataPtr();
        }

        if (!transaction.save(databasePath().constData()))
        {
            return DataPtr();
        }

        return transaction.commit();
    }));
}

void Database::updateReady(int index)
{
    const auto data = m_update.resultAt(index);

    if (!data)
    {
        emit failedToUpdate("Failed to parse or save data.");

        return;
    }

    m_data = data;

    m_settings.setDatabaseUpdatedOn();

    emit updated();
}

std::vector< quintptr > Database::query(
    std::string channel, std::string topic, std::string title,
    const SortColumn sortColumn, const SortOrder sortOrder) const
{
    std::vector< quintptr > id;

    boost::to_lower(channel);
    boost::to_lower(topic);
    boost::to_lower(title);

    if (!channel.empty())
    {
        collect(m_data->showsByChannel, channel, id);
        filter(m_data->showsByTopic, topic, id);
        filter(m_data->showsByTitle, title, id);
    }
    else if (!topic.empty())
    {
        collect(m_data->showsByTopic, topic, id);
        filter(m_data->showsByChannel, channel, id);
        filter(m_data->showsByTitle, title, id);
    }
    else if (!title.empty())
    {
        collect(m_data->showsByTitle, title, id);
        filter(m_data->showsByChannel, channel, id);
        filter(m_data->showsByTopic, topic, id);
    }
    else
    {
        id.resize(m_data->shows.size());
        std::iota(id.begin(), id.end(), 0);
    }

    switch (sortColumn)
    {
    default:
    case SortChannel:
        switch (sortOrder)
        {
        default:
        case SortAscending:
            break;
        case SortDescending:
            chronologicalSort(m_data->showsByChannel, sortOrder, m_data->shows, id);
            break;
        }
        break;
    case SortTopic:
        chronologicalSort(m_data->showsByTopic, sortOrder, m_data->shows, id);
        break;
    case SortTitle:
        chronologicalSort(m_data->showsByTitle, sortOrder, m_data->shows, id);
        break;
    case SortDate:
        sort(std::mem_fn(&Show::date), sortOrder, m_data->shows, id);
        break;
    case SortTime:
        sort(std::mem_fn(&Show::time), sortOrder, m_data->shows, id);
        break;
    case SortDuration:
        sort(std::mem_fn(&Show::duration), sortOrder, m_data->shows, id);
        break;
    }

    return id;
}

std::shared_ptr< const Show > Database::show(const quintptr id) const
{
    return { m_data, &m_data->shows.at(id) };
}

std::vector< std::string > Database::channels() const
{
    return m_data->channels;
}

std::vector< std::string > Database::topics(std::string channel) const
{
    std::vector< std::string > topics;

    boost::to_lower(channel);

    if (!channel.empty())
    {
        struct Comparator
        {
            bool operator()(const std::pair< std::string, std::string >& pair, const std::string& string) const
            {
                return pair.first < string;
            }

            bool operator()(const std::string& string, const std::pair< std::string, std::string >& pair) const
            {
                return string < pair.first;
            }

        };

        const auto range = std::equal_range(m_data->topics.begin(), m_data->topics.end(), channel, Comparator());

        for (auto iterator = range.first; iterator != range.second; ++iterator)
        {
            topics.push_back(iterator->second);
        }
    }
    else
    {
        topics.reserve(m_data->topics.size());

        for (const auto& pair : m_data->topics)
        {
            topics.push_back(pair.second);
        }
    }

    return topics;
}

} // QMediathekView
