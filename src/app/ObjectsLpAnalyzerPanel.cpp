#include "ObjectsLpAnalyzerPanel.hpp"

#include <QBrush>
#include <QColor>
#include <QHeaderView>
#include <QMenu>
#include <QStringList>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <stdexcept>

namespace app
{
    namespace
    {
        constexpr std::uint32_t MaxDisplayedRows = 1024;
        constexpr int SizeColumn = 2;
        constexpr int ObjectIdColumn = 3;
        constexpr int RawBytesColumn = 4;
        constexpr int FirstU32Column = 5;
        constexpr int U32ColumnCount = 9;
        constexpr int S16Column = FirstU32Column + U32ColumnCount;
        constexpr int XColumn = S16Column + 1;
        constexpr int YColumn = S16Column + 2;
        constexpr int ZColumn = S16Column + 3;
        constexpr int ModelColumn = S16Column + 4;

        QTableWidgetItem* makeReadOnlyItem(const QString& text)
        {
            auto* item = new QTableWidgetItem(text);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            return item;
        }

        QString fixedText(float value)
        {
            if (!std::isfinite(value))
                return "invalid";
            return QString::number(value, 'f', 4);
        }

        bool saneCoordinate(float value)
        {
            return std::isfinite(value) && std::abs(value) <= 100000.0f;
        }

        bool plausibleRecordSize(std::uint16_t size, std::size_t remaining)
        {
            return size >= 8 && size <= 4096 && size <= remaining;
        }
    }

    ObjectsLpAnalyzerPanel::ObjectsLpAnalyzerPanel(QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(6, 6, 6, 6);
        layout->setSpacing(4);

        m_titleLabel = new QLabel("No Objects.LP selected", this);
        m_summaryLabel = new QLabel("Select Objects.LP to inspect records.", this);
        m_summaryLabel->setWordWrap(true);
        m_hintLabel = new QLabel("Tip: enable Objects.LP variable record mode, then right-click a u32 column to assign X, Y, or Z.", this);
        m_hintLabel->setWordWrap(true);

        m_table = new QTableWidget(this);
        m_table->setColumnCount(ModelColumn + 1);

        QStringList headers;
        headers << "Index" << "Offset" << "Size" << "Object ID" << "Raw bytes";
        for (int i = 0; i < U32ColumnCount; ++i)
            headers << QString("u32[%1]").arg(i);
        headers << "s16[0..11]" << "X" << "Y" << "Z" << "Model candidate";
        m_table->setHorizontalHeaderLabels(headers);

        m_table->verticalHeader()->setVisible(false);
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
        m_table->setAlternatingRowColors(true);
        m_table->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_table, &QTableWidget::customContextMenuRequested, this, &ObjectsLpAnalyzerPanel::showTableContextMenu);

