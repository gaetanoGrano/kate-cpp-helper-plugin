/**
 * \file
 *
 * \brief Class \c kate::utils (implementation)
 *
 * \date Mon Feb  6 04:12:51 MSK 2012 -- Initial design
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

// Project specific includes
#include <src/utils.h>

// Standard includes
#include <boost/range/algorithm/find.hpp>
#include <KDE/KDebug>
#include <KDE/KTextEditor/Range>
#include <algorithm>
#include <cassert>
#include <vector>

/// \internal Turn debug SPAM from \c #include parser
#define ENABLE_INCLUDE_PARSER_SPAM 0

namespace kate { namespace {
const char* const INCLUDE_STR = "include";
const std::vector<QString> SUITABLE_DOCUMENT_TYPES = {
    QString("text/x-c++src")
  , QString("text/x-c++hdr")
  , QString("text/x-csrc")
  , QString("text/x-chdr")
};
const std::vector<QString> SUITABLE_HIGHLIGHT_TYPES = {
    QString("C++")
  , QString("C++11")
  , QString("C++/Qt4")
  , QString("C")
};
}                                                           // anonymous namespace

/**
 * Return a range w/ filename if given line contains a valid \c #include
 * directive, empty range otherwise.
 *
 * \param line a string w/ line to parse
 * \param strict return failure if closing \c '>' or \c '"' missed
 *
 * \return instance of \c IncludeParseResult
 *
 * \warning Line would always 0 in a range! U have to set it after call this function!
 */
