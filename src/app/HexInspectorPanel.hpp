#pragma once

#include <QPlainTextEdit>
#include <QWidget>

#include <filesystem>

namespace app
{
    class HexInspectorPanel final : public QWidget
    {
        Q_OBJECT

    public:
        explicit HexInspectorPanel(QWidget* parent = nullptr);

        void loadFile(const std::filesystem::path& path);
        void clear();

    private:
        QPlainTextEdit* m_text = nullptr;
    };
}
