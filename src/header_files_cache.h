/**
 * \file
 *
 * \brief Class \c kate::HeaderFilesCache (interface)
 *
 * \date Thu Dec 27 14:25:24 MSK 2012 -- Initial design
 */
/*
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
#include <src/index/types.h>

// Standard includes
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/serialization/split_member.hpp>
#include <QtCore/QString>
#include <limits>
#include <sstream>
#include <string>

namespace kate {

/**
 * \brief Bidirectinal mapping of \c QString to unique integer ID
 *
 * To resolve a string into ID one can use \c operator[].
 * In case of absent (yet) string an unique ID will be assigned
 * and returned. To resolve ID into a string here is an
 * overload of \c operator[] w/ \c int parameter.
 */
class HeaderFilesCache
{
public:
    typedef index::fileid id_type;

    explicit HeaderFilesCache(id_type start_id = 0)
      : m_current_id{start_id}
      , m_cache_is_dirty{false}
    {}

    /// \name Accessors
    //@{
    const QString operator[](id_type id) const;             ///< Operator to get a string value by ID
    id_type operator[](const QString&) const ;              ///< Operator to get an ID by string
    id_type operator[](const QString&);                     ///< Operator to get an ID by string
    //@}

    enum : id_type
    {
        NOT_FOUND = std::numeric_limits<id_type>::max()
    };

    bool isEmpty() const;
    std::size_t size() const;
    bool isDirty() const;

    std::string storeToString() const;
    void loadFromString(const std::string&);

private:
    /**
     * \brief Structure to associate a filename with an index
     * \todo Not sure that \c QString is a good idea to store
     */
    struct value_type
    {
        QString m_filename;
        id_type m_id;

        template <typename Archive>
        void save(Archive&, const unsigned int) const;

        template <typename Archive>
        void load(Archive&, const unsigned int);

        BOOST_SERIALIZATION_SPLIT_MEMBER()
    };
    struct int_idx;
    struct string_idx;
    /// Type to hold index of header files and assigned IDs
    typedef boost::multi_index_container<
        value_type
      , boost::multi_index::indexed_by<
            boost::multi_index::ordered_unique<
                boost::multi_index::tag<int_idx>
              , boost::multi_index::member<
                    value_type
                  , decltype(value_type::m_id)
                  , &value_type::m_id
                  >
              >
          , boost::multi_index::ordered_unique<
                boost::multi_index::tag<string_idx>
              , boost::multi_index::member<
                    value_type
                  , decltype(value_type::m_filename)
                  , &value_type::m_filename
                  >
              >
          >
      > index_type;

    friend class boost::serialization::access;

    template <typename Archive>
    void serialize(Archive& ar, const unsigned int)
    {
        ar & m_cache;
    }

    index_type m_cache;
    id_type m_current_id;
    mutable bool m_cache_is_dirty;
};

/**
 * \param[in] id identifier to get filename for
 * \return filename for correcponding ID, or empty string if not found
 */
inline const QString HeaderFilesCache::operator[](id_type id) const
{
    QString result;
    auto it = m_cache.get<int_idx>().find(id);
    if (it != end(m_cache.get<int_idx>()))
        result = it->m_filename;
    return result;
}

/**
 * Immutable version of ID access operator.
 *
 * \param[in] filename a filename to get identifier for
 * \return ID of the given value or \c HeaderFilesCache::NOT_FOUND
 */
inline auto HeaderFilesCache::operator[](const QString& filename) const -> id_type
{
    auto result = id_type(NOT_FOUND);
    auto it = m_cache.get<string_idx>().find(filename);
    if (it != end(m_cache.get<string_idx>()))
    {
        result = it->m_id;
    }
    return result;
}

/**
 * Mutable version of ID access operator.
 *
 * \param[in] filename a filename to get identifier for
 * \return ID of the given value (will add a new cache entry if not found)
 */
inline auto HeaderFilesCache::operator[](const QString& filename) -> id_type
{
    auto result = id_type(NOT_FOUND);
    auto it = m_cache.get<string_idx>().find(filename);
    if (it == end(m_cache.get<string_idx>()))
    {
        result = m_current_id++;
        m_cache.insert({filename, result});
        m_cache_is_dirty = true;
    }
    else result = it->m_id;
    return result;
}

inline bool HeaderFilesCache::isEmpty() const
{
    return m_cache.empty();
}

inline std::size_t HeaderFilesCache::size() const
{
    return m_cache.size();
}

inline bool HeaderFilesCache::isDirty() const
{
    return m_cache_is_dirty;
}

inline std::string HeaderFilesCache::storeToString() const
{
    std::stringstream ofs{std::ios_base::out | std::ios_base::binary};
    boost::archive::binary_oarchive oa{ofs};
    oa << m_current_id << m_cache;
    m_cache_is_dirty = false;
    return ofs.str();
}

inline void HeaderFilesCache::loadFromString(const std::string& raw_data)
{
    std::stringstream ifs{raw_data, std::ios_base::in | std::ios_base::binary};
    boost::archive::binary_iarchive ia{ifs};
    ia >> m_current_id >> m_cache;
    m_cache_is_dirty = false;
}

template <typename Archive>
inline void HeaderFilesCache::value_type::save(Archive& ar, const unsigned int) const
{
    std::string filename = m_filename.toUtf8().constData();
    ar & m_id & filename;
}

template <typename Archive>
inline void HeaderFilesCache::value_type::load(Archive& ar, const unsigned int)
{
    std::string filename;
    ar & m_id & filename;
    m_filename = QString(filename.c_str());
}

}                                                           // namespace kate
// kate: hl C++/Qt4;
