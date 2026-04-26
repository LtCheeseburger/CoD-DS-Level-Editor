#include "MainWindow.hpp"

#include <QDockWidget>
#include <QFileDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QCoreApplication>
#include <QTreeWidgetItem>
#include <QIcon>

#include <utility>

#include "codds/LevelFolder.hpp"
#include "codds/ReversePointScanner.hpp"
#include "codds/ReverseObjectScanner.hpp"
#include "core/Logger.hpp"
#include "nitro/NsbmdParser.hpp"

namespace app
{
    namespace
    {
        QString byteSizeText(std::uintmax_t sizeBytes)
        {
            return QString("%1 bytes").arg(static_cast<qulonglong>(sizeBytes));
        }

        QTreeWidgetItem* addCategory(QTreeWidgetItem* root, const QString& name, int count)
        {
            auto* item = new QTreeWidgetItem(root);
            item->setText(0, QString("%1 (%2)").arg(name).arg(count));
            return item;
        }

        int roleToInt(codds::LevelFileRole role)
        {
            return static_cast<int>(role);
        }

        codds::LevelFileRole intToRole(int value)
        {
            return static_cast<codds::LevelFileRole>(value);
        }

        int sourceToInt(MainWindow::ItemSource source)
        {
            return static_cast<int>(source);
        }

        void applyFileItemData(
            QTreeWidgetItem* item,
            const std::filesystem::path& path,
            codds::LevelFileRole role,
            MainWindow::ItemSource source)
        {
            item->setData(0, static_cast<int>(MainWindow::ItemDataRole::AbsolutePath), QString::fromStdString(path.string()));
            item->setData(0, static_cast<int>(MainWindow::ItemDataRole::FileRole), roleToInt(role));
            item->setData(0, static_cast<int>(MainWindow::ItemDataRole::Source), sourceToInt(source));
        }

        void addFileItems(QTreeWidgetItem* parent, const std::vector<codds::LevelFileEntry>& entries)
        {
            for (const auto& entry : entries)
            {
                auto* fileItem = new QTreeWidgetItem(parent);
                fileItem->setText(0, QString::fromStdString(entry.path.filename().string()));
                fileItem->setToolTip(0, QString::fromStdString(entry.path.string()) + "\n" + byteSizeText(entry.sizeBytes));
                applyFileItemData(fileItem, entry.path, entry.role, MainWindow::ItemSource::LevelLocal);
            }
        }

        bool isReverseEngineeringRole(codds::LevelFileRole role)
        {
            return role == codds::LevelFileRole::ObjectPlacement_LP ||
                   role == codds::LevelFileRole::RoomLayout_ROO ||
                   role == codds::LevelFileRole::Collision_CVHM ||
                   role == codds::LevelFileRole::Script_NSLBC;
        }

        bool isObjectsLpPath(const std::filesystem::path& path)
        {
            return QString::fromStdString(path.filename().string()).compare("Objects.LP", Qt::CaseInsensitive) == 0;
        }

        codds::ReverseLayoutSettings fixedRecordCopy(codds::ReverseLayoutSettings settings)
        {
            // The Objects.LP variable-record parser is specific to Objects.LP.
            // Full-folder scans must not apply that parser to ROO/CVHM/NSLBC, because
            // arbitrary bytes can look like plausible record sizes and produce invalid streams.
            settings.objectsLpVariableRecordMode = false;
            settings.spawnObjectCubes = false;
            return settings;
        }
    }

    MainWindow::MainWindow(QWidget* parent)
        : QMainWindow(parent)
    {
        setWindowTitle("CoD DS Level Studio v0.1.2");
        setWindowIcon(QIcon(":/app/icon.png"));

        m_viewport = new ViewportWidget(this);
        setCentralWidget(m_viewport);

        buildMenus();
        buildDockPanels();
        autoLoadDefaultModelsFolder();
    }

    void MainWindow::buildMenus()
    {
        auto* fileMenu = menuBar()->addMenu("&File");

        auto* openLevelAction = fileMenu->addAction("Open Level Folder...");
        connect(openLevelAction, &QAction::triggered, this, &MainWindow::openLevelFolder);

        auto* setModelsFolderAction = fileMenu->addAction("Set Game Models Folder...");
        connect(setModelsFolderAction, &QAction::triggered, this, &MainWindow::setGameModelsFolder);

        auto* openNsbmdAction = fileMenu->addAction("Open NSBMD...");
        connect(openNsbmdAction, &QAction::triggered, this, &MainWindow::openNsbmdFile);

        fileMenu->addSeparator();

        auto* exitAction = fileMenu->addAction("Exit");
        connect(exitAction, &QAction::triggered, this, &QWidget::close);
    }

