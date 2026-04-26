#include "HexInspectorPanel.hpp"

#include <QFontDatabase>
#include <QVBoxLayout>

#include <cctype>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <vector>

namespace app
{
    namespace
    {
        std::vector<unsigned char> readBytes(const std::filesystem::path& path)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
                return {};

            return std::vector<unsigned char>{
                std::istreambuf_iterator<char>(file),
                std::istreambuf_iterator<char>()
            };
        }

        QString formatHexDump(const std::vector<unsigned char>& bytes)
        {
            std::ostringstream out;
            out << std::hex << std::setfill('0');

            for (std::size_t row = 0; row < bytes.size(); row += 16)
            {
                out << std::setw(8) << row << "  ";

                for (std::size_t i = 0; i < 16; ++i)
                {
                    if (row + i < bytes.size())
                        out << std::setw(2) << static_cast<int>(bytes[row + i]) << ' ';
                    else
                        out << "   ";

                    if (i == 7)
                        out << ' ';
                }

                out << " |";
                for (std::size_t i = 0; i < 16 && row + i < bytes.size(); ++i)
                {
                    const unsigned char c = bytes[row + i];
                    out << (std::isprint(c) ? static_cast<char>(c) : '.');
                }
                out << "|\n";
            }

            return QString::fromStdString(out.str());
        }
    }

    HexInspectorPanel::HexInspectorPanel(QWidget* parent)
        : QWidget(parent)
    {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);

        m_text = new QPlainTextEdit(this);
        m_text->setReadOnly(true);
        m_text->setLineWrapMode(QPlainTextEdit::NoWrap);
        m_text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        m_text->setPlainText("Select a binary level file to inspect.");

        layout->addWidget(m_text);
    }

    void HexInspectorPanel::loadFile(const std::filesystem::path& path)
    {
        const auto bytes = readBytes(path);
        m_text->setPlainText(formatHexDump(bytes));
    }

    void HexInspectorPanel::clear()
    {
        m_text->clear();
    }
}
