/**
 * \file
 *
 * \brief Class \c kate::TranslationUnit (implementation)
 *
 * \date Thu Nov 22 18:07:27 MSK 2012 -- Initial design
 */
/*
 * KateIncludeHelperPlugin is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * KateIncludeHelperPlugin is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// Project specific includes
#include <src/translation_unit.h>
#include <src/clang_utils.h>

// Standard includes
#include <cassert>

namespace kate { namespace {
/// Show some debug info about completion string from Clang
void debugShowCompletionResult(
    const unsigned i
  , const unsigned priority
  , const CXCompletionString str
  , const CXCursorKind cursor_kind
  )
{
    kDebug() << ">>> Completion " << i
        << ", priority " << priority
        << ", kind " << toString(cursor_kind).toUtf8().constData();

    // Show info about parent context
    CXCursorKind parent_ck;
    const DCXString parent = clang_getCompletionParent(str, &parent_ck);
    const auto parent_str = clang_getCString(parent);
    kDebug() << "  parent: " << (parent_str ? parent_str : "<none>")
      << ',' << toString(parent_ck).toUtf8().constData();

    // Show individual chunks
    for (unsigned ci = 0, cn = clang_getNumCompletionChunks(str); ci < cn; ++ci)
    {
        auto kind = clang_getCompletionChunkKind(str, ci);
        DCXString text_str = clang_getCompletionChunkText(str, ci);
        kDebug() << "  chunk [" << i << ':' << ci << "]: " << toString(kind).toUtf8().constData()
          << ", text=" << QString(clang_getCString(text_str));
    }

    // Show annotations
    for (unsigned ai = 0u, an = clang_getCompletionNumAnnotations(str); ai < an; ++ai)
    {
        auto ann_str = clang_getCompletionAnnotation(str, an);
        kDebug() << "  ann. text[" << ai << "]: " << QString(clang_getCString(ann_str));
    }

    // Show availability
    const char* av = "unknown";
    switch (clang_getCompletionAvailability(str))
    {
        case CXAvailability_Available:
            av = "available";
            break;
        case CXAvailability_Deprecated:
            av = "deprecated";
            break;
        case CXAvailability_NotAvailable:
            av = "not available";
            break;
        case CXAvailability_NotAccessible:
            av = "not accessible";
            break;
    }
    kDebug() << "  availability: " <<  av;
    kDebug() << ">>> -----------------------------------";
}

}                                                           // anonymous namespace

/**
 * This constructor can be used to load previously translated and saved unit:
 * the typical way is to load a PCH files
 */
TranslationUnit::TranslationUnit(
    CXIndex index
  , const KUrl& filename_url
  , const QVector<QPair<QString, QString>>& unsaved_files
  )
  : m_filename(filename_url.toLocalFile().toUtf8())
  , m_unit(clang_createTranslationUnit(index, m_filename.constData()))
{
    if (!m_unit)
        throw Exception::LoadFailure("Fail to load a preparsed file");

    updateUnsavedFiles(unsaved_files);

    auto result = clang_reparseTranslationUnit(
        m_unit
      , m_unsaved_files.size()
      , m_unsaved_files.data()
      , 0
      );
    if (result)
        throw Exception::ReparseFailure("It seems preparsed file is invalid");
}

TranslationUnit::TranslationUnit(
    CXIndex index
  , const KUrl& filename_url
  , const QStringList& options
  , const ParseOptions parse_options
  , const QVector<QPair<QString, QString>>& unsaved_files
  )
  : m_filename(filename_url.toLocalFile().toUtf8())
{
    kDebug() << "Making a translation unit for" << filename_url.toLocalFile();
    kDebug() << "w/ the following compiler options:" << options;

    // Transform options compatible to clang API
    // NOTE Too fraking much actions for that simple task...
    // Definitely Qt doesn't suitable for that sort of tasks...
    std::vector<QByteArray> utf8_options(options.size());
    std::vector<const char*> clang_options(options.size(), nullptr);
    {
        int opt_idx = 0;
        for (const auto& o : options)
        {
            utf8_options[opt_idx] = o.toUtf8();
            clang_options[opt_idx] = utf8_options[opt_idx].constData();
            opt_idx++;
        }
    }

    // Setup internal structures w/ unsaved files
    updateUnsavedFiles(unsaved_files);

    // Ok, ready to parse
    m_unit = clang_parseTranslationUnit(
        index
      , m_filename.constData()
      , clang_options.data()
      , clang_options.size()
      , m_unsaved_files.data()
      , m_unsaved_files.size()
      , static_cast<unsigned>(parse_options)
      );
    if (!m_unit)
        throw Exception::ParseFailure("Failure to parse C++ code");
}

