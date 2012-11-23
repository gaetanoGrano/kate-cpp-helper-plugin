/**
 * \file
 *
 * \brief Class \c kate::IncludeHelperPlugin (implementation)
 *
 * \date Sun Jan 29 09:15:53 MSK 2012 -- Initial design
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
#include <src/config.h>
#include <src/include_helper_plugin.h>
#include <src/include_helper_plugin_config_page.h>
#include <src/include_helper_plugin_view.h>
#include <src/document_info.h>
#include <src/translation_unit.h>
#include <src/utils.h>

// Standard includes
#include <kate/application.h>
#include <kate/mainwindow.h>
#include <KAboutData>
#include <KDebug>
#include <KPluginFactory>
#include <KPluginLoader>
#include <KTextEditor/MovingInterface>
#include <QtCore/QFileInfo>

K_PLUGIN_FACTORY(IncludeHelperPluginFactory, registerPlugin<kate::IncludeHelperPlugin>();)
K_EXPORT_PLUGIN(
    IncludeHelperPluginFactory(
        KAboutData(
            "kateincludehelperplugin"
          , "kate_includehelper_plugin"
          , ki18n("Include Helper Plugin")
          , PLUGIN_VERSION
          , ki18n("Helps to work w/ C/C++ headers little more easy")
          , KAboutData::License_LGPL_V3
          )
      )
  )

namespace kate {
//BEGIN IncludeHelperPlugin
IncludeHelperPlugin::IncludeHelperPlugin(
    QObject* application
  , const QList<QVariant>&
  )
  : Kate::Plugin(static_cast<Kate::Application*>(application), "kate_includehelper_plugin")
  /// \todo Make parameters to \c clang_createIndex() configurable?
  , m_index(clang_createIndex(0, 0))
{
    assert("clang index expected to be valid" && m_index);
    // Connect self to configuration updates
    connect(
        &m_config
      , SIGNAL(dirWatchSettingsChanged())
      , this
      , SLOT(updateDirWatcher())
      );
    connect(
        &m_config
      , SIGNAL(precompiledHeaderFileChanged())
      , this
      , SLOT(refreshPCH())
      );
}

IncludeHelperPlugin::~IncludeHelperPlugin()
{
    kDebug() << "Unloading...";
    Q_FOREACH(doc_info_type::mapped_type info, m_doc_info)
    {
        delete info;
    }
}

Kate::PluginView* IncludeHelperPlugin::createView(Kate::MainWindow* parent)
{
    return new IncludeHelperPluginView(parent, IncludeHelperPluginFactory::componentData(), this);
}

Kate::PluginConfigPage* IncludeHelperPlugin::configPage(uint number, QWidget* parent, const char*)
{
    assert("This plugin have the only configuration page" && number == 0);
    if (number != 0)
        return 0;
    return new IncludeHelperPluginConfigPage(parent, this);
}

void IncludeHelperPlugin::updateDirWatcher(const QString& path)
{
    m_dir_watcher->addDir(path, KDirWatch::WatchSubDirs | KDirWatch::WatchFiles);
    connect(
        m_dir_watcher.get()
      , SIGNAL(created(const QString&))
      , this
      , SLOT(createdPath(const QString&))
      );
    connect(
        m_dir_watcher.get()
      , SIGNAL(deleted(const QString&))
      , this
      , SLOT(deletedPath(const QString&))
      );
}

void IncludeHelperPlugin::updateDirWatcher()
{
    if (m_dir_watcher)
        m_dir_watcher->stopScan();
    m_dir_watcher.reset(new KDirWatch(0));
    m_dir_watcher->stopScan();

    if (config().what_to_monitor() & 2)
    {
        kDebug() << "Going to monitor system dirs for changes...";
        for (const QString& path : config().systemDirs())
            updateDirWatcher(path);
    }
    if (config().what_to_monitor() & 1)
    {
        kDebug() << "Going to monitor session dirs for changes...";
        for (const QString& path : config().sessionDirs())
            updateDirWatcher(path);
    }

    m_dir_watcher->startScan(true);
}

void IncludeHelperPlugin::createdPath(const QString& path)
{
    // No reason to call update if it is just a dir was created...
    if (QFileInfo(path).isFile() && m_last_updated != path)
    {
        kDebug() << "DirWatcher said created: " << path;
        updateCurrentView();
        m_last_updated = path;
    }
}

void IncludeHelperPlugin::deletedPath(const QString& path)
{
    if (m_last_updated != path)
    {
        kDebug() << "DirWatcher said deleted: " << path;
        updateCurrentView();
        m_last_updated = path;
    }
}

void IncludeHelperPlugin::updateCurrentView()
{
    KTextEditor::View* view = application()->activeMainWindow()->activeView();
    if (view)
    {
        updateDocumentInfo(view->document());
    }
}

void IncludeHelperPlugin::updateDocumentInfo(KTextEditor::Document* doc)
{
    assert("Valid document expected" && doc);
    kDebug() << "(re)scan document " << doc << " for #includes...";
    KTextEditor::MovingInterface* mv_iface = qobject_cast<KTextEditor::MovingInterface*>(doc);
    if (!mv_iface)
    {
        kDebug() << "No moving iface!!!!!!!!!!!";
        return;
    }

    // Try to remove prev collected info
    {
        IncludeHelperPlugin::doc_info_type::iterator it = managed_docs().find(doc);
        if (it != managed_docs().end())
        {
            delete *it;
            managed_docs().erase(it);
        }
    }

    DocumentInfo* di = new DocumentInfo(this);

    // Search lines and filenames #include'd in this document
    for (int i = 0, end_line = doc->lines(); i < end_line; i++)
    {
        const QString& line_str = doc->line(i);
        kate::IncludeParseResult r = parseIncludeDirective(line_str, false);
        if (r.m_range.isValid())
        {
            r.m_range.setBothLines(i);
            di->addRange(
                mv_iface->newMovingRange(
                    r.m_range
                  , KTextEditor::MovingRange::ExpandLeft | KTextEditor::MovingRange::ExpandRight
                  )
              );
        }
    }
    managed_docs().insert(doc, di);
}

/// \todo Move this method to view class. View may access managed documents via accessor method.
void IncludeHelperPlugin::textInserted(KTextEditor::Document* doc, const KTextEditor::Range& range)
{
    kDebug() << doc << " new text: " << doc->text(range);
    KTextEditor::MovingInterface* mv_iface = qobject_cast<KTextEditor::MovingInterface*>(doc);
    if (!mv_iface)
    {
        kDebug() << "No moving iface!!!!!!!!!!!";
        return;
    }
    // Find corresponding document info, insert if needed
    IncludeHelperPlugin::doc_info_type::iterator it = managed_docs().find(doc);
    if (it == managed_docs().end())
    {
        it = managed_docs().insert(doc, new DocumentInfo(this));
    }
    // Search lines and filenames #include'd in this range
    for (int i = range.start().line(); i < range.end().line() + 1; i++)
    {
        const QString& line_str = doc->line(i);
        kate::IncludeParseResult r = parseIncludeDirective(line_str, true);
        if (r.m_range.isValid())
        {
            r.m_range.setBothLines(i);
            if (!(*it)->isRangeWithSameExists(r.m_range))
            {
                (*it)->addRange(
                    mv_iface->newMovingRange(
                        r.m_range
                      , KTextEditor::MovingRange::ExpandLeft | KTextEditor::MovingRange::ExpandRight
                      )
                  );
            } else kDebug() << "range already registered";
        }
        else kDebug() << "no valid #include found";
    }
}

/// Used by config page to open a PCH header
void IncludeHelperPlugin::openDocument(const KUrl& pch_header)
{
    application()->activeMainWindow()->openUrl(pch_header);
}

/**
 * \attention Clang has some problems (core dump) on loading a PCH file if latter
 * doesn't exists before call to \c TranslationUnit ctor. As I so in \c gdb, it has
 * (seem) uninitialized counter for tokens in a \c Preprocessor class (due some kind
 * of delayed initialization or smth like that)... So it is why presence of a PCH
 * must be checked before...
 * \todo Investigate a bug in clang 3.1
 */
