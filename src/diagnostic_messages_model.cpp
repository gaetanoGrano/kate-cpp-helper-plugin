/**
 * \file
 *
 * \brief Class \c kate::DiagnosticMessagesModel (implementation)
 *
 * \date Sun Aug 11 04:42:02 MSK 2013 -- Initial design
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
#include <src/diagnostic_messages_model.h>

// Standard includes
#include <KDE/KDebug>
#include <QtGui/QBrush>

namespace kate {

int DiagnosticMessagesModel::rowCount(const QModelIndex&) const
{
    return static_cast<int>(m_records.size());
}

int DiagnosticMessagesModel::columnCount(const QModelIndex&) const
{
    return 2;
}

QVariant DiagnosticMessagesModel::data(const QModelIndex& index, const int role) const
{
    switch (role)
    {
        case Qt::DisplayRole:
#if 0
            kDebug(DEBUG_AREA) << "Getting diag data: row=" << index.row() << ", col=" << index.column();
#endif
            // Check if requested item has a source code location
            // (m_file member must not be empty)
            if (!m_records[index.row()].m_location.empty())
                // Form a compiler-like SPAM
                return QString("%1:%2:%3: %4").arg(
                    m_records[index.row()].m_location.file().toLocalFile()
                , QString::number(m_records[index.row()].m_location.line())
                , QString::number(m_records[index.row()].m_location.column())
                , m_records[index.row()].m_text
                );
            // Just return a message text
            return m_records[index.row()].m_text;
        case Qt::ForegroundRole:
            switch (m_records[index.row()].m_type)
            {
                case clang::diagnostic_message::type::error:
                    return QBrush(Qt::red);
                case clang::diagnostic_message::type::warning:
                    return QBrush(Qt::yellow);
                case clang::diagnostic_message::type::info:
                case clang::diagnostic_message::type::debug:
                default:
                    break;
            }
            break;
        default:
            break;
    }
    return QVariant();
}

void DiagnosticMessagesModel::clear()
{
    beginResetModel();
    m_records.clear();
    endResetModel();
}

}                                                           // namespace kate
// kate: hl C++11/Qt4;
