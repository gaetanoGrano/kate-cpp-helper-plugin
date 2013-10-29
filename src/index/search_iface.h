/**
 * \file
 *
 * \brief Class \c kate::index::search_iface (interface)
 *
 * \date Sat Oct 26 11:21:52 MSK 2013 -- Initial design
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

// Standard includes
#include <vector>

class QString;

namespace kate { namespace index {

struct search_result {};

/**
 * \brief Interface class to searchable index
 */
class search_iface
{
public:
    /// Destructor
    virtual ~search_iface() {}

    virtual std::vector<search_result> search(const QString&) = 0;
};

}}                                                          // namespace index, kate