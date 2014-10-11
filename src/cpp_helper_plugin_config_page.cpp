/**
 * \file
 *
 * \brief Class \c kate::CppHelperPluginConfigPage (implementation)
 *
 * \date Mon Feb  6 06:04:17 MSK 2012 -- Initial design
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
#include "cpp_helper_plugin_config_page.h"
#include "cpp_helper_plugin.h"
#include "ui_clang_completion_settings.h"
#include "ui_clang_settings.h"
#include "ui_detect_compiler_paths.h"
#include "ui_other_settings.h"
#include "ui_path_config.h"
#include "ui_session_paths_sets.h"
#include "utils.h"

// Standard includes
#include <kate/application.h>
#include <kate/documentmanager.h>
#include <KDE/KDebug>
#include <KDE/KDirSelectDialog>
#include <KDE/KFileDialog>
#include <KDE/KPassivePopup>
#include <KDE/KShellCompletion>
#include <KDE/KStandardDirs>
#include <KDE/KTabWidget>
#include <QtGui/QVBoxLayout>
#include <cstdlib>
#include <vector>

namespace kate { namespace {
const char* const INCSET_GROUP_NAME = "SessionIncludeSet";
const char* const INCSET_NAME_KEY = "Name";
const char* const INCSET_DIRS_KEY = "Dirs";
/// \attention Make sure this path replaced everywhre in case of changes
/// \todo Make a constant w/ single declaration place for this path
const char* const INCSET_FILE_TPL = "plugins/katecpphelperplugin/%1.incset";
const char* const SANITIZER_RULES_GROUP_NAME = "Kate C++ Helper plugin: Completion Sanitizer Rules";
const QString DEFAULT_GCC_BINARY = "g++";
const QString DEFAULT_CLANG_BINARY = "clang++";
/// \todo Add more VCS dirs to recognize
std::vector<const char*> VSC_DIRS = {
    ".git", ".hg", ".svn"
};
std::vector<const char*> WELL_KNOWN_NOT_SUITABLE_DIRS = {
    "/bin"
  , "/boot"
  , "/etc"
  , "/home"
  , "/sbin"
  , "/usr"
  , "/usr/bin"
  , "/usr/sbin"
  , "/usr/local"
  , "/var"
};

bool hasVCSDir(const QString& url)
{
    for (const auto vcs_dir : VSC_DIRS)
    {
        const auto di = QFileInfo{QDir{QDir::cleanPath(url)}.filePath(vcs_dir)};
        if (di.isDir() && di.exists())
            return true;
    }
    return false;
}

bool isFirstLevelPath(const QString& dir)
{
    const auto url = KUrl(dir).directory();
    return KUrl(url).path() == QDir::rootPath();
}

bool isOneOfWellKnownPaths(const QString& url)
{
    for (const auto dir : WELL_KNOWN_NOT_SUITABLE_DIRS)
        if (url == QLatin1String{dir})
            return true;
    return false;
}

}                                                           // anonymous namespace

//BEGIN CppHelperPluginConfigPage
/**
 * The main task of the constructor is to setup GUI.
 * To populate GUI w/ current configuration \c reset() called after that.
 */
CppHelperPluginConfigPage::CppHelperPluginConfigPage(
    QWidget* parent
  , CppHelperPlugin* plugin
  )
  : Kate::PluginConfigPage{parent}
  , m_plugin{plugin}
  , m_pss_config{new Ui::PerSessionSettingsConfigWidget()}
  , m_clang_config{new Ui::CLangOptionsWidget()}
  , m_system_list{new Ui::PathListConfigWidget()}
  , m_session_list{new Ui::PathListConfigWidget()}
  , m_compiler_paths{new Ui::DetectCompilerPathsWidget()}
  , m_favorite_sets{new Ui::SessionPathsSetsWidget()}
  , m_completion_settings{new Ui::CompletionSettings()}
  , m_compiler_proc{this}
{
    auto* layout = new QVBoxLayout{this};
    auto* tab = new KTabWidget{this};
    layout->addWidget(tab);
    layout->setMargin(0);

    // Global #include paths
    {
        auto* system_tab = new QWidget{tab};
        // Setup paths list widget
        auto* paths = new QWidget{system_tab};
        paths->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_system_list->setupUi(paths);
        // Connect add/del buttons to actions
        connect(m_system_list->addButton, SIGNAL(clicked()), this, SLOT(addGlobalIncludeDir()));
        connect(m_system_list->delButton, SIGNAL(clicked()), this, SLOT(delGlobalIncludeDir()));
        connect(m_system_list->moveUpButton, SIGNAL(clicked()), this, SLOT(moveGlobalDirUp()));
        connect(m_system_list->moveDownButton, SIGNAL(clicked()), this, SLOT(moveGlobalDirDown()));
        connect(m_system_list->clearButton, SIGNAL(clicked()), this, SLOT(clearGlobalDirs()));
        // Setup predefined compiler's paths widget
        auto* compilers = new QWidget{system_tab};
        compilers->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_compiler_paths->setupUi(compilers);
        {
            auto gcc_binary = findBinary(DEFAULT_GCC_BINARY);
            if (gcc_binary.isEmpty())
                m_compiler_paths->gcc->setEnabled(false);
            else
                m_compiler_paths->gcc->setText(gcc_binary);
        }
        {
            auto clang_binary = findBinary(DEFAULT_CLANG_BINARY);
            if (clang_binary.isEmpty())
                m_compiler_paths->clang->setEnabled(false);
            else
                m_compiler_paths->clang->setText(clang_binary);
        }
        // Connect add button to action
        connect(m_compiler_paths->addButton, SIGNAL(clicked()), this, SLOT(detectPredefinedCompilerPaths()));
        // Setup layout
        auto* layout = new QVBoxLayout{system_tab};
        layout->addWidget(paths, 1);
        layout->addWidget(compilers, 0);
        system_tab->setLayout(layout);
        tab->addTab(system_tab, i18nc("@title:tab", "System Paths List"));
    }

    // Session #include paths
    {
        auto* session_tab = new QWidget{tab};
        // Setup paths list widget
        auto* paths = new QWidget{session_tab};
        paths->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_session_list->setupUi(paths);
        // Connect add/del buttons to actions
        connect(m_session_list->addButton, SIGNAL(clicked()), this, SLOT(addSessionIncludeDir()));
        connect(m_session_list->delButton, SIGNAL(clicked()), this, SLOT(delSessionIncludeDir()));
        connect(m_session_list->moveUpButton, SIGNAL(clicked()), this, SLOT(moveSessionDirUp()));
        connect(m_session_list->moveDownButton, SIGNAL(clicked()), this, SLOT(moveSessionDirDown()));
        connect(m_session_list->clearButton, SIGNAL(clicked()), this, SLOT(clearSessionDirs()));
        // Setup predefined compiler's paths widget
        auto* favorites = new QWidget{session_tab};
        favorites->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_favorite_sets->setupUi(favorites);
        connect(m_favorite_sets->addButton, SIGNAL(clicked()), this, SLOT(addSet()));
        connect(m_favorite_sets->removeButton, SIGNAL(clicked()), this, SLOT(removeSet()));
        connect(m_favorite_sets->storeButton, SIGNAL(clicked()), this, SLOT(storeSet()));
        connect(m_favorite_sets->addSuggestedDirButton, SIGNAL(clicked()), this, SLOT(addSuggestedDir()));
        connect(m_favorite_sets->vcsOnly, SIGNAL(clicked()), this, SLOT(updateSuggestions()));
        // Setup layout
        auto* layout = new QVBoxLayout{session_tab};
        layout->addWidget(paths, 1);
        layout->addWidget(favorites, 0);
        session_tab->setLayout(layout);
        tab->addTab(session_tab, i18nc("@title:tab", "Session Paths List"));
    }

    // Clang settings
    {
        auto* clang_tab = new QWidget{tab};
        m_clang_config->setupUi(clang_tab);
        tab->addTab(clang_tab, i18nc("@title:tab", "Clang Settings"));
        // Monitor changes to PCH file
        connect(
            m_clang_config->pchHeader
          , SIGNAL(textChanged(const QString&))
          , this
          , SLOT(pchHeaderChanged(const QString&))
          );
        connect(
            m_clang_config->pchHeader
            // ATTENTION Documentation is wrong about signal parameter type!
            // http://api.kde.org/4.x-api/kdelibs-apidocs/kio/html/classKUrlRequester.html
          , SIGNAL(urlSelected(const KUrl&))
          , this
          , SLOT(pchHeaderChanged(const KUrl&))
          );
        // Connect open and rebuild PCH file button
        /// \todo Monitor PCH header file for changes:
        /// it could be absent at this moment, but may appear later,
        /// while configuration dialog still opened...
        connect(m_clang_config->openPchHeader, SIGNAL(clicked()), this, SLOT(openPCHHeaderFile()));
        connect(m_clang_config->rebuildPch, SIGNAL(clicked()), this, SLOT(rebuildPCH()));
    }

    // Completion settings
    {
        auto* comp_tab = new QWidget{tab};
        m_completion_settings->setupUi(comp_tab);
        tab->addTab(comp_tab, i18nc("@title:tab", "Code Completion Settings"));
        connect(m_completion_settings->addRule, SIGNAL(clicked()), this, SLOT(addEmptySanitizeRule()));
        connect(m_completion_settings->removeRule, SIGNAL(clicked()), this, SLOT(removeSanitizeRule()));
        connect(m_completion_settings->upRule, SIGNAL(clicked()), this, SLOT(moveSanitizeRuleUp()));
        connect(m_completion_settings->downRule, SIGNAL(clicked()), this, SLOT(moveSanitizeRuleDown()));
        connect(m_completion_settings->exportRules, SIGNAL(clicked()), this, SLOT(exportSanitizeRules()));
        connect(m_completion_settings->importRules, SIGNAL(clicked()), this, SLOT(importSanitizeRules()));
        connect(
            m_completion_settings->sanitizeRules
          , SIGNAL(cellChanged(int, int))
          , this
          , SLOT(validateSanitizeRule(int, int))
          );
    }

    // Other settings
    {
        auto* pss_tab = new QWidget{tab};
        m_pss_config->setupUi(pss_tab);
        tab->addTab(pss_tab, i18nc("@title:tab", "Other Settings"));
        // Disable completion on 'ignore extensions' like edit
        m_pss_config->ignoreExtensions->setCompletionMode(KGlobalSettings::CompletionNone);
    }

    // Subscribe self to compiler process signals
    connect(
        &m_compiler_proc
      , SIGNAL(error(QProcess::ProcessError))
      , this
      , SLOT(error(QProcess::ProcessError))
      );
    connect(
        &m_compiler_proc
      , SIGNAL(finished(int, QProcess::ExitStatus))
      , this
      , SLOT(finished(int, QProcess::ExitStatus))
      );
    connect(&m_compiler_proc, SIGNAL(readyReadStandardError()), this, SLOT(readyReadStandardError()));
    connect(&m_compiler_proc, SIGNAL(readyReadStandardOutput()), this, SLOT(readyReadStandardOutput()));

    // Populate configuration w/ data
    reset();
}