void TranslationUnit::updateUnsavedFiles(const QVector<QPair<QString, QString>>& unsaved_files)
{
    // Resize internal vectors according expected items count
    m_unsaved_files_utf8.resize(unsaved_files.size());
    m_unsaved_files.resize(unsaved_files.size());
    // Transform unsaved files compatible to clang API
    int uf_idx = 0;
    for (const auto& p : unsaved_files)
    {
        m_unsaved_files_utf8[uf_idx] = {p.first.toUtf8(), p.second.toUtf8()};
        m_unsaved_files[uf_idx].Filename = m_unsaved_files_utf8[uf_idx].first.constData();
        m_unsaved_files[uf_idx].Contents = m_unsaved_files_utf8[uf_idx].second.constData();
        /// \note Fraking \c QByteArray has \c int as return type of \c size()! IDIOTS!
        m_unsaved_files[uf_idx].Length = unsigned(m_unsaved_files_utf8[uf_idx].second.size());
        uf_idx++;
    }
}

QList<ClangCodeCompletionItem> TranslationUnit::completeAt(const int line, const int column)
{
    QList<ClangCodeCompletionItem> completions;

    DCXCodeCompleteResults res = clang_codeCompleteAt(
        m_unit
      , m_filename.constData()
      , unsigned(line)
      , unsigned(column)
      , m_unsaved_files.data()
      , m_unsaved_files.size()
      , 0
      );
    if (!res)
        throw std::runtime_error("Can't complete");

    clang_sortCodeCompletionResults(res->Results, res->NumResults);

    // Show some diagnostic SPAM
    for (unsigned i = 0; i < clang_codeCompleteGetNumDiagnostics(res); ++i)
    {
        DCXDiagnostic diag = clang_codeCompleteGetDiagnostic(res, i);
        DCXString s = clang_getDiagnosticSpelling(diag);
        kWarning() << "Clang WARNING: " << clang_getCString(s);
    }

    completions.reserve(res->NumResults);                   // Peallocate enough space for completion results

    // Lets look what we've got...
    for (unsigned i = 0; i < res->NumResults; ++i)
    {
        const auto str = res->Results[i].CompletionString;
        const auto priority = clang_getCompletionPriority(str);
        const auto cursor_kind = res->Results[i].CursorKind;
        debugShowCompletionResult(i, priority, str, cursor_kind);

        // Skip unusable completions
        // 0) check availability
        const auto av = clang_getCompletionAvailability(str);
        if (av != CXAvailability_Available && av != CXAvailability_Deprecated)
        {
            kDebug() << "!! Skip result" << i << "as not available";
            continue;
        }
        // 1) check usefulness
        /// \todo Make it configurable
        if (cursor_kind == CXCursor_NotImplemented)
            continue;

        // Collect all completion chunks and from a format string
        QString text_before;
        QString typed_text;
        QString text_after;
        QStringList placeholders;
        // A lambda to append given text to different parts
        // of future completion string, depending on already processed text
        auto appender = [&](const QString& text)
        {
            if (typed_text.isEmpty())
                text_before += text;
            else
                text_after += text;
        };
        for (unsigned j = 0; j < clang_getNumCompletionChunks(str); ++j)
        {
            auto kind = clang_getCompletionChunkKind(str, j);
            DCXString text_str = clang_getCompletionChunkText(str, j);
            QString text = clang_getCString(text_str);
            switch (kind)
            {
                // Text that a user would be expected to type to get this code-completion result
                case CXCompletionChunk_TypedText:
                // Text that should be inserted as part of a code-completion result
                case CXCompletionChunk_Text:
                    typed_text += text;
                    break;
                // Placeholder text that should be replaced by the user
                case CXCompletionChunk_Placeholder:
                    appender("%" + QString::number(placeholders.size() + 1) + "%");
                    placeholders.push_back(text);
                    break;
                case CXCompletionChunk_ResultType:
                case CXCompletionChunk_LeftParen:
                case CXCompletionChunk_RightParen:
                case CXCompletionChunk_LeftBracket:
                case CXCompletionChunk_RightBracket:
                case CXCompletionChunk_LeftBrace:
                case CXCompletionChunk_RightBrace:
                case CXCompletionChunk_LeftAngle:
                case CXCompletionChunk_RightAngle:
                case CXCompletionChunk_Comma:
                case CXCompletionChunk_Colon:
                case CXCompletionChunk_SemiColon:
                case CXCompletionChunk_Equal:
                case CXCompletionChunk_HorizontalSpace:
                case CXCompletionChunk_VerticalSpace:
                    appender(text);
                    break;
                // Informative text that should be displayed but never inserted
                // as part of the template
                case CXCompletionChunk_Informative:
                case CXCompletionChunk_CurrentParameter:
                // A code-completion string that describes "optional" text that
                // could be a part of the template (but is not required)
                case CXCompletionChunk_Optional:
                    /// \todo Extract and append an options text (default parameters)
                default:
                    break;
            }
        }
        assert("Priority expected to be less than 100" && priority < 101u);
        completions.push_back({text_before, typed_text, text_after, placeholders, priority, cursor_kind});
    }

    return completions;
}

void TranslationUnit::storeTo(const KUrl& filename)
{
    QByteArray pch_filename = filename.toLocalFile().toUtf8();
    auto result = clang_saveTranslationUnit(m_unit, pch_filename.constData(), CXSaveTranslationUnit_None);
    kDebug() << "sore result=" << result;
    if (result != CXSaveError_None)
        throw Exception::SaveFailure("Failure on save a PCH file");
}

}                                                           // namespace kate
// kate: hl C++11/Qt4;
