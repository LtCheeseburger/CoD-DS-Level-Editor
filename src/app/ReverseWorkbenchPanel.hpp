#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QWidget>

#include <filesystem>

#include "codds/ReverseCandidate.hpp"

namespace app
{
    class ReverseWorkbenchPanel final : public QWidget
    {
        Q_OBJECT

    public:
        explicit ReverseWorkbenchPanel(QWidget* parent = nullptr);

        void setCurrentFile(const std::filesystem::path& path);
        void clearCurrentFile();
        void setLoadedLevelFileCount(int count);
        codds::ReverseLayoutSettings settings() const;
        std::filesystem::path currentFile() const;

    public slots:
        void setXOffset(int offset);
        void setYOffset(int offset);
        void setZOffset(int offset);
        void setCoordinateOffsets(int xOffset, int yOffset, int zOffset);

    signals:
        void settingsChanged();

    private:
        void emitSettingsChanged();

        QLabel* m_currentFileLabel = nullptr;
        QComboBox* m_structSizeCombo = nullptr;
        QComboBox* m_coordinateFormatCombo = nullptr;
        QSpinBox* m_startOffsetSpin = nullptr;
        QSpinBox* m_xOffsetSpin = nullptr;
        QSpinBox* m_yOffsetSpin = nullptr;
        QSpinBox* m_zOffsetSpin = nullptr;
        QSpinBox* m_maxRecordsSpin = nullptr;
        QCheckBox* m_pointPreviewCheck = nullptr;
        QCheckBox* m_normalizeScaleCheck = nullptr;
        QCheckBox* m_drawGridCheck = nullptr;
        QCheckBox* m_drawStrideLinesCheck = nullptr;
        QCheckBox* m_scanEntireLevelCheck = nullptr;
        QCheckBox* m_spawnObjectCubesCheck = nullptr;
        QCheckBox* m_renderResolvedModelsCheck = nullptr;
        QCheckBox* m_objectsLpVariableRecordModeCheck = nullptr;
        QSpinBox* m_recordSizeOffsetSpin = nullptr;
        QSpinBox* m_modelIndexOffsetSpin = nullptr;
        QDoubleSpinBox* m_objectCubeSizeSpin = nullptr;
        QDoubleSpinBox* m_scaleMultiplierSpin = nullptr;
        QLabel* m_loadedLevelFilesLabel = nullptr;

        std::filesystem::path m_currentFile;
    };
}