IncludeParseResult parseIncludeDirective(const QString& line, const bool strict)
{
#if ENABLE_INCLUDE_PARSER_SPAM
    kDebug(DEBUG_AREA) << "text2parse=" << line << ", strict=" << strict;
#endif

    enum State
    {
        skipOptionalInitialSpaces
      , foundHash
      , checkInclude
      , skipSpace
      , skipOptionalSpaces
      , foundOpenChar
      , findCloseChar
      , stop
    };

    // Perpare 'default' result
    IncludeParseResult result;

    int start = -1;
    int end = -1;
    int tmp = 0;
    QChar close = 0;
    State state = skipOptionalInitialSpaces;
    for (int pos = 0; pos < line.length() && state != stop; ++pos)
    {
        switch (state)
        {
            case skipOptionalInitialSpaces:
                if (!line[pos].isSpace())
                {
                    if (line[pos] == QLatin1Char('#'))
                    {
                        state = foundHash;
                        continue;
                    }
#if ENABLE_INCLUDE_PARSER_SPAM
                    kDebug(DEBUG_AREA) << "pase failure: smth other than '#' first char in a line";
#endif
                    return result;                          // Error: smth other than '#' first char in a line
                }
                break;
            case foundHash:
                if (line[pos].isSpace())
                    continue;
                else
                    state = checkInclude;
                // NOTE No `break' here!
            case checkInclude:
                if (INCLUDE_STR[tmp++] != line[pos])
                {
#if ENABLE_INCLUDE_PARSER_SPAM
                    kDebug(DEBUG_AREA) << "pase failure: is not 'include' after '#'";
#endif
                    return result;                          // Error: is not 'include' after '#'
                }
                if (tmp == 7)
                    state = skipSpace;
                break;
            case skipSpace:
                if (line[pos].isSpace())
                {
                    state = skipOptionalSpaces;
                    continue;
                }
#if ENABLE_INCLUDE_PARSER_SPAM
                kDebug(DEBUG_AREA) << "pase failure: is not no space after '#include'";
#endif
                return result;                              // Error: no space after '#include'
            case skipOptionalSpaces:
                if (line[pos].isSpace())
                    continue;
                // Check open char type
                if (line[pos] == QLatin1Char('<'))
                {
                    close = QLatin1Char('>');
                    result.type = IncludeStyle::global;
                }
                else if (line[pos] == QLatin1Char('"'))
                {
                    close = QLatin1Char('"');
                    result.type = IncludeStyle::local;
                }
                else
                {
#if ENABLE_INCLUDE_PARSER_SPAM
                    kDebug(DEBUG_AREA) << "pase failure: not a valid open char";
#endif
                    return result;
                }
                state = foundOpenChar;
                break;                                      // NOTE We have to move to next char (if remain smth)
            case foundOpenChar:
                state = findCloseChar;
                start = pos;
                end = pos;
                // NOTE No `break' here!
            case findCloseChar:
                if (line[pos] == close)
                {
                    // Do not allow empty range between open and close char in strict mode
                    if (start == pos && strict)
                    {
#if ENABLE_INCLUDE_PARSER_SPAM
                        kDebug(DEBUG_AREA) << "pase failure: empty filename";
#endif
                        return result;                      // in strict mode return false for incomplete #include
                    }
                    result.is_complete = true;              // Found close char! #include complete...
                    state = stop;
                    end = pos;
                }
                else if (line[pos].isSpace())
                {
                    if (strict)
                    {
#if ENABLE_INCLUDE_PARSER_SPAM
                        kDebug(DEBUG_AREA) << "pase failure: space before close char met";
#endif
                        return result;                      // in strict mode return false for incomplete #include
                    }
                    state = stop;                           // otherwise, it is Ok to have incomplete string...
                    end = pos;
                }
                break;
            case stop:
            default:
                assert(!"Parsing FSM broken!");
        }
    }
    // Check state after EOL occurs
    switch (state)
    {
        case foundOpenChar:
            if (!strict)
                result.range = KTextEditor::Range(0, line.length(), 0, line.length());
#if ENABLE_INCLUDE_PARSER_SPAM
            kDebug(DEBUG_AREA) << "pase failure: EOL after open char";
#endif
            break;
        case findCloseChar:
            if (!strict)
                result.range = KTextEditor::Range(0, start, 0, line.length());
#if ENABLE_INCLUDE_PARSER_SPAM
            kDebug(DEBUG_AREA) << "pase failure: EOL before close char";
#endif
            break;
        case stop:
            result.range = KTextEditor::Range(0, start, 0, end);
            break;
        case skipSpace:
        case skipOptionalInitialSpaces:
        case foundHash:
        case checkInclude:
        case skipOptionalSpaces:
            break;
        default:
            assert(!"Parsing FSM broken!");
    }
#if ENABLE_INCLUDE_PARSER_SPAM
    kDebug(DEBUG_AREA) << "result-range=" << result.range <<
        ", is_complete=" << result.is_complete <<
      ;
#endif
    return result;
}

/**
 * This function used to guess is user started to type \c #include directive...
 */
QString tryToCompleteIncludeDirective(const QString& text)
{
    // Check if user typed at least '#in' -- there is no other preprocessor
    // directives than '#include' -- so we can `complete` it as well :)
    enum struct State
    {
        initial
      , skip_possible_spaces
      , skip_expected_letter
      , fail_if_not_end_of_string
      , end_ok
      , end_failed
    };
    State state = State::initial;
    int pos = 0;
    int first_non_space_pos = 0;
    for (
        int i = 0, last = text.length()
      ; i < last && state != State::end_ok && state != State::end_failed
      ; ++i
      )
    {
        switch (state)
        {
            case State::initial:
                if (text[i] == QLatin1Char('#'))
                    state = State::skip_possible_spaces;
                else
                    state = State::end_failed;
                break;
            case State::skip_possible_spaces:
                if (text[i].isSpace())
                    break;
                state = State::skip_expected_letter;
                first_non_space_pos = i;
            case State::skip_expected_letter:
                if (text[i] != QLatin1Char(INCLUDE_STR[pos++]))
                    state = State::end_failed;
                else if (pos == 7)
                    state = State::fail_if_not_end_of_string;
                break;
            case State::fail_if_not_end_of_string:
                state = State::end_failed;
                break;
            default:
                assert(!"Unexpected State! Review your buggy code!");
                break;
        }
    }
    // Check that at least '#in' string was processed...
    assert("position expected to be >= 0" && pos >= 0);
    if ((state == State::skip_expected_letter && 1 < pos) || state == State::fail_if_not_end_of_string)
        state = State::end_ok;

    // Form a completion result string
    QString result;
    if (state == State::end_ok)
    {
        result = text.mid(0, first_non_space_pos + pos + 1);
        if (pos < 7)
            result += (INCLUDE_STR + pos);
    }
    return result;
}