void CppHelperPluginConfigPage::apply()
{
    kDebug(DEBUG_AREA) << "** CONFIG-PAGE **: Applying configuration";
    // Get settings from 'System #include dirs' tab
    {
        QStringList dirs;
        for (auto i = 0; i < m_session_list->pathsList->count(); ++i)
            dirs.append(m_session_list->pathsList->item(i)->text());
        m_plugin->config().setSessionDirs(dirs);
    }

    // Get settings from 'Session #include dirs' tab
    {
        QStringList dirs;
        for (auto i = 0; i < m_system_list->pathsList->count(); ++i)
            dirs.append(m_system_list->pathsList->item(i)->text());
        m_plugin->config().setSystemDirs(dirs);
    }

    // Get settings from 'Clang (compiler) Settings' tab
    m_plugin->config().setPrecompiledHeaderFile(m_clang_config->pchHeader->text());
    m_plugin->config().setClangParams(m_clang_config->commandLineParams->toPlainText());

    // Get settings from 'Per Session Settings' tab
    m_plugin->config().setUseLtGt(m_pss_config->includeMarkersSwitch->isChecked());
    m_plugin->config().setUseCwd(m_pss_config->useCurrentDirSwitch->isChecked());
    m_plugin->config().setOpenFirst(m_pss_config->openFirstHeader->isChecked());
    m_plugin->config().setUseWildcardSearch(m_pss_config->useWildcardSearch->isChecked());
    auto want_monitor = PluginConfiguration::MonitorTargets::nothing;
    if (m_pss_config->session->isChecked())
        want_monitor = PluginConfiguration::MonitorTargets::sessionDirs;
    else if (m_pss_config->system->isChecked())
        want_monitor = PluginConfiguration::MonitorTargets::systemDirs;
    else if (m_pss_config->all->isChecked())
        want_monitor = PluginConfiguration::MonitorTargets::both;
    m_plugin->config().setMonitorTargets(want_monitor);
    {
        auto extensions = m_pss_config
          ->ignoreExtensions
          ->text()
          .split(QRegExp{"[, :;]+"}, QString::SkipEmptyParts);
        kDebug(DEBUG_AREA) << "Extensions to ignore:" << extensions;
        m_plugin->config().setIgnoreExtensions(extensions);
    }

    // Get settings from 'Clang Completion Settings' tab
    m_plugin->config().setAutoCompletions(m_completion_settings->autoCompletions->isChecked());
    m_plugin->config().setIncludeMacros(m_completion_settings->includeMacros->isChecked());
    m_plugin->config().setUsePrefixColumn(m_completion_settings->usePrefixColumn->isChecked());
    m_plugin->config().setHighlightCompletions(m_completion_settings->highlightResults->isChecked());
    m_plugin->config().setSanitizeCompletions(m_completion_settings->sanitizeResults->isChecked());
    m_plugin->config().setAppendOnImport(m_completion_settings->appendOnImport->isChecked());
    pushSanitizeRules();
}

/**
 * This method should do a reset configuration to current state --
 * i.e. reread configuration data from the plugin's storage.
 */
void CppHelperPluginConfigPage::reset()
{
    kDebug(DEBUG_AREA) << "** CONFIG-PAGE **: Reseting configuration";

    // Put dirs to the list
    m_system_list->pathsList->addItems(m_plugin->config().systemDirs());
    m_session_list->pathsList->addItems(m_plugin->config().sessionDirs());

    m_clang_config->pchHeader->setUrl(m_plugin->config().precompiledHeaderFile());
    m_clang_config->commandLineParams->setPlainText(m_plugin->config().clangParams());

    m_pss_config->includeMarkersSwitch->setChecked(m_plugin->config().useLtGt());
    m_pss_config->useCurrentDirSwitch->setChecked(m_plugin->config().useCwd());
    m_pss_config->ignoreExtensions->setText(
        m_plugin->config().ignoreExtensions().join(", ")
      );
    m_pss_config->openFirstHeader->setChecked(m_plugin->config().shouldOpenFirstInclude());
    m_pss_config->useWildcardSearch->setChecked(m_plugin->config().useWildcardSearch());

    m_completion_settings->highlightResults->setChecked(m_plugin->config().highlightCompletions());
    m_completion_settings->sanitizeResults->setChecked(m_plugin->config().sanitizeCompletions());
    m_completion_settings->appendOnImport->setChecked(m_plugin->config().appendOnImport());
    m_completion_settings->autoCompletions->setChecked(m_plugin->config().autoCompletions());
    m_completion_settings->includeMacros->setChecked(m_plugin->config().includeMacros());
    m_completion_settings->usePrefixColumn->setChecked(m_plugin->config().usePrefixColumn());

    pullSanitizeRules();

    // Setup dirs watcher
    auto flags = m_plugin->config().monitorTargets();
    m_pss_config->nothing->setChecked(flags == PluginConfiguration::MonitorTargets::nothing);
    m_pss_config->session->setChecked(flags == PluginConfiguration::MonitorTargets::sessionDirs);
    m_pss_config->system->setChecked(flags == PluginConfiguration::MonitorTargets::systemDirs);
    m_pss_config->all->setChecked(flags == PluginConfiguration::MonitorTargets::both);

    pchHeaderChanged(m_plugin->config().precompiledHeaderFile());
    updateSuggestions();
    updateSets();
}

