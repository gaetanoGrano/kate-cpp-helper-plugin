/**
 * \file
 *
 * \brief Class \c kate::SearchResultsTableModel (implementation)
 *
 * \date Mon Oct 28 09:34:39 MSK 2013 -- Initial design
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

// Project specific includes
#include <src/search_results_table_model.h>
#include <src/database_manager.h>

// Standard includes
#include <KDE/KLocalizedString>

namespace kate {

SearchResultsTableModel::SearchResultsTableModel(DatabaseManager& db_mgr)
  : m_db_mgr(db_mgr)
{
}

int SearchResultsTableModel::columnCount(const QModelIndex&) const
{
    return column::last__;
}

int SearchResultsTableModel::rowCount(const QModelIndex&) const
{
    return m_results.size();
}

QModelIndex SearchResultsTableModel::index(
    const int row
  , const int col
  , const QModelIndex& parent
  ) const
{
    if (!parent.isValid() && std::size_t(row) < m_results.size() && col < column::last__)
        return createIndex(row, col, 0);
    return QModelIndex();
}

QModelIndex SearchResultsTableModel::parent(const QModelIndex&) const
{
    return QModelIndex();
}

QVariant SearchResultsTableModel::data(const QModelIndex& index, const int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant{};

    assert("Sanity check" && std::size_t(index.row()) < m_results.size());

    auto result = QVariant{};
    switch (index.column())
    {
        case column::NAME:
            result = m_results[index.row()].m_name;
            break;
        case column::KIND:
        {
            auto kind_str = QString{};
            if (m_results[index.row()].m_flags.m_static)
                kind_str = "static ";
            switch (m_results[index.row()].m_template_kind)
            {
                case CXIdxEntity_Template:
                    kind_str += "template ";
                    break;
                case CXIdxEntity_TemplatePartialSpecialization:
                    kind_str += "partly specialized template ";
                    break;
                case CXIdxEntity_TemplateSpecialization:
                    kind_str += "fully specialized template ";
                    break;
                default:
                    break;
            }
            result = QString{kind_str + toString(m_results[index.row()].m_kind)};
            break;
        }
        default:
            break;
    }
    return result;
}

QVariant SearchResultsTableModel::headerData(
    const int section
  , const Qt::Orientation orientation
  , const int role
  ) const
{
    if (role == Qt::DisplayRole)
    {
        if (orientation == Qt::Horizontal)
        {
            switch (section)
            {
                case column::KIND:
                    return QString{i18nc("@title:column", "Kind")};
                case column::NAME:
                    return QString{i18nc("@title:column", "Name")};
                default:
                    break;
            }
        }
        else if (orientation == Qt::Vertical)
        {
            return QString::number(section + 1);
        }
    }
    return QVariant{};
}

Qt::ItemFlags SearchResultsTableModel::flags(const QModelIndex&) const
{
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

}                                                           // namespace kate