void IncludeHelperPlugin::refreshPCH(bool force_recompile)
{
    if (config().precompiledHeaderFile().isEmpty())
    {
        kDebug() << "No PCH file configured! Code completion will be slooow!";
        return;
    }

    const QString pch_file_name = config().precompiledHeaderFile().toLocalFile() + ".kate.pch";
    QFileInfo pi(pch_file_name);
    if (!pi.exists())
        force_recompile = true;
    try
    {
        std::unique_ptr<TranslationUnit> pch_unit;
        if (force_recompile)
        {
            kDebug() << "Going to produce a PCH file" << pch_file_name
              << "from" << config().precompiledHeaderFile();

            pch_unit.reset(
                new TranslationUnit(
                    index()
                  , config().precompiledHeaderFile()
                  , config().formCompilerOptions()
                  , TranslationUnit::ParseOptions::PCHOptions
                  )
              );
        }
        else
        {
            kDebug() << "(Re)loading the PCH file" << pch_file_name;
            pch_unit.reset(new TranslationUnit(index(), pch_file_name));
        }
        pch_unit->storeTo(pch_file_name);
    }
    catch (const TranslationUnit::Exception::LoadFailure&)
    {
        kDebug() << "Loading failure. Trying to recompile the PCH file...";
        // Ok, try to recompile
        refreshPCH(true);
        return;
    }
    catch (const TranslationUnit::Exception::ParseFailure&)
    {
        kError() << "PCH file failure. Code completion will be damn slow!";
        /// \todo Add an option to disable code completion w/o PCH file
        return;
    }
#if 0
    catch (const TranslationUnit::Exception::StoreFailure&)
    {
        kError() << "Unable to store a PCH file";
        if (!force_recompile)
            refreshPCH(true);
    }
#endif
    catch (const TranslationUnit::Exception&)
    {
        kError() << "Smth wrong w/ the PCH file";
    }
    config().setPrecompiledFile(pch_file_name);
    return;
}

//END IncludeHelperPlugin
}                                                           // namespace kate
// kate: hl C++11/Qt4