void CppHelperPluginConfigPage::defaults()
{
    kDebug(DEBUG_AREA) << "** CONFIG-PAGE **: Default configuration requested";
    /// \todo Fill configuration elements w/ default values
}

void CppHelperPluginConfigPage::addSessionIncludeDir()
{
    addDirTo(
        KDirSelectDialog::selectDirectory(KUrl{}, true, this)
      , m_session_list->pathsList
      );
    Q_EMIT(changed());
}

void CppHelperPluginConfigPage::delSessionIncludeDir()
{
    /// \todo \c delete ? really? any other (better) way to remove item?
    delete m_session_list->pathsList->currentItem();
    Q_EMIT(changed());
}

void CppHelperPluginConfigPage::moveSessionDirUp()
{
    const auto current = m_session_list->pathsList->currentRow();
    if (current)
    {
        m_session_list->pathsList->insertItem(
            current - 1
          , m_session_list->pathsList->takeItem(current)
          );
        m_session_list->pathsList->setCurrentRow(current - 1);
        Q_EMIT(changed());
    }
}

void CppHelperPluginConfigPage::moveSessionDirDown()
{
    const auto current = m_session_list->pathsList->currentRow();
    if (current < m_session_list->pathsList->count() - 1)
    {
        m_session_list->pathsList->insertItem(
            current + 1
          , m_session_list->pathsList->takeItem(current)
          );
        m_session_list->pathsList->setCurrentRow(current + 1);
        Q_EMIT(changed());
    }
}

void CppHelperPluginConfigPage::clearSessionDirs()
{
    m_session_list->pathsList->clear();
    Q_EMIT(changed());
}

/**
 * Append configured paths from selected \c #include \c set
 */
void CppHelperPluginConfigPage::addSet()
{
    auto it = m_include_sets.find(m_favorite_sets->setsList->currentText());
    if (it != end(m_include_sets))
    {
        auto general = KConfigGroup{it->second.m_config, INCSET_GROUP_NAME};
        auto dirs = general.readPathEntry(INCSET_DIRS_KEY, QStringList());
        if (!dirs.isEmpty())
        {
            for (const auto& dir : dirs)
                addDirTo(dir, m_session_list->pathsList);
            Q_EMIT(changed());
        }
    }
}

void CppHelperPluginConfigPage::removeSet()
{
    auto it = m_include_sets.find(m_favorite_sets->setsList->currentText());
    if (it != end(m_include_sets))
    {
        QFile file{it->second.m_file};
        kDebug(DEBUG_AREA) << "Going to remove file" << file.fileName();
        if (!file.remove())
        {
            KPassivePopup::message(
                i18nc("@title:window", "Error")
              , i18nc("@info:tooltip", "<qt>Unable to remove file:<br /><icode>%1</icode></qt>", file.fileName())
              , qobject_cast<QWidget*>(this)
              );
            return;
        }
        KPassivePopup::message(
            i18nc("@title:window", "Done")
          , i18nc("@info:tooltip", "<qt>Remove successed<br /><icode>%1</icode></qt>", file.fileName())
          , qobject_cast<QWidget*>(this)
          );
        updateSets();
    }
}

void CppHelperPluginConfigPage::storeSet()
{
    auto set_name = m_favorite_sets->setsList->currentText();
    kDebug(DEBUG_AREA) << "Current set name:" << set_name;

    auto cfg = KSharedConfigPtr{};
    {
        auto it = m_include_sets.find(set_name);
        if (it == end(m_include_sets))
        {
            auto filename = QString{QUrl::toPercentEncoding(set_name)};
            auto incset_file = KStandardDirs::locateLocal(
                "appdata"
              , QString{INCSET_FILE_TPL}.arg(filename)
              , true
              );
            kDebug(DEBUG_AREA) << "Going to make a new incset file for it:" << incset_file;
            cfg = KSharedConfig::openConfig(incset_file, KConfig::SimpleConfig);
        }
        else cfg = it->second.m_config;
    }

    auto dirs = QStringList{};
    for (auto i = 0, last = m_session_list->pathsList->count(); i < last; ++i)
        dirs << m_session_list->pathsList->item(i)->text();
    kDebug(DEBUG_AREA) << "Collected current paths:" << dirs;

    // Write Name and Dirs entries to the config
    KConfigGroup general(cfg, INCSET_GROUP_NAME);
    general.writeEntry(INCSET_NAME_KEY, set_name);
    general.writePathEntry(INCSET_DIRS_KEY, dirs);
    /// \todo I wonder is it always successed? ORLY?!
    cfg->sync();
    updateSets(set_name);
}

