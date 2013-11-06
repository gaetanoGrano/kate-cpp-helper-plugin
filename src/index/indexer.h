/**
 * \file
 *
 * \brief Class \c kate::index::indexer (interface)
 *
 * \date Wed Oct  9 11:17:12 MSK 2013 -- Initial design
 */
/*
 * Copyright (C) 2011-2013 Alex Turbov, all rights reserved.
 * This is free software. It is licensed for use, modification and
 * redistribution under the terms of the GNU General Public License,
 * version 3 or later <http://gnu.org/licenses/gpl.html>
 *
 * KateCppHelperPlugin is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * KateCppHelperPlugin is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

// Project specific includes
#include <src/clang/disposable.h>
#include <src/clang/location.h>
#include <src/index/database.h>
#include <src/index/search_result.h>
#include <src/index/types.h>

// Standard includes
#include <KDE/KUrl>
#include <memory>
#include <vector>

class QThread;

namespace kate { namespace index { namespace details {
class worker;                                               // fwd decl
}                                                           // namespace details


/**
 * \brief Class to index C/C++ sources into a searchable databse
 *
 * [More detailed description here]
 *
 */
class indexer : public QObject
{
    Q_OBJECT

public:
    enum class status
    {
        stopped
      , running
    };
    /// Construct an indexer from database path
    indexer(dbid, const std::string&);
    /// Cleanup everything
    ~indexer();

    indexer& set_compiler_options(std::vector<const char*>&&);
    indexer& set_indexing_options(unsigned);
    indexer& add_target(const KUrl&);

    static unsigned default_indexing_options();

public Q_SLOTS:
    void start();
    void stop();

private Q_SLOTS:
    void indexing_uri_slot(QString);
    void worker_finished_slot();
    void thread_finished_slot();
    void error_slot(clang::location, QString);

Q_SIGNALS:
    void indexing_uri(QString);
    void finished();
    void error(clang::location, QString);
    void stopping();

private:
    friend class details::worker;

    std::unique_ptr<QThread> m_worker_thread;
    clang::DCXIndex m_index;
    std::vector<const char*> m_options;
    std::vector<KUrl> m_targets;
    rw::database m_db;
    unsigned m_indexing_options = {default_indexing_options()};
};

inline indexer& indexer::set_compiler_options(std::vector<const char*>&& options)
{
    m_options = std::move(options);
    return *this;
}

inline indexer& indexer::add_target(const KUrl& url)
{
    m_targets.emplace_back(url);
    return *this;
}

inline indexer& indexer::set_indexing_options(const unsigned options)
{
    m_indexing_options = options;
    return *this;
}

inline unsigned indexer::default_indexing_options()
{
    return CXIndexOpt_SkipParsedBodiesInSession | CXIndexOpt_SuppressWarnings;
}

}}                                                          // namespace index, kate
