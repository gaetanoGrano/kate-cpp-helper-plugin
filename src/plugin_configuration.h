/**
 * \file
 *
 * \brief Class \c kate::PluginConfiguration (interface)
 *
 * \date Fri Nov 23 11:25:46 MSK 2012 -- Initial design
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

// Standard includes
#include <KConfigBase>
#include <KUrl>
#include <QtCore/QStringList>
#include <QtCore/QRegExp>
#include <vector>
#include <utility>

namespace kate {

/**
 * \brief Class to manage configuration data of the plug-in
 *
 * [More detailed description here]
 *
 */
class PluginConfiguration
  : public QObject
{
    Q_OBJECT

public:
    typedef std::vector<std::pair<QRegExp, QString>> sanitize_rules_list_type;

    PluginConfiguration()
      : m_monitor_flags(0)
      , m_use_ltgt(true)
      , m_config_dirty(false)
      , m_open_first(false)
      , m_use_wildcard_search(false)
      , m_highlight_completions(true)
      , m_sanitize_completions(true)
      , m_auto_completions(true)
      , m_include_macros(true)
      , m_use_prefix_column(false)
    {}

    /// \name Accessors
    //@{
    const QStringList& sessionDirs() const;
    const QStringList& systemDirs() const;
    const QStringList& ignoreExtensions() const;
    const QStringList& enabledIndices() const;
    const KUrl& precompiledHeaderFile() const;
    const KUrl& pchFile() const;
    const QString& clangParams() const;
    bool useLtGt() const;
    bool useCwd() const;
    bool shouldOpenFirstInclude() const;
    bool useWildcardSearch() const;
    int what_to_monitor() const;
    bool sanitizeCompletions() const;
    bool highlightCompletions() const;
    bool autoCompletions() const;
    const sanitize_rules_list_type& sanitizeRules() const;
    bool includeMacros() const;
    bool usePrefixColumn() const;
    unsigned completionFlags() const;
    //@}

    /// \name Modifiers
    //@{
    void setSessionDirs(QStringList&);
    void setSystemDirs(QStringList&);
    void setIgnoreExtensions(QStringList&);
    void setClangParams(const QString&);
    void setPrecompiledHeaderFile(const KUrl&);
    void setPrecompiledFile(const KUrl&);
    void setUseLtGt(bool);
    void setUseCwd(bool);
    void setOpenFirst(bool);
    void setUseWildcardSearch(bool);
    void setWhatToMonitor(int);
    void setSanitizeCompletions(bool);
    void setHighlightCompletions(bool);
    void setAutoCompletions(bool);
    void setSanitizeRules(sanitize_rules_list_type&&);
    void setIncludeMacros(bool);
    void setUsePrefixColumn(bool);
    //@}

    void readSessionConfig(KConfigBase*, const QString&);
    void writeSessionConfig(KConfigBase*, const QString&);
    void readConfig();                                      ///< Read global config

    QStringList formCompilerOptions() const;

Q_SIGNALS:
    void dirWatchSettingsChanged();
    void sessionDirsChanged();
    void systemDirsChanged();
    void precompiledHeaderFileChanged();
    void clangOptionsChanged();

private:
    sanitize_rules_list_type m_sanitize_rules;
    QStringList m_system_dirs;
    QStringList m_session_dirs;
    QStringList m_ignore_ext;
    QStringList m_enabled_indices;
    KUrl m_pch_header;
    KUrl m_pch_file;
    QString m_clang_params;
    int m_monitor_flags;
    /// If \c true <em>Copy #include</em> action would put filename into \c '<' and \c '>'
    /// instead of \c '"'
    bool m_use_ltgt;
    bool m_use_cwd;
    bool m_config_dirty;
    bool m_open_first;
    bool m_use_wildcard_search;
    bool m_highlight_completions;
    bool m_sanitize_completions;
    bool m_auto_completions;
    bool m_include_macros;
    bool m_use_prefix_column;
};

inline const QStringList& PluginConfiguration::sessionDirs() const
{
    return m_session_dirs;
}
inline const QStringList& PluginConfiguration::systemDirs() const
{
    return m_system_dirs;
}
inline const QStringList& PluginConfiguration::ignoreExtensions() const
{
    return m_ignore_ext;
}
inline const QStringList& PluginConfiguration::enabledIndices() const
{
    return m_enabled_indices;
}
inline const KUrl& PluginConfiguration::precompiledHeaderFile() const
{
    return m_pch_header;
}
inline const KUrl& PluginConfiguration::pchFile() const
{
    return m_pch_file;
}
inline const QString& PluginConfiguration::clangParams() const
{
    return m_clang_params;
}
inline bool PluginConfiguration::useLtGt() const
{
    return m_use_ltgt;
}
inline bool PluginConfiguration::useCwd() const
{
    return m_use_cwd;
}
inline bool PluginConfiguration::shouldOpenFirstInclude() const
{
    return m_open_first;
}
inline bool PluginConfiguration::useWildcardSearch() const
{
    return m_use_wildcard_search;
}
inline int PluginConfiguration::what_to_monitor() const
{
    return m_monitor_flags;
}
inline bool PluginConfiguration::sanitizeCompletions() const
{
    return m_sanitize_completions;
}
inline bool PluginConfiguration::highlightCompletions() const
{
    return m_highlight_completions;
}
inline bool PluginConfiguration::autoCompletions() const
{
    return m_auto_completions;
}
inline auto PluginConfiguration::sanitizeRules() const -> const sanitize_rules_list_type&
{
    return m_sanitize_rules;
}

inline void PluginConfiguration::setPrecompiledFile(const KUrl& file)
{
    m_pch_file = file;
}

inline bool PluginConfiguration::includeMacros() const
{
    return m_include_macros;
}

inline bool PluginConfiguration::usePrefixColumn() const
{
    return m_use_prefix_column;
}

}                                                           // namespace kate
// kate: hl C++11/Qt4;