void CppHelperPluginConfigPage::addSuggestedDir()
{
    addDirTo(m_favorite_sets->suggestionsList->currentText(), m_session_list->pathsList);
    Q_EMIT(changed());
}

void CppHelperPluginConfigPage::addGlobalIncludeDir()
{
    addDirTo(
        KDirSelectDialog::selectDirectory(KUrl(), true, this)
      , m_system_list->pathsList
      );
    Q_EMIT(changed());
}

void CppHelperPluginConfigPage::delGlobalIncludeDir()
{
    delete m_system_list->pathsList->currentItem();
    Q_EMIT(changed());
}

void CppHelperPluginConfigPage::moveGlobalDirUp()
{
    const auto current = m_system_list->pathsList->currentRow();
    if (current)
    {
        m_system_list->pathsList->insertItem(
            current - 1
          , m_system_list->pathsList->takeItem(current)
          );
        m_system_list->pathsList->setCurrentRow(current - 1);
        Q_EMIT(changed());
    }
}

void CppHelperPluginConfigPage::moveGlobalDirDown()
{
    const auto current = m_system_list->pathsList->currentRow();
    if (current < m_system_list->pathsList->count() - 1)
    {
        m_system_list->pathsList->insertItem(
            current + 1
          , m_system_list->pathsList->takeItem(current)
          );
        m_system_list->pathsList->setCurrentRow(current + 1);
        Q_EMIT(changed());
    }
}

void CppHelperPluginConfigPage::clearGlobalDirs()
{
    m_system_list->pathsList->clear();
    Q_EMIT(changed());
}

/**
 * \note Silly Qt can't deal w/ signals and slots w/ different signatures,
 * and even nasty cheat like \c QSignalMapper can't help here to open a document
 * by pressing a button and get a current value from an other line edit control...
 * So it is why I have this one-liner... DAMN Qt!
 */
void CppHelperPluginConfigPage::openPCHHeaderFile()
{
    const auto& pch_url = m_clang_config->pchHeader->url();
    const auto& pch_file = pch_url.toLocalFile();
    if (!pch_file.isEmpty() && isPresentAndReadable(pch_file))
        m_plugin->openDocument(pch_url);
    else
        KPassivePopup::message(
            i18nc("@title:window", "Error")
          , i18nc("@info:tooltip", "<qt>PCH header file is not configured or readable.</qt>")
          , qobject_cast<QWidget*>(this)
          );
}

void CppHelperPluginConfigPage::rebuildPCH()
{
    const auto& pch_url = m_clang_config->pchHeader->url();
    const auto& pch_file = pch_url.toLocalFile();
    if (!pch_file.isEmpty() && isPresentAndReadable(pch_file))
        m_plugin->makePCHFile(pch_url);
    else
        KPassivePopup::message(
            i18nc("@title:window", "Error")
          , i18nc("@info:tooltip", "<qt>PCH header file is not configured or readable.</qt>")
          , qobject_cast<QWidget*>(this)
          );
}

void CppHelperPluginConfigPage::pchHeaderChanged(const QString& filename)
{
    const auto is_valid_pch_file = isPresentAndReadable(filename);
    kDebug(DEBUG_AREA) << "Check if PCH header file present and readable: "
      << filename << ", result=" << is_valid_pch_file;
    m_clang_config->openPchHeader->setEnabled(is_valid_pch_file);
    m_clang_config->rebuildPch->setEnabled(is_valid_pch_file);
    Q_EMIT(changed());
}

void CppHelperPluginConfigPage::pchHeaderChanged(const KUrl& filename)
{
    pchHeaderChanged(filename.toLocalFile());
}

/**
 * \todo Need to raise a timer to detect a hunged process and kill it after timeout.
 */
void CppHelperPluginConfigPage::detectPredefinedCompilerPaths()
{
    const auto binary = getCurrentCompiler();
    kDebug(DEBUG_AREA) << "Determine predefined compiler paths for" << binary;

    m_output.clear();
    m_error.clear();
    m_compiler_proc.clearProgram();
    m_compiler_proc << binary << "-v" << "-E" << "-x" << "c++" << "/dev/null";
    m_compiler_proc.setOutputChannelMode(KProcess::SeparateChannels);
    m_compiler_proc.start();

    QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));
    m_compiler_paths->addButton->setDisabled(true);
}