    void MainWindow::buildDockPanels()
    {
        auto* assetDock = new QDockWidget("Level / Asset Browser", this);
        m_assetTree = new QTreeWidget(assetDock);
        m_assetTree->setHeaderLabel("CoD DS Level");
        assetDock->setWidget(m_assetTree);
        addDockWidget(Qt::LeftDockWidgetArea, assetDock);

        connect(m_assetTree, &QTreeWidget::itemSelectionChanged, this, &MainWindow::onAssetSelectionChanged);

        auto* hexDock = new QDockWidget("Hex Inspector", this);
        m_hexInspector = new HexInspectorPanel(hexDock);
        hexDock->setWidget(m_hexInspector);
        addDockWidget(Qt::BottomDockWidgetArea, hexDock);

        auto* objectsDock = new QDockWidget("Objects.LP Analyzer", this);
        m_objectsLpAnalyzer = new ObjectsLpAnalyzerPanel(objectsDock);
        m_objectsLpAnalyzer->setModelLibrary(&m_modelLibrary);
        objectsDock->setWidget(m_objectsLpAnalyzer);
        addDockWidget(Qt::BottomDockWidgetArea, objectsDock);
        tabifyDockWidget(hexDock, objectsDock);
        hexDock->raise();

        auto* reverseDock = new QDockWidget("Reverse Engineering Workbench", this);
        m_reverseWorkbench = new ReverseWorkbenchPanel(reverseDock);
        reverseDock->setWidget(m_reverseWorkbench);
        addDockWidget(Qt::RightDockWidgetArea, reverseDock);

        connect(m_reverseWorkbench, &ReverseWorkbenchPanel::settingsChanged, this, &MainWindow::onReverseSettingsChanged);

        connect(m_objectsLpAnalyzer, &ObjectsLpAnalyzerPanel::requestSetXOffset, m_reverseWorkbench, &ReverseWorkbenchPanel::setXOffset);
        connect(m_objectsLpAnalyzer, &ObjectsLpAnalyzerPanel::requestSetYOffset, m_reverseWorkbench, &ReverseWorkbenchPanel::setYOffset);
        connect(m_objectsLpAnalyzer, &ObjectsLpAnalyzerPanel::requestSetZOffset, m_reverseWorkbench, &ReverseWorkbenchPanel::setZOffset);
        connect(m_objectsLpAnalyzer, &ObjectsLpAnalyzerPanel::requestSetCoordinateOffsets, m_reverseWorkbench, &ReverseWorkbenchPanel::setCoordinateOffsets);
    }

    void MainWindow::autoLoadDefaultModelsFolder()
    {
        const auto exeDir = std::filesystem::path(QCoreApplication::applicationDirPath().toStdString());
        const auto modelsDir = exeDir / "Models";

        if (!std::filesystem::exists(modelsDir) || !std::filesystem::is_directory(modelsDir))
        {
            core::Logger::info("Default Models folder not found beside executable: " + modelsDir.string());
            return;
        }

        core::Logger::info("Auto-loading default Models folder: " + modelsDir.string());
        m_modelLibrary.scan(modelsDir);

        if (m_objectsLpAnalyzer)
            m_objectsLpAnalyzer->setModelLibrary(&m_modelLibrary);

        populateModelLibraryTree();
    }

    void MainWindow::clearAssetTree()
    {
        if (m_assetTree)
            m_assetTree->clear();
        m_modelLibraryRoot = nullptr;
        m_loadedReverseFiles.clear();
        if (m_reverseWorkbench)
            m_reverseWorkbench->setLoadedLevelFileCount(0);
    }

    void MainWindow::openLevelFolder()
    {
        const QString folderPath = QFileDialog::getExistingDirectory(
            this,
            "Open CoD DS Level Folder"
        );

        if (folderPath.isEmpty())
            return;

        try
        {
            const auto level = codds::LevelFolderScanner::scan(folderPath.toStdString());

            clearAssetTree();

            auto collectReverseFiles = [this](const std::vector<codds::LevelFileEntry>& entries)
            {
                for (const auto& entry : entries)
                    m_loadedReverseFiles.push_back(entry.path);
            };
            collectReverseFiles(level.objectPlacement);
            collectReverseFiles(level.roomLayout);
            collectReverseFiles(level.collision);
            collectReverseFiles(level.scripts);

            if (m_reverseWorkbench)
                m_reverseWorkbench->setLoadedLevelFileCount(static_cast<int>(m_loadedReverseFiles.size()));

            auto* root = new QTreeWidgetItem(m_assetTree);
            root->setText(0, QString::fromStdString(level.rootPath.filename().string()));
            root->setToolTip(0, QString::fromStdString(level.rootPath.string()));

            auto* models = addCategory(root, "Level Models / NSBMD", static_cast<int>(level.models.size()));
            addFileItems(models, level.models);

            auto* placement = addCategory(root, "Object Placement / LP", static_cast<int>(level.objectPlacement.size()));
            addFileItems(placement, level.objectPlacement);

            auto* rooms = addCategory(root, "Rooms / ROO", static_cast<int>(level.roomLayout.size()));
            addFileItems(rooms, level.roomLayout);

            auto* collision = addCategory(root, "Collision / CVHM", static_cast<int>(level.collision.size()));
            addFileItems(collision, level.collision);

            auto* scripts = addCategory(root, "Scripts / NSLBC", static_cast<int>(level.scripts.size()));
            addFileItems(scripts, level.scripts);

            if (!level.unknown.empty())
            {
                auto* unknown = addCategory(root, "Unknown", static_cast<int>(level.unknown.size()));
                addFileItems(unknown, level.unknown);
                unknown->setExpanded(true);
            }

            populateModelLibraryTree();

            root->setExpanded(true);
            models->setExpanded(true);
            placement->setExpanded(true);
            rooms->setExpanded(true);
            collision->setExpanded(true);
            scripts->setExpanded(true);
        }
        catch (const std::exception& e)
        {
            QMessageBox::critical(this, "Failed To Open Level Folder", e.what());
        }
    }

    void MainWindow::openNsbmdFile()
    {
        const QString path = QFileDialog::getOpenFileName(
            this,
            "Open NSBMD File",
            QString(),
            "Nintendo DS Model Files (*.nsbmd *.bmd *.bdl);;All Files (*.*)"
        );

        if (path.isEmpty())
            return;

        const auto parsed = nitro::NsbmdParser::parseHeaderOnly(path.toStdString());

        if (!parsed.valid)
        {
            QMessageBox::warning(this, "Invalid File", "This does not appear to be a valid BMD0/BDL0 file.");
            return;
        }

        auto* root = new QTreeWidgetItem(m_assetTree);
        root->setText(0, QString::fromStdString(parsed.path.filename().string()));
        applyFileItemData(root, parsed.path, codds::LevelFileRole::ModelAsset_NSBMD, ItemSource::LevelLocal);
        showNsbmdHeaderInTree(root, parsed.path);
        root->setExpanded(true);
    }

    void MainWindow::setGameModelsFolder()
    {
        const QString folderPath = QFileDialog::getExistingDirectory(
            this,
            "Set Game Models Folder"
        );

        if (folderPath.isEmpty())
            return;

        m_modelLibrary.scan(folderPath.toStdString());
        if (m_objectsLpAnalyzer)
            m_objectsLpAnalyzer->setModelLibrary(&m_modelLibrary);
        populateModelLibraryTree();
    }

    void MainWindow::populateModelLibraryTree()
    {
        if (!m_assetTree)
            return;

        if (m_modelLibraryRoot)
        {
            delete m_modelLibraryRoot;
            m_modelLibraryRoot = nullptr;
        }

        if (m_modelLibrary.assets().empty())
            return;

        m_modelLibraryRoot = new QTreeWidgetItem(m_assetTree);
        m_modelLibraryRoot->setText(0, QString("Game Model Library (%1)").arg(static_cast<int>(m_modelLibrary.assets().size())));
        m_modelLibraryRoot->setToolTip(0, QString::fromStdString(m_modelLibrary.rootPath().string()));

        for (const auto& asset : m_modelLibrary.assets())
        {
            auto* item = new QTreeWidgetItem(m_modelLibraryRoot);
            item->setText(0, QString::fromStdString(asset.relativePath.generic_string()));
            item->setToolTip(0, QString::fromStdString(asset.absolutePath.string()));
            applyFileItemData(item, asset.absolutePath, codds::LevelFileRole::ModelAsset_NSBMD, ItemSource::GameModelLibrary);
        }

        m_modelLibraryRoot->setExpanded(true);
    }

    void MainWindow::onAssetSelectionChanged()
    {
        const auto selected = m_assetTree->selectedItems();
        if (selected.empty())
            return;

        auto* item = selected.front();
        const QVariant pathData = item->data(0, static_cast<int>(ItemDataRole::AbsolutePath));
        const QVariant roleData = item->data(0, static_cast<int>(ItemDataRole::FileRole));

        if (!pathData.isValid() || !roleData.isValid())
            return;

        const std::filesystem::path path = pathData.toString().toStdString();
        const auto role = intToRole(roleData.toInt());

        if (isReverseEngineeringRole(role))
        {
            m_hexInspector->loadFile(path);
            m_reverseWorkbench->setCurrentFile(path);

            if (m_objectsLpAnalyzer)
            {
                if (role == codds::LevelFileRole::ObjectPlacement_LP)
                    m_objectsLpAnalyzer->loadFile(path);
                else
                    m_objectsLpAnalyzer->clear();
            }

            return;
        }

        if (role == codds::LevelFileRole::ModelAsset_NSBMD)
        {
            m_reverseWorkbench->clearCurrentFile();
            m_viewport->clearPointCloud();
            if (m_objectsLpAnalyzer)
                m_objectsLpAnalyzer->clear();
            showNsbmdHeaderInTree(item, path);
        }
    }

