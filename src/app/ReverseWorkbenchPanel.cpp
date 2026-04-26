#include "ReverseWorkbenchPanel.hpp"

#include <QFormLayout>
#include <QPushButton>
#include <QtGlobal>
#include <QVBoxLayout>

namespace app
{
    ReverseWorkbenchPanel::ReverseWorkbenchPanel(QWidget* parent)
        : QWidget(parent)
    {
        auto* outer = new QVBoxLayout(this);
        outer->setContentsMargins(8, 8, 8, 8);

        m_currentFileLabel = new QLabel("Current file: none", this);
        m_currentFileLabel->setWordWrap(true);
        outer->addWidget(m_currentFileLabel);

        m_loadedLevelFilesLabel = new QLabel("Loaded RE files: 0", this);
        outer->addWidget(m_loadedLevelFilesLabel);

        auto* form = new QFormLayout();
        outer->addLayout(form);

        m_structSizeCombo = new QComboBox(this);
        for (int size : {8, 12, 16, 20, 24, 28, 32, 36, 40, 48, 56, 64, 80, 96, 128})
            m_structSizeCombo->addItem(QString::number(size), size);
        m_structSizeCombo->setCurrentText("32");
        form->addRow("Struct size", m_structSizeCombo);

        m_coordinateFormatCombo = new QComboBox(this);
        m_coordinateFormatCombo->addItem("float32", static_cast<int>(codds::CoordinateFormat::Float32));
        m_coordinateFormatCombo->addItem("int16 fixed / 4096", static_cast<int>(codds::CoordinateFormat::Int16Fixed4096));
        m_coordinateFormatCombo->addItem("int32 fixed / 4096", static_cast<int>(codds::CoordinateFormat::Int32Fixed4096));
        form->addRow("Coordinate format", m_coordinateFormatCombo);

        m_startOffsetSpin = new QSpinBox(this);
        m_startOffsetSpin->setRange(0, 0x7fffffff);
        form->addRow("Start offset", m_startOffsetSpin);

        m_xOffsetSpin = new QSpinBox(this);
        m_xOffsetSpin->setRange(0, 1024);
        m_xOffsetSpin->setValue(0);
        form->addRow("X offset", m_xOffsetSpin);

        m_yOffsetSpin = new QSpinBox(this);
        m_yOffsetSpin->setRange(0, 1024);
        m_yOffsetSpin->setValue(4);
        form->addRow("Y offset", m_yOffsetSpin);

        m_zOffsetSpin = new QSpinBox(this);
        m_zOffsetSpin->setRange(0, 1024);
        m_zOffsetSpin->setValue(8);
        form->addRow("Z offset", m_zOffsetSpin);

        m_maxRecordsSpin = new QSpinBox(this);
        m_maxRecordsSpin->setRange(1, 1000000);
        m_maxRecordsSpin->setValue(2048);
        form->addRow("Max records", m_maxRecordsSpin);

        m_scaleMultiplierSpin = new QDoubleSpinBox(this);
        m_scaleMultiplierSpin->setRange(0.0001, 1000000.0);
        m_scaleMultiplierSpin->setDecimals(4);
        m_scaleMultiplierSpin->setSingleStep(0.25);
        m_scaleMultiplierSpin->setValue(1.0);
        form->addRow("Viewport scale", m_scaleMultiplierSpin);

        m_modelIndexOffsetSpin = new QSpinBox(this);
        m_modelIndexOffsetSpin->setRange(0, 1024);
        m_modelIndexOffsetSpin->setValue(0);
        form->addRow("Model index/object ID offset", m_modelIndexOffsetSpin);

        m_recordSizeOffsetSpin = new QSpinBox(this);
        m_recordSizeOffsetSpin->setRange(0, 1024);
        m_recordSizeOffsetSpin->setValue(0);
        form->addRow("Record size offset", m_recordSizeOffsetSpin);

        m_objectCubeSizeSpin = new QDoubleSpinBox(this);
        m_objectCubeSizeSpin->setRange(0.01, 1000.0);
        m_objectCubeSizeSpin->setDecimals(3);
        m_objectCubeSizeSpin->setSingleStep(0.05);
        m_objectCubeSizeSpin->setValue(0.35);
        form->addRow("Object cube size", m_objectCubeSizeSpin);

        m_pointPreviewCheck = new QCheckBox("Enable point preview", this);
        outer->addWidget(m_pointPreviewCheck);

        m_scanEntireLevelCheck = new QCheckBox("Scan entire loaded level folder", this);
        outer->addWidget(m_scanEntireLevelCheck);

        m_normalizeScaleCheck = new QCheckBox("Normalize scale", this);
        m_normalizeScaleCheck->setChecked(true);
        outer->addWidget(m_normalizeScaleCheck);

        m_drawGridCheck = new QCheckBox("Draw grid + XYZ axes", this);
        m_drawGridCheck->setChecked(true);
        outer->addWidget(m_drawGridCheck);

        m_drawStrideLinesCheck = new QCheckBox("Draw stride visualizer lines", this);
        outer->addWidget(m_drawStrideLinesCheck);

        m_spawnObjectCubesCheck = new QCheckBox("Spawn object cubes using model index", this);
        outer->addWidget(m_spawnObjectCubesCheck);

        m_renderResolvedModelsCheck = new QCheckBox("Render resolved NSBMD model instances", this);
        m_renderResolvedModelsCheck->setToolTip("Experimental: decodes native NSBMD display lists and renders primitive-aware wireframes; falls back to proxies when decode fails.");
        outer->addWidget(m_renderResolvedModelsCheck);

        m_objectsLpVariableRecordModeCheck = new QCheckBox("Objects.LP variable record mode", this);
        outer->addWidget(m_objectsLpVariableRecordModeCheck);

        auto* objectsPresetButton = new QPushButton("Apply Objects.LP model-instance preset", this);
        outer->addWidget(objectsPresetButton);

        outer->addStretch(1);

        connect(m_structSizeCombo, &QComboBox::currentIndexChanged, this, &ReverseWorkbenchPanel::emitSettingsChanged);
        connect(m_coordinateFormatCombo, &QComboBox::currentIndexChanged, this, &ReverseWorkbenchPanel::emitSettingsChanged);
        connect(m_startOffsetSpin, qOverload<int>(&QSpinBox::valueChanged), this, &ReverseWorkbenchPanel::emitSettingsChanged);
        connect(m_xOffsetSpin, qOverload<int>(&QSpinBox::valueChanged), this, &ReverseWorkbenchPanel::emitSettingsChanged);
        connect(m_yOffsetSpin, qOverload<int>(&QSpinBox::valueChanged), this, &ReverseWorkbenchPanel::emitSettingsChanged);
        connect(m_zOffsetSpin, qOverload<int>(&QSpinBox::valueChanged), this, &ReverseWorkbenchPanel::emitSettingsChanged);
        connect(m_maxRecordsSpin, qOverload<int>(&QSpinBox::valueChanged), this, &ReverseWorkbenchPanel::emitSettingsChanged);
        connect(m_scaleMultiplierSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &ReverseWorkbenchPanel::emitSettingsChanged);
        connect(m_modelIndexOffsetSpin, qOverload<int>(&QSpinBox::valueChanged), this, &ReverseWorkbenchPanel::emitSettingsChanged);
        connect(m_recordSizeOffsetSpin, qOverload<int>(&QSpinBox::valueChanged), this, &ReverseWorkbenchPanel::emitSettingsChanged);
        connect(m_objectCubeSizeSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &ReverseWorkbenchPanel::emitSettingsChanged);
        connect(m_pointPreviewCheck, &QCheckBox::toggled, this, &ReverseWorkbenchPanel::emitSettingsChanged);
        connect(m_scanEntireLevelCheck, &QCheckBox::toggled, this, [this](bool checked) {
            if (checked && m_spawnObjectCubesCheck->isChecked())
                m_spawnObjectCubesCheck->setChecked(false);
            emitSettingsChanged();
        });
        connect(m_normalizeScaleCheck, &QCheckBox::toggled, this, &ReverseWorkbenchPanel::emitSettingsChanged);
        connect(m_drawGridCheck, &QCheckBox::toggled, this, &ReverseWorkbenchPanel::emitSettingsChanged);
        connect(m_drawStrideLinesCheck, &QCheckBox::toggled, this, &ReverseWorkbenchPanel::emitSettingsChanged);
        connect(m_spawnObjectCubesCheck, &QCheckBox::toggled, this, [this](bool checked) {
            // Object cube preview is an Objects.LP-only interpretation. Do not combine it
            // with full-folder scans across ROO/CVHM/NSLBC.
            if (checked && m_scanEntireLevelCheck->isChecked())
                m_scanEntireLevelCheck->setChecked(false);
            emitSettingsChanged();
        });
        connect(m_renderResolvedModelsCheck, &QCheckBox::toggled, this, &ReverseWorkbenchPanel::emitSettingsChanged);
        connect(m_objectsLpVariableRecordModeCheck, &QCheckBox::toggled, this, &ReverseWorkbenchPanel::emitSettingsChanged);
        connect(objectsPresetButton, &QPushButton::clicked, this, [this]() {
            m_structSizeCombo->setCurrentText("36");
            m_coordinateFormatCombo->setCurrentIndex(m_coordinateFormatCombo->findData(static_cast<int>(codds::CoordinateFormat::Int32Fixed4096)));
            m_startOffsetSpin->setValue(0x18);
            m_xOffsetSpin->setValue(12);
            m_yOffsetSpin->setValue(8);
            m_zOffsetSpin->setValue(4);
            m_recordSizeOffsetSpin->setValue(0);
            m_modelIndexOffsetSpin->setValue(2);
            m_maxRecordsSpin->setValue(10000);
            m_pointPreviewCheck->setChecked(true);
            m_scanEntireLevelCheck->setChecked(false);
            m_spawnObjectCubesCheck->setChecked(true);
            m_renderResolvedModelsCheck->setChecked(true);
            m_objectsLpVariableRecordModeCheck->setChecked(true);
            emitSettingsChanged();
        });
    }