bool isSuitableDocument(const QString& mime_str, const QString& hl_mode)
{
    auto it = boost::find(SUITABLE_DOCUMENT_TYPES, mime_str);
    if (it == end(SUITABLE_DOCUMENT_TYPES))
    {
        if (mime_str == QLatin1String("text/plain"))
        {
            it = boost::find(SUITABLE_HIGHLIGHT_TYPES, hl_mode);
            return it != end(SUITABLE_HIGHLIGHT_TYPES);
        }
        return false;
    }
    return true;
}

bool isSuitableDocumentAndHighlighting(const QString& mime_str, const QString& hl_mode)
{
    auto it = boost::find(SUITABLE_DOCUMENT_TYPES, mime_str);
    if (it != end(SUITABLE_DOCUMENT_TYPES) || mime_str == QLatin1String("text/plain"))
    {
        it = boost::find(SUITABLE_HIGHLIGHT_TYPES, hl_mode);
        return it != end(SUITABLE_HIGHLIGHT_TYPES);
    }
    return false;
}

/**
 * \todo Is there any way to make a joint view for both containers?
 *
 * \param[in] file filename to look for in the next 2 lists...
 * \param[in] locals per session \c #include search paths list
 * \param[in] system global \c #include search paths list
 * \return list of absolute filenames
 */
QStringList findHeader(const QString& file, const QStringList& locals, const QStringList& system)
{
    QStringList result;
    kDebug(DEBUG_AREA) << "Trying locals first...";
    findFiles(file, locals, result);                        // Try locals first
    kDebug(DEBUG_AREA) << "Trying system paths...";
    findFiles(file, system, result);                        // Then try system paths
    return result;
}

void updateListsFromFS(
    const QString& path                                     ///< Path to append to every dir in a list to scan
  , const QStringList& dirs2scan                            ///< List of directories to scan
  , const QStringList& masks                                ///< Filename masks used for globbing
  , QStringList& dirs                                       ///< Directories list to append to
  , QStringList& files                                      ///< Files list to append to
  , const QStringList& ignore_extensions                    ///< Extensions to ignore
  )
{
    const QDir::Filters common_flags = QDir::NoDotAndDotDot | QDir::CaseSensitive | QDir::Readable;
    for (const auto& d : dirs2scan)
    {
        const auto dir = QDir::cleanPath(d + '/' + path);
        kDebug(DEBUG_AREA) << "Trying " << dir;
        {
            QStringList result = QDir(dir).entryList(masks, QDir::Dirs | common_flags);
            for (const auto& r : result)
            {
                const QString d = r + '/';
                if (!dirs.contains(d)) dirs.append(d);
            }
        }
        {
            QStringList result = QDir(dir).entryList(masks, QDir::Files | common_flags);
            for (const auto& r : result)
            {
                const auto ignore = std::any_of(
                    std::begin(ignore_extensions)
                  , std::end(ignore_extensions)
                  , [r](const QString& ext) { return r.endsWith(ext); }
                  );
                // Append if a file not in the list yet, and shouldn't be ignored
                if (!files.contains(r) && !ignore)
                    files.append(r);
                else
                    kDebug(DEBUG_AREA) << "Skip " << r;
            }
        }
    }
}

}                                                           // namespace kate
// kate: hl C++/Qt4;