    void MainWindow::onReverseSettingsChanged()
    {
        if (!m_reverseWorkbench)
            return;

        const auto settings = m_reverseWorkbench->settings();

        if (m_objectsLpAnalyzer)
            m_objectsLpAnalyzer->setSettings(settings);

        if (!settings.pointPreviewEnabled)
        {
            m_viewport->clearPointCloud();
            return;
        }

        const auto currentPath = m_reverseWorkbench->currentFile();

        if (settings.spawnObjectCubes)
        {
            if (currentPath.empty())
                return;

            const auto fileName = QString::fromStdString(currentPath.filename().string()).toLower();
            if (fileName != "objects.lp")
            {
                core::Logger::warning("Object cube preview requires Objects.LP as the current file");
                m_viewport->clearPointCloud();
                return;
            }

            auto objects = codds::ReverseObjectScanner::scanObjects(currentPath, settings, &m_modelLibrary);
            core::Logger::info("Workbench object cube result: " + std::to_string(objects.size()) + " object candidate(s)");
            m_viewport->setObjectCubes(std::move(objects), settings);
            return;
        }

        std::vector<codds::ReversePoint> allPoints;

        if (settings.scanEntireLevelFolder)
        {
            constexpr std::size_t kFullLevelPointCap = 50000;

            for (const auto& file : m_loadedReverseFiles)
            {
                codds::ReverseLayoutSettings scanSettings = settings;

                if (settings.objectsLpVariableRecordMode)
                {
                    if (!isObjectsLpPath(file))
                    {
                        core::Logger::debug("Full-level scan skipped variable Objects.LP mode for " + file.filename().string());
                        continue;
                    }
                }
                else if (!isObjectsLpPath(file))
                {
                    scanSettings = fixedRecordCopy(settings);
                }

                auto points = codds::ReversePointScanner::scanPoints(file, scanSettings);

                const std::size_t remaining = kFullLevelPointCap > allPoints.size()
                    ? kFullLevelPointCap - allPoints.size()
                    : 0;

                if (remaining == 0)
                    break;

                if (points.size() > remaining)
                    points.resize(remaining);

                allPoints.insert(allPoints.end(), points.begin(), points.end());
            }

            core::Logger::info("Workbench full-level scan result: " + std::to_string(allPoints.size()) + " candidate point(s)");
        }
        else
        {
            if (currentPath.empty())
                return;

            allPoints = codds::ReversePointScanner::scanPoints(currentPath, settings);
            core::Logger::info("Workbench scan result: " + std::to_string(allPoints.size()) + " candidate point(s)");
        }

        m_viewport->setPointCloud(std::move(allPoints), settings);
    }

    void MainWindow::showNsbmdHeaderInTree(QTreeWidgetItem* parent, const std::filesystem::path& path)
    {
        if (!parent)
            return;

        for (int i = parent->childCount() - 1; i >= 0; --i)
        {
            if (parent->child(i)->text(0) == "NSBMD Header")
                delete parent->takeChild(i);
        }

        const auto parsed = nitro::NsbmdParser::parseHeaderOnly(path);
        if (!parsed.valid)
            return;

        auto* headerRoot = new QTreeWidgetItem(parent);
        headerRoot->setText(0, "NSBMD Header");

        auto* magic = new QTreeWidgetItem(headerRoot);
        magic->setText(0, QString("Magic: %1").arg(QString::fromStdString(parsed.magic)));

        auto* version = new QTreeWidgetItem(headerRoot);
        version->setText(0, QString("Version: 0x%1").arg(parsed.version, 4, 16, QChar('0')));

        auto* fileSize = new QTreeWidgetItem(headerRoot);
        fileSize->setText(0, QString("File size: %1").arg(parsed.fileSize));

        auto* sections = new QTreeWidgetItem(headerRoot);
        sections->setText(0, QString("Sections (%1)").arg(parsed.sections.size()));

        for (const auto& section : parsed.sections)
        {
            auto* item = new QTreeWidgetItem(sections);
            item->setText(
                0,
                QString("%1 @ 0x%2")
                    .arg(QString::fromStdString(section.tag.empty() ? "UNKNOWN" : section.tag))
                    .arg(section.offset, 8, 16, QChar('0'))
            );
        }

        headerRoot->setExpanded(true);
        sections->setExpanded(true);
        parent->setExpanded(true);
    }
}