    void ReverseWorkbenchPanel::setCurrentFile(const std::filesystem::path& path)
    {
        m_currentFile = path;
        m_currentFileLabel->setText(QString("Current file: %1").arg(QString::fromStdString(path.filename().string())));
        emit settingsChanged();
    }

    void ReverseWorkbenchPanel::clearCurrentFile()
    {
        m_currentFile.clear();
        m_currentFileLabel->setText("Current file: none");
        emit settingsChanged();
    }

    void ReverseWorkbenchPanel::setLoadedLevelFileCount(int count)
    {
        m_loadedLevelFilesLabel->setText(QString("Loaded RE files: %1").arg(count));
    }

    codds::ReverseLayoutSettings ReverseWorkbenchPanel::settings() const
    {
        codds::ReverseLayoutSettings result;
        result.structSize = static_cast<std::uint32_t>(m_structSizeCombo->currentData().toUInt());
        result.coordinateFormat = static_cast<codds::CoordinateFormat>(m_coordinateFormatCombo->currentData().toInt());
        result.startOffset = static_cast<std::uint32_t>(m_startOffsetSpin->value());
        result.xOffset = static_cast<std::uint32_t>(m_xOffsetSpin->value());
        result.yOffset = static_cast<std::uint32_t>(m_yOffsetSpin->value());
        result.zOffset = static_cast<std::uint32_t>(m_zOffsetSpin->value());
        result.maxRecords = static_cast<std::uint32_t>(m_maxRecordsSpin->value());
        result.pointPreviewEnabled = m_pointPreviewCheck->isChecked();
        result.normalizeScale = m_normalizeScaleCheck->isChecked();
        result.drawGrid = m_drawGridCheck->isChecked();
        result.drawStrideLines = m_drawStrideLinesCheck->isChecked();
        result.scanEntireLevelFolder = m_scanEntireLevelCheck->isChecked();
        result.spawnObjectCubes = m_spawnObjectCubesCheck->isChecked();
        result.renderResolvedModels = m_renderResolvedModelsCheck->isChecked();
        result.objectsLpVariableRecordMode = m_objectsLpVariableRecordModeCheck->isChecked();
        result.recordSizeOffset = static_cast<std::uint32_t>(m_recordSizeOffsetSpin->value());
        result.modelIndexOffset = static_cast<std::uint32_t>(m_modelIndexOffsetSpin->value());
        result.objectCubeSize = static_cast<float>(m_objectCubeSizeSpin->value());
        result.scaleMultiplier = static_cast<float>(m_scaleMultiplierSpin->value());
        return result;
    }

    std::filesystem::path ReverseWorkbenchPanel::currentFile() const
    {
        return m_currentFile;
    }

    void ReverseWorkbenchPanel::setXOffset(int offset)
    {
        m_xOffsetSpin->setValue(offset);
    }

    void ReverseWorkbenchPanel::setYOffset(int offset)
    {
        m_yOffsetSpin->setValue(offset);
    }

    void ReverseWorkbenchPanel::setZOffset(int offset)
    {
        m_zOffsetSpin->setValue(offset);
    }

    void ReverseWorkbenchPanel::setCoordinateOffsets(int xOffset, int yOffset, int zOffset)
    {
        m_xOffsetSpin->setValue(xOffset);
        m_yOffsetSpin->setValue(yOffset);
        m_zOffsetSpin->setValue(zOffset);
    }

    void ReverseWorkbenchPanel::emitSettingsChanged()
    {
        emit settingsChanged();
    }
}