void CppHelperPluginConfigPage::error(QProcess::ProcessError error)
{
    const auto binary = getCurrentCompiler();
    auto status_str = QString{};
    switch (error)
    {
        case QProcess::FailedToStart: status_str = i18nc("@info:tooltip", "Process failed to start"); break;
        case QProcess::Crashed:       status_str = i18nc("@info:tooltip", "Process crashed"); break;
        case QProcess::Timedout:      status_str = i18nc("@info:tooltip", "Timedout");        break;
        case QProcess::WriteError:    status_str = i18nc("@info:tooltip", "Write error");     break;
        case QProcess::ReadError:     status_str = i18nc("@info:tooltip", "Read error");      break;
        case QProcess::UnknownError:
        default:
            status_str = i18nc("@info:tooltip", "Unknown error");
            break;
    }
    KPassivePopup::message(
        i18nc("@title:window", "Error")
      , i18nc(
            "@info:tooltip"
          , "<qt>Failed to execute <command>%1</command>:<nl /><message>%2</message><qt>"
          , binary
          , status_str
          )
      , qobject_cast<QWidget*>(this)
      );
    QApplication::setOverrideCursor(QCursor(Qt::ArrowCursor));
    m_compiler_paths->addButton->setDisabled(false);
}

void CppHelperPluginConfigPage::finished(int exit_code, QProcess::ExitStatus exit_status)
{
    kDebug(DEBUG_AREA) << "Compiler STDOUT: " << m_output;
    kDebug(DEBUG_AREA) << "Compiler STDERR: " << m_error;
    QApplication::setOverrideCursor(QCursor(Qt::ArrowCursor));
    m_compiler_paths->addButton->setDisabled(false);

    // Do nothign on failure
    if (exit_status != QProcess::NormalExit && exit_code != EXIT_SUCCESS)
    {
        KPassivePopup::message(
        i18nc("@title:window", "Error")
          , i18nc(
                "@info:tooltip"
              , "<qt>Unable to get predefined <icode>#include</icode> paths. Process exited with code %1</qt>"
              , exit_code
              )
          , qobject_cast<QWidget*>(this)
          );
        return;
    }
    // Split output by lines
    auto lines = m_error.split('\n');
    auto collect_paths = false;
    for (const auto& line : lines)
    {
        if (line == QLatin1String{"#include <...> search starts here:"})
        {
            collect_paths = true;
            continue;
        }
        if (line == QLatin1String{"End of search list."})
        {
            collect_paths = false;
            continue;
        }
        if (collect_paths)
        {
            addDirTo(line.trimmed(), m_system_list->pathsList);
        }
    }
}

void CppHelperPluginConfigPage::readyReadStandardOutput()
{
    m_compiler_proc.setReadChannel(QProcess::StandardOutput);
    m_output.append(m_compiler_proc.readAll());
}

void CppHelperPluginConfigPage::readyReadStandardError()
{
    m_compiler_proc.setReadChannel(QProcess::StandardError);
    m_error.append(m_compiler_proc.readAll());
}

bool CppHelperPluginConfigPage::contains(const QString& dir, const KListWidget* const list)
{
    for (auto i = 0; i < list->count(); ++i)
        if (list->item(i)->text() == dir)
            return true;
    return false;
}

void CppHelperPluginConfigPage::addDirTo(const KUrl& dir_uri, KListWidget* const list)
{
    if (dir_uri.isValid() && !dir_uri.isEmpty())
    {
        // get URI as local file/path
        auto dir_str = QDir::cleanPath(dir_uri.toLocalFile());
        if (!contains(dir_str, list))                       // append only if given path not in a list already
            new QListWidgetItem{dir_str, list};
    }
}

QString CppHelperPluginConfigPage::findBinary(const QString& binary) const
{
    assert("binary name expected to be non-empty" && !binary.isEmpty());

    auto result = QString{};
    if (const auto* const path_env = std::getenv("PATH"))
    {
        /// \todo Is there any portable way to get paths separator?
        const auto paths = QString{path_env}.split(':', QString::SkipEmptyParts);
        for (const auto& path : paths)
        {
            const auto full_path = QDir{path}.filePath(binary);
            const auto fi = QFileInfo{full_path};
            if (fi.exists() && fi.isExecutable())
            {
                result = full_path;
                break;
            }
        }
    }
    return result;
}

QString CppHelperPluginConfigPage::getCurrentCompiler() const
{
    auto binary = QString{};
    if (m_compiler_paths->gcc->isChecked())
        binary = findBinary(DEFAULT_GCC_BINARY);
    else if (m_compiler_paths->clang->isChecked())
        binary = findBinary(DEFAULT_CLANG_BINARY);

    return binary;
}

/**
 * Find all \c *.inset files in \c ${APPDATA}/plugins/katecpphelperplugin.
 * Open found file (as ordinal KDE config), read a set \c Name and fill the
 * \c m_include_sets map w/ \e Name to \c KSharedConfigPtr entry.
 * After all found files forcessed, fill the combobox w/ found entries.
 * 
 * \param current if not empty string, preselect this item
 */