        layout->addWidget(m_titleLabel);
        layout->addWidget(m_summaryLabel);
        layout->addWidget(m_hintLabel);
        layout->addWidget(m_table, 1);
    }

    void ObjectsLpAnalyzerPanel::setModelLibrary(const codds::ModelLibrary* modelLibrary)
    {
        m_modelLibrary = modelLibrary;
        rebuildTable();
    }

    void ObjectsLpAnalyzerPanel::setSettings(const codds::ReverseLayoutSettings& settings)
    {
        m_settings = settings;
        rebuildTable();
    }

    void ObjectsLpAnalyzerPanel::loadFile(const std::filesystem::path& path)
    {
        m_currentPath = path;

        try
        {
            m_data = readAllBytes(path);
            m_titleLabel->setText(QString("Objects.LP Analyzer: %1").arg(QString::fromStdString(path.filename().string())));
        }
        catch (const std::exception& e)
        {
            m_data.clear();
            m_titleLabel->setText("Objects.LP Analyzer: load failed");
            m_summaryLabel->setText(QString("Failed to load file: %1").arg(e.what()));
        }

        rebuildTable();
    }

    void ObjectsLpAnalyzerPanel::clear()
    {
        m_currentPath.clear();
        m_data.clear();
        m_titleLabel->setText("No Objects.LP selected");
        m_summaryLabel->setText("Select Objects.LP to inspect records.");
        m_table->setRowCount(0);
    }

    void ObjectsLpAnalyzerPanel::showTableContextMenu(const QPoint& position)
    {
        const QModelIndex index = m_table->indexAt(position);
        if (!index.isValid())
            return;

        const int column = index.column();
        const int offset = byteOffsetForColumn(column);
        if (offset < 0)
            return;

        QMenu menu(this);
        menu.addAction(QString("Set byte offset %1 as X").arg(offset), this, [this, offset]() { emit requestSetXOffset(offset); });
        menu.addAction(QString("Set byte offset %1 as Y").arg(offset), this, [this, offset]() { emit requestSetYOffset(offset); });
        menu.addAction(QString("Set byte offset %1 as Z").arg(offset), this, [this, offset]() { emit requestSetZOffset(offset); });
        menu.addSeparator();
        menu.addAction("Try X/Y/Z from this column", this, [this, offset]() {
            emit requestSetCoordinateOffsets(offset, offset + 4, offset + 8);
        });
        menu.exec(m_table->viewport()->mapToGlobal(position));
    }

    int ObjectsLpAnalyzerPanel::byteOffsetForColumn(int column) const
    {
        if (column >= FirstU32Column && column < FirstU32Column + U32ColumnCount)
            return (column - FirstU32Column) * 4;
        return -1;
    }

    void ObjectsLpAnalyzerPanel::markCoordinateColumns(std::uint32_t, int column, QTableWidgetItem* item) const
    {
        const int offset = byteOffsetForColumn(column);
        if (offset < 0)
            return;

        if (static_cast<std::uint32_t>(offset) == m_settings.xOffset)
            item->setBackground(QBrush(QColor(90, 35, 35)));
        else if (static_cast<std::uint32_t>(offset) == m_settings.yOffset)
            item->setBackground(QBrush(QColor(35, 80, 35)));
        else if (static_cast<std::uint32_t>(offset) == m_settings.zOffset)
            item->setBackground(QBrush(QColor(35, 45, 90)));
    }

    void ObjectsLpAnalyzerPanel::rebuildTable()
    {
        if (!m_table)
            return;

        m_table->setRowCount(0);

        if (m_data.empty())
            return;

        const std::size_t componentSize = coordinateSize(m_settings.coordinateFormat);
        const std::size_t maxFieldEnd = std::max({
            static_cast<std::size_t>(m_settings.xOffset) + componentSize,
            static_cast<std::size_t>(m_settings.yOffset) + componentSize,
            static_cast<std::size_t>(m_settings.zOffset) + componentSize,
            static_cast<std::size_t>(m_settings.modelIndexOffset) + 2u,
            static_cast<std::size_t>(m_settings.recordSizeOffset) + 2u
        });

        if (m_settings.startOffset >= m_data.size())
        {
            m_summaryLabel->setText("Start offset is past end of file.");
            return;
        }

        std::vector<std::size_t> recordOffsets;
        std::vector<std::size_t> recordSizes;
        recordOffsets.reserve(MaxDisplayedRows);
        recordSizes.reserve(MaxDisplayedRows);

        std::size_t base = m_settings.startOffset;
        while (base < m_data.size() && recordOffsets.size() < MaxDisplayedRows && recordOffsets.size() < m_settings.maxRecords)
        {
            std::size_t recordSize = m_settings.structSize;
            if (m_settings.objectsLpVariableRecordMode)
            {
                const std::size_t sizeOff = base + m_settings.recordSizeOffset;
                if (sizeOff + 2 > m_data.size())
                    break;
                recordSize = readU16LE(m_data, sizeOff);
                if (!plausibleRecordSize(static_cast<std::uint16_t>(recordSize), m_data.size() - base))
                    break;
            }
            else if (recordSize == 0)
            {
                break;
            }

            recordOffsets.push_back(base);
            recordSizes.push_back(recordSize);
            base += recordSize;
        }

        m_table->setRowCount(static_cast<int>(recordOffsets.size()));
        std::vector<CandidateRow> summaryRows;
        summaryRows.reserve(recordOffsets.size());

        for (std::uint32_t row = 0; row < recordOffsets.size(); ++row)
        {
            const std::size_t recBase = recordOffsets[row];
            const std::size_t recSize = recordSizes[row];

            CandidateRow candidate;
            candidate.index = row;
            candidate.offset = static_cast<std::uint32_t>(recBase);
            if (maxFieldEnd <= recSize)
            {
                candidate.x = readCoordinate(m_data, recBase + m_settings.xOffset, m_settings.coordinateFormat);
                candidate.y = readCoordinate(m_data, recBase + m_settings.yOffset, m_settings.coordinateFormat);
                candidate.z = readCoordinate(m_data, recBase + m_settings.zOffset, m_settings.coordinateFormat);
                candidate.modelCandidate = findModelCandidate(m_data, recBase, recSize);
            }
            else
            {
                candidate.x = candidate.y = candidate.z = std::numeric_limits<float>::quiet_NaN();
            }
            summaryRows.push_back(candidate);

            const auto set = [&](int column, const QString& text)
            {
                auto* item = makeReadOnlyItem(text);
                markCoordinateColumns(row, column, item);
                m_table->setItem(static_cast<int>(row), column, item);
            };

            set(0, QString::number(row));
            set(1, QString("0x%1").arg(static_cast<qulonglong>(recBase), 8, 16, QChar('0')));
            set(SizeColumn, QString::number(static_cast<qulonglong>(recSize)));
            if (m_settings.modelIndexOffset + 2 <= recSize && recBase + m_settings.modelIndexOffset + 2 <= m_data.size())
                set(ObjectIdColumn, QString::number(readU16LE(m_data, recBase + m_settings.modelIndexOffset)));
            else
                set(ObjectIdColumn, "");
            set(RawBytesColumn, hexBytes(m_data, recBase, std::min<std::size_t>(recSize, 36)));

            for (int i = 0; i < U32ColumnCount; ++i)
            {
                const std::size_t rel = static_cast<std::size_t>(i) * 4u;
                const std::size_t off = recBase + rel;
                if (off + 4 <= m_data.size() && rel + 4u <= recSize)
                    set(FirstU32Column + i, QString::number(static_cast<std::int32_t>(readU32LE(m_data, off))));
                else
                    set(FirstU32Column + i, "");
            }

            QStringList s16Values;
            for (int i = 0; i < 12; ++i)
            {
                const std::size_t rel = static_cast<std::size_t>(i) * 2u;
                const std::size_t off = recBase + rel;
                if (off + 2 <= m_data.size() && rel + 2u <= recSize)
                    s16Values << QString::number(static_cast<std::int16_t>(readU16LE(m_data, off)));
            }
            set(S16Column, s16Values.join(", "));

            set(XColumn, fixedText(candidate.x));
            set(YColumn, fixedText(candidate.y));
            set(ZColumn, fixedText(candidate.z));
            set(ModelColumn, candidate.modelCandidate);
        }

        m_table->resizeColumnsToContents();
        rebuildSummary(summaryRows, static_cast<std::uint32_t>(recordOffsets.size()), static_cast<std::uint32_t>(recordOffsets.size()));
    }

    void ObjectsLpAnalyzerPanel::rebuildSummary(const std::vector<CandidateRow>& rows, std::uint32_t displayedRows, std::uint32_t totalRecords)
    {
        std::uint32_t finiteCount = 0;
        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float minZ = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();
        float maxZ = std::numeric_limits<float>::lowest();
        std::map<QString, int> modelCounts;

        for (const auto& row : rows)
        {
            if (saneCoordinate(row.x) && saneCoordinate(row.y) && saneCoordinate(row.z))
            {
                ++finiteCount;
                minX = std::min(minX, row.x); minY = std::min(minY, row.y); minZ = std::min(minZ, row.z);
                maxX = std::max(maxX, row.x); maxY = std::max(maxY, row.y); maxZ = std::max(maxZ, row.z);
            }

            if (!row.modelCandidate.isEmpty())
                ++modelCounts[row.modelCandidate];
        }

        QString bounds = "no sane XYZ values";
        if (finiteCount > 0)
        {
            bounds = QString("X[%1, %2] Y[%3, %4] Z[%5, %6]")
                .arg(minX, 0, 'f', 3).arg(maxX, 0, 'f', 3)
                .arg(minY, 0, 'f', 3).arg(maxY, 0, 'f', 3)
                .arg(minZ, 0, 'f', 3).arg(maxZ, 0, 'f', 3);
        }

        QString modelSummary = "no direct model-index candidates";
        if (!modelCounts.empty())
        {
            QStringList parts;
            int emitted = 0;
            for (const auto& [name, count] : modelCounts)
            {
                parts << QString("%1 x%2").arg(name).arg(count);
                if (++emitted >= 4)
                    break;
            }
            modelSummary = parts.join(" | ");
        }

        m_summaryLabel->setText(
            QString("%1 mode, start 0x%2, showing %3/%4 records. XYZ sane: %5. Bounds: %6. Model hits: %7")
                .arg(m_settings.objectsLpVariableRecordMode ? "Variable record" : QString("Fixed struct %1").arg(m_settings.structSize))
                .arg(m_settings.startOffset, 0, 16)
                .arg(displayedRows)
                .arg(totalRecords)
                .arg(finiteCount)
                .arg(bounds)
                .arg(modelSummary));
    }

    std::vector<std::uint8_t> ObjectsLpAnalyzerPanel::readAllBytes(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            throw std::runtime_error("Failed to open file: " + path.string());

        return std::vector<std::uint8_t>{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
    }

    std::uint16_t ObjectsLpAnalyzerPanel::readU16LE(const std::vector<std::uint8_t>& data, std::size_t offset)
    {
        return static_cast<std::uint16_t>(data[offset]) | static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[offset + 1]) << 8);
    }

    std::uint32_t ObjectsLpAnalyzerPanel::readU32LE(const std::vector<std::uint8_t>& data, std::size_t offset)
    {
        return static_cast<std::uint32_t>(data[offset]) |
               (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
               (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
               (static_cast<std::uint32_t>(data[offset + 3]) << 24);
    }

    float ObjectsLpAnalyzerPanel::readCoordinate(const std::vector<std::uint8_t>& data, std::size_t offset, codds::CoordinateFormat format)
    {
        switch (format)
        {
            case codds::CoordinateFormat::Float32:
            {
                const std::uint32_t raw = readU32LE(data, offset);
                float value = 0.0f;
                std::memcpy(&value, &raw, sizeof(float));
                return value;
            }
            case codds::CoordinateFormat::Int16Fixed4096:
                return static_cast<float>(static_cast<std::int16_t>(readU16LE(data, offset))) / 4096.0f;
            case codds::CoordinateFormat::Int32Fixed4096:
                return static_cast<float>(static_cast<std::int32_t>(readU32LE(data, offset))) / 4096.0f;
        }
        return 0.0f;
    }

    std::size_t ObjectsLpAnalyzerPanel::coordinateSize(codds::CoordinateFormat format)
    {
        return format == codds::CoordinateFormat::Int16Fixed4096 ? 2u : 4u;
    }

    QString ObjectsLpAnalyzerPanel::hexBytes(const std::vector<std::uint8_t>& data, std::size_t offset, std::size_t count)
    {
        QStringList parts;
        const std::size_t end = std::min(data.size(), offset + count);
        for (std::size_t i = offset; i < end; ++i)
            parts << QString("%1").arg(static_cast<unsigned>(data[i]), 2, 16, QChar('0'));
        return parts.join(' ');
    }

    QString ObjectsLpAnalyzerPanel::findModelCandidate(const std::vector<std::uint8_t>& data, std::size_t base, std::size_t structSize) const
    {
        if (!m_modelLibrary || m_modelLibrary->assets().empty())
            return {};

        const auto& assets = m_modelLibrary->assets();
        const std::size_t maxU16Fields = std::min<std::size_t>(structSize / 2u, 18u);
        for (std::size_t i = 0; i < maxU16Fields; ++i)
        {
            const std::size_t off = base + i * 2u;
            if (off + 2 > data.size())
                continue;

            const std::uint16_t value = readU16LE(data, off);
            if (value < assets.size())
                return QString("u16[%1]=%2 -> %3").arg(i).arg(value).arg(QString::fromStdString(assets[value].displayName));
            if (value > 0 && value - 1 < assets.size())
                return QString("u16[%1]=%2 -> %3 (1-based)").arg(i).arg(value).arg(QString::fromStdString(assets[value - 1].displayName));
        }

        return {};
    }
}
