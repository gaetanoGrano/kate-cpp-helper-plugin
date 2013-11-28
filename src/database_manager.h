/**
 * \file
 *
 * \brief Class \c kate::DatabaseManager (interface)
 *
 * \date Sun Oct 13 08:47:24 MSK 2013 -- Initial design
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
#include <src/index/database.h>
#include <src/clang/compiler_options.h>
#include <src/index/combined_index.h>
#include <src/index/search_result.h>
#include <src/indexing_targets_list_model.h>
#include <src/indices_table_model.h>
#include <src/database_options.h>
#include <src/diagnostic_messages_model.h>

// Standard includes
#include <boost/filesystem/path.hpp>
#include <boost/uuid/uuid.hpp>
#include <KDE/KTextEditor/Cursor>
#include <KDE/KUrl>
#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <memory>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

class QAbstractTableModel;
class QAbstractListModel;
class QModelIndex;

namespace kate { namespace index {
class indexer;                                              // fwd decl
}                                                           // namespace index

/**
 * \brief Manage databases used by current session
 *
 * [More detailed description here]
 *
 */
class DatabaseManager : public QObject
{
    Q_OBJECT

    /// Delete copy ctor
    DatabaseManager(const DatabaseManager&) = delete;
    /// Delete copy-assign operator
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    /// Delete move ctor
    DatabaseManager(DatabaseManager&&) = delete;
    /// Delete move-assign operator
    DatabaseManager& operator=(DatabaseManager&&) = delete;

public:
    /// Default constructor
    DatabaseManager();
    /// Destructor
    ~DatabaseManager();

    struct exception : public std::runtime_error
    {
        struct invalid_manifest;
        explicit exception(const std::string&);
    };

    /// Obtain a table model for currently configured indices
    QAbstractItemModel* getDatabasesTableModel();
    /// Obtain a list model for currently configured targets
    QAbstractListModel* getTargetsListModel();
    /// Obtain a table model for search results
    QAbstractItemModel* getSearchResultsTableModel();

    /// Reset everything using this params
    void reset(const std::set<boost::uuids::uuid>&, const KUrl& = DatabaseManager::getDefaultBaseDir());
    /// Set compiler optoins for indexer
    void setCompilerOptions(clang::compiler_options&&);
    /// Do search request, get results
    std::vector<index::search_result> startSearchGetResults(QString);

public Q_SLOTS:
    void enable(const QString&, bool);
    void createNewIndex();
    void removeCurrentIndex();
    void stopIndexer();
    void rebuildCurrentIndex();
    void rebuildFinished();
    void refreshCurrentTargets(const QModelIndex&);
    void selectCurrentTarget(const QModelIndex&);
    void addNewTarget();
    void removeCurrentTarget();
    void reportCurrentFile(QString);
    void reportIndexingError(clang::diagnostic_message);
    void indexLocalsToggled(bool);
    void indexImplicitsToggled(bool);

Q_SIGNALS:
    void indexStatusChanged(const QString&, bool);
    void diagnosticMessage(clang::diagnostic_message);
    void reindexingStarted(const QString&);
    void reindexingFinished(const QString&);
    void setIndexLocalsChecked(bool);
    void setSkipImplicitsChecked(bool);

private:
    friend class IndicesTableModel;
    friend class IndexingTargetsListModel;

    struct database_state
    {
        enum class status
        {
            unknown
          , ok
          , invalid
          , reindexing
        };
        std::unique_ptr<DatabaseOptions> m_options;
        std::unique_ptr<index::ro::database> m_db;
        status m_status = {status::unknown};
        boost::uuids::uuid m_id;
        bool m_enabled = {false};

        database_state() = default;                         ///< Default initialization
        database_state(database_state&&) = default;         ///< Default move constructor
        /// Default move-assign operator
        database_state& operator=(database_state&&) = default;
        ~database_state();

        bool isOk() const;
        void loadMetaFrom(const QString&);
    };

    typedef std::vector<database_state> collections_type;

    static KUrl getDefaultBaseDir();
    database_state tryLoadDatabaseMeta(const boost::filesystem::path&);
    void enable(int, bool);
    bool isEnabled(int) const;
    void renameCollection(int, const QString&);
    index::search_result makeSearchResult(const index::document&);
    void reportError(const QString& = QString{}, int = -1, bool = false);
    const database_state& findIndexByID(const index::dbid) const;

    KUrl m_base_dir;
    IndicesTableModel m_indices_model;
    IndexingTargetsListModel m_targets_model;
    collections_type m_collections;
    std::set<boost::uuids::uuid> m_enabled_list;
    clang::compiler_options m_compiler_options;
    std::unique_ptr<index::indexer> m_indexer;
    index::combined_index m_search_db;
    int m_last_selected_index;
    int m_last_selected_target;
    int m_indexing_in_progress;
};

struct DatabaseManager::exception::invalid_manifest : public DatabaseManager::exception
{
    invalid_manifest(const std::string& str) : DatabaseManager::exception(str) {}
};

inline DatabaseManager::exception::exception(const std::string& str)
  : std::runtime_error(str)
{}

inline QAbstractItemModel* DatabaseManager::getDatabasesTableModel()
{
    return &m_indices_model;
}

inline QAbstractListModel* DatabaseManager::getTargetsListModel()
{
    return &m_targets_model;
}

inline void DatabaseManager::setCompilerOptions(clang::compiler_options&& options)
{
    m_compiler_options = std::move(options);
}

}                                                           // namespace kate
// kate: hl C++11/Qt4;