void CppHelperPluginConfigPage::updateSets(const QString& current)
{
    // Remove everything collected before
    m_favorite_sets->setsList->clear();
    m_include_sets.clear();

    // Find *.incset files
    auto sets = KGlobal::dirs()->findAllResources(
        "appdata"
      , QString{INCSET_FILE_TPL}.arg("*")
      , KStandardDirs::NoSearchOptions
      );
    kDebug(DEBUG_AREA) << "sets:" << sets;

    // Form a map of set names to shared configs
    for (const auto& filename : sets)
    {
        KSharedConfigPtr incset = KSharedConfig::openConfig(filename, KConfig::SimpleConfig);
        KConfigGroup general(incset, INCSET_GROUP_NAME);
        auto set_name = general.readEntry(INCSET_NAME_KEY, QString{});
        auto dirs = general.readPathEntry(INCSET_DIRS_KEY, QStringList{});
        kDebug(DEBUG_AREA) << "set name: " << set_name;
        kDebug(DEBUG_AREA) << "dirs: " << dirs;
        m_include_sets[set_name] = IncludeSetInfo{incset, filename};
    }

    // Fill the `sets` combobox w/ names
    kDebug() << "current set name" << current;
    for (const auto& p : m_include_sets)
    {
        kDebug() << "adding set name" << p.first;
        m_favorite_sets->setsList->addItem(p.first);
    }
    // Try to preset a set name
    if (!current.isEmpty())
        m_favorite_sets->setsList->setCurrentItem(current, false);
}

void CppHelperPluginConfigPage::updateSuggestions()
{
    // Obtain a list of currecntly opened documents
    auto documents = m_plugin->application()->documentManager()->documents();
    // Collect paths
    auto dirs = QStringList{};
    const auto should_check_vcs = m_favorite_sets->vcsOnly->isChecked();
    for (auto* current_doc : documents)
    {
        // Get current document's URI
        const auto current_doc_uri = current_doc->url();
        // Check if valid
        if (current_doc_uri.isValid() && !current_doc_uri.isEmpty())
        {
            // Traverse over all parent dirs
            for (
                KUrl url = current_doc_uri.directory()
              ; url.hasPath() && url.path() != QDir::rootPath()
              ; url = url.upUrl()
              )
            {
                auto dir = QDir::cleanPath(url.path());     // Obtain path as string
                // Check uniqueness and other constraints
                const bool should_add = dirs.indexOf(dir) == -1
                  && !contains(dir, m_system_list->pathsList)
                  && !contains(dir, m_session_list->pathsList)
                  && (
                      (should_check_vcs && hasVCSDir(dir))
                    || (!should_check_vcs && !isOneOfWellKnownPaths(dir) && !isFirstLevelPath(dir))
                    )
                  ;
                if (should_add)                             // Add current path only if not added yet
                    dirs << dir;
            }
        }
    }
    qSort(dirs);
    kDebug(DEBUG_AREA) << "Suggestions list:" << dirs;
    // Update combobox w/ collected list
    m_favorite_sets->suggestionsList->clear();
    m_favorite_sets->suggestionsList->addItems(dirs);

    // Enable/disable controls according documents list emptyness
    const auto is_enabled = !dirs.isEmpty();
    m_favorite_sets->addSuggestedDirButton->setEnabled(is_enabled);
    m_favorite_sets->suggestionsList->setEnabled(is_enabled);
}

void CppHelperPluginConfigPage::addEmptySanitizeRule()
{
    kDebug(DEBUG_AREA) << "rules rows =" << m_completion_settings->sanitizeRules->rowCount();
    kDebug(DEBUG_AREA) << "rules cols =" << m_completion_settings->sanitizeRules->columnCount();

    const auto row = m_completion_settings->sanitizeRules->rowCount();
    m_completion_settings->sanitizeRules->insertRow(row);
    m_completion_settings->sanitizeRules->setItem(row, 0, new QTableWidgetItem{});
    m_completion_settings->sanitizeRules->setItem(row, 1, new QTableWidgetItem{});
}

void CppHelperPluginConfigPage::removeSanitizeRule()
{
    m_completion_settings->sanitizeRules->removeRow(
        m_completion_settings->sanitizeRules->currentRow()
      );
    Q_EMIT(changed());
}

void CppHelperPluginConfigPage::swapRuleRows(const int src, const int dst)
{
    auto* src_col_0 = m_completion_settings->sanitizeRules->takeItem(src, 0);
    auto* src_col_1 = m_completion_settings->sanitizeRules->takeItem(src, 1);
    auto* dst_col_0 = m_completion_settings->sanitizeRules->takeItem(dst, 0);
    auto* dst_col_1 = m_completion_settings->sanitizeRules->takeItem(dst, 1);
    m_completion_settings->sanitizeRules->setItem(src, 0, dst_col_0);
    m_completion_settings->sanitizeRules->setItem(src, 1, dst_col_1);
    m_completion_settings->sanitizeRules->setItem(dst, 0, src_col_0);
    m_completion_settings->sanitizeRules->setItem(dst, 1, src_col_1);
}

void CppHelperPluginConfigPage::moveSanitizeRuleUp()
{
    const auto current = m_completion_settings->sanitizeRules->currentRow();
    if (current)
    {
        kDebug(DEBUG_AREA) << "Current rule row " << current;
        swapRuleRows(current - 1, current);
        Q_EMIT(changed());
    }
}

void CppHelperPluginConfigPage::moveSanitizeRuleDown()
{
    const auto current = m_completion_settings->sanitizeRules->currentRow();
    if (current < m_completion_settings->sanitizeRules->rowCount() - 1)
    {
        kDebug(DEBUG_AREA) << "Current rule row " << current;
        swapRuleRows(current, current + 1);
        Q_EMIT(changed());
    }
}

