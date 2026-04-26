#pragma once

#include <QMainWindow>
#include <QTreeWidget>

#include <filesystem>
#include <vector>

#include "app/HexInspectorPanel.hpp"
#include "app/ObjectsLpAnalyzerPanel.hpp"
#include "app/ReverseWorkbenchPanel.hpp"
#include "app/ViewportWidget.hpp"
#include "codds/ModelLibrary.hpp"

namespace app
{
    class MainWindow final : public QMainWindow
    {
        Q_OBJECT

    public:
        explicit MainWindow(QWidget* parent = nullptr);

        enum class ItemDataRole
        {
            AbsolutePath = Qt::UserRole + 1,
            FileRole = Qt::UserRole + 2,
            Source = Qt::UserRole + 3
        };

        enum class ItemSource
        {
            None,
            LevelLocal,
            GameModelLibrary
        };

    private slots:
        void openLevelFolder();
        void openNsbmdFile();
        void setGameModelsFolder();
        void onAssetSelectionChanged();
        void onReverseSettingsChanged();

    private:
        void buildMenus();
        void buildDockPanels();
        void autoLoadDefaultModelsFolder();
        void clearAssetTree();
        void populateModelLibraryTree();
        void showNsbmdHeaderInTree(QTreeWidgetItem* parent, const std::filesystem::path& path);

        ViewportWidget* m_viewport = nullptr;
        QTreeWidget* m_assetTree = nullptr;
        HexInspectorPanel* m_hexInspector = nullptr;
        ObjectsLpAnalyzerPanel* m_objectsLpAnalyzer = nullptr;
        ReverseWorkbenchPanel* m_reverseWorkbench = nullptr;
        QTreeWidgetItem* m_modelLibraryRoot = nullptr;

        codds::ModelLibrary m_modelLibrary;
        std::vector<std::filesystem::path> m_loadedReverseFiles;
    };
}
