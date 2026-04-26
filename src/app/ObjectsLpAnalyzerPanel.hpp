#pragma once

#include <QLabel>
#include <QPoint>
#include <QTableWidget>
#include <QWidget>

#include <cstdint>
#include <filesystem>
#include <vector>

#include "codds/ModelLibrary.hpp"
#include "codds/ReverseCandidate.hpp"

namespace app
{
    class ObjectsLpAnalyzerPanel final : public QWidget
    {
        Q_OBJECT

    public:
        explicit ObjectsLpAnalyzerPanel(QWidget* parent = nullptr);

        void setModelLibrary(const codds::ModelLibrary* modelLibrary);
        void setSettings(const codds::ReverseLayoutSettings& settings);
        void loadFile(const std::filesystem::path& path);
        void clear();

    signals:
        void requestSetXOffset(int offset);
        void requestSetYOffset(int offset);
        void requestSetZOffset(int offset);
        void requestSetCoordinateOffsets(int xOffset, int yOffset, int zOffset);

    private slots:
        void showTableContextMenu(const QPoint& position);

    private:
        struct CandidateRow
        {
            std::uint32_t index = 0;
            std::uint32_t offset = 0;
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            QString modelCandidate;
        };

        void rebuildTable();
        void rebuildSummary(const std::vector<CandidateRow>& rows, std::uint32_t displayedRows, std::uint32_t totalRecords);
        void markCoordinateColumns(std::uint32_t row, int column, QTableWidgetItem* item) const;
        int byteOffsetForColumn(int column) const;

        static std::vector<std::uint8_t> readAllBytes(const std::filesystem::path& path);
        static std::uint16_t readU16LE(const std::vector<std::uint8_t>& data, std::size_t offset);
        static std::uint32_t readU32LE(const std::vector<std::uint8_t>& data, std::size_t offset);
        static float readCoordinate(const std::vector<std::uint8_t>& data, std::size_t offset, codds::CoordinateFormat format);
        static std::size_t coordinateSize(codds::CoordinateFormat format);
        static QString hexBytes(const std::vector<std::uint8_t>& data, std::size_t offset, std::size_t count);

        QString findModelCandidate(const std::vector<std::uint8_t>& data, std::size_t base, std::size_t structSize) const;

        QLabel* m_titleLabel = nullptr;
        QLabel* m_summaryLabel = nullptr;
        QLabel* m_hintLabel = nullptr;
        QTableWidget* m_table = nullptr;

        const codds::ModelLibrary* m_modelLibrary = nullptr;
        codds::ReverseLayoutSettings m_settings;
        std::filesystem::path m_currentPath;
        std::vector<std::uint8_t> m_data;
    };
}