std::pair<bool, QString> CppHelperPluginConfigPage::isSanitizeRuleValid(
    const int row
  , const int column
  ) const
{
    if (!column)                                            // Only 1st column can be validated...
    {
        auto* const item = m_completion_settings->sanitizeRules->item(row, column);
        auto expr = QRegExp{item->text()};
        kDebug(DEBUG_AREA) << "Validate regex text: " << item->text() << ", pattern text:" << expr.pattern();
        return std::make_pair(expr.isValid(), expr.errorString());
    }
    /// \todo Make sure that \e replace part contains valid number of
    /// capture contexts...
    return std::make_pair(true, QString{});
}

void CppHelperPluginConfigPage::validateSanitizeRule(const int row, const int column)
{
    kDebug(DEBUG_AREA) << "Sanitize rule has been changed: row =" << row << ", col =" << column;
    const auto p = isSanitizeRuleValid(row, column);
    if (!p.first)
    {
        KPassivePopup::message(
            i18nc("@title:window", "Error")
          , i18nc(
                "@info:tooltip"
              , "Regular expression at (%1, %2) is not valid: %3"
              , row
              , column
              , p.second
              )
          , qobject_cast<QWidget*>(this)
          );
        /// \todo How to enter in edit mode?
        m_completion_settings->sanitizeRules->cellWidget(row, column)->setFocus();
    }
    Q_EMIT(changed());
}

void CppHelperPluginConfigPage::exportSanitizeRules()
{
    const auto& export_to = KFileDialog::getSaveFileName(
        KUrl()
      , QString()
      , this
      , i18nc("@title:window", "Export completion sanitizer rules to...")
      , KFileDialog::ConfirmOverwrite
      );
    auto cfg = KSharedConfig::openConfig(export_to, KConfig::SimpleConfig);
    KConfigGroup the_only_group(cfg, SANITIZER_RULES_GROUP_NAME);
    m_plugin->config().writeSanitizeRulesTo(the_only_group);
}

void CppHelperPluginConfigPage::importSanitizeRules()
{
    const auto& export_to = KFileDialog::getOpenFileName(
        KUrl()
      , QString()
      , this
      , i18nc("@title:window", "Import completion sanitizer rules from...")
      );
    auto cfg = KSharedConfig::openConfig(export_to, KConfig::SimpleConfig);
    KConfigGroup the_only_group(cfg, SANITIZER_RULES_GROUP_NAME);
    m_plugin->config().readSanitizeRulesFrom(
        the_only_group
      , !m_completion_settings->appendOnImport->isChecked()
      );
    // Refresh view after import
    pullSanitizeRules();
}

void CppHelperPluginConfigPage::pullSanitizeRules()
{
    const auto& rules = m_plugin->config().sanitizeRules();
    m_completion_settings->sanitizeRules->clear();
    m_completion_settings->sanitizeRules->setRowCount(rules.size());
    kDebug(DEBUG_AREA) << "Sanitize rules count: " << rules.size();
    auto row = 0;
    for (const auto& rule : rules)
    {
        auto* find = new QTableWidgetItem{rule.first.pattern()};
        auto* replace = new QTableWidgetItem{rule.second};
        m_completion_settings->sanitizeRules->setItem(row, 0, find);
        m_completion_settings->sanitizeRules->setItem(row, 1, replace);
        row++;
        kDebug(DEBUG_AREA) << row << ") setting find =" << find << ", replace =" << replace;
    }

    if (rules.empty())
        m_completion_settings->sanitizeRules->setColumnWidth(
            0
          , m_completion_settings->sanitizeRules->size().width() / 2
          );
    else
        m_completion_settings->sanitizeRules->resizeColumnsToContents();

    /// \todo Why headers text can't be taken from \c ui file?
    m_completion_settings->sanitizeRules->setHorizontalHeaderItem(
        0
      , new QTableWidgetItem{i18nc("@title:column", "Find")}
      );
    m_completion_settings->sanitizeRules->setHorizontalHeaderItem(
        1
      , new QTableWidgetItem{i18nc("@title:column", "Replace")}
      );
}

void CppHelperPluginConfigPage::pushSanitizeRules()
{
    // Push sanitize rules back to plugin's configuration
    auto rules = PluginConfiguration::sanitize_rules_list_type{};
    rules.reserve(m_completion_settings->sanitizeRules->rowCount());
    for (auto row = 0; row != m_completion_settings->sanitizeRules->rowCount(); ++row)
    {
        auto* find_item = m_completion_settings->sanitizeRules->item(row, 0);
        auto* repl_item = m_completion_settings->sanitizeRules->item(row, 1);
        auto find_regex = QRegExp{find_item->text()};
        if (find_regex.isValid())
            rules.emplace_back(find_regex, repl_item->text());
        else
            kWarning() << "Ignore sanitize rule w/ invalid regex" << find_item->text();
    }
    kDebug(DEBUG_AREA) << rules.size() << " sanitize rules collected";
    m_plugin->config().setSanitizeRules(std::move(rules));
}

//END CppHelperPluginConfigPage
}                                                           // namespace kate
// kate: hl C++/Qt4;
