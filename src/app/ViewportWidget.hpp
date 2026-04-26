#pragma once

#include <QElapsedTimer>
#include <QOpenGLWidget>
#include <QPoint>
#include <QSet>
#include <QTimer>
#include <QVector3D>

#include <string>
#include <unordered_map>
#include <vector>

#include "codds/ReverseCandidate.hpp"
#include "nitro/NsbmdGeometryDecoder.hpp"
#include "render/Renderer.hpp"

namespace app
{
    class ViewportWidget final : public QOpenGLWidget
    {
        Q_OBJECT

    public:
        explicit ViewportWidget(QWidget* parent = nullptr);

        void setPointCloud(std::vector<codds::ReversePoint> points, const codds::ReverseLayoutSettings& settings);
        void setObjectCubes(std::vector<codds::ReverseObjectInstance> objects, const codds::ReverseLayoutSettings& settings);
        void clearPointCloud();

    protected:
        void initializeGL() override;
        void resizeGL(int width, int height) override;
        void paintGL() override;

        void keyPressEvent(QKeyEvent* event) override;
        void keyReleaseEvent(QKeyEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void wheelEvent(QWheelEvent* event) override;

    private slots:
        void tickCamera();

    private:
        void drawPointCloudOverlay();
        void drawGridAndAxes(QPainter& painter);
        void drawWorldLine(QPainter& painter, const QVector3D& a, const QVector3D& b, const QColor& color, int width = 1);
        bool projectPoint(const QVector3D& world, QPointF& outScreen, float& outDepth) const;
        QVector3D forwardVector() const;
        QVector3D rightVector() const;
        QVector3D upVector() const;
        QVector3D normalizedPointPosition(const codds::ReversePoint& point) const;
        QVector3D normalizedObjectPosition(const codds::ReverseObjectInstance& object) const;
        void drawWireCube(QPainter& painter, const QVector3D& center, float halfSize, const QColor& color);
        void drawModelProxy(QPainter& painter, const codds::ReverseObjectInstance& object, const QVector3D& center);
        bool drawDecodedModelMesh(QPainter& painter, const codds::ReverseObjectInstance& object, const QVector3D& center);
        const nitro::DecodedNsbmdMesh* decodedMeshForObject(const codds::ReverseObjectInstance& object);
        QVector3D modelProxyHalfExtents(const codds::ReverseObjectInstance& object) const;
        void recomputePointCloudBounds();

        render::Renderer m_renderer;

        std::vector<codds::ReversePoint> m_points;
        std::vector<codds::ReverseObjectInstance> m_objects;
        std::unordered_map<std::string, nitro::DecodedNsbmdMesh> m_meshCache;
        bool m_pointCloudEnabled = false;
        bool m_objectCubesEnabled = false;
        codds::ReverseLayoutSettings m_viewSettings;
        QVector3D m_pointMin {0.0f, 0.0f, 0.0f};
        QVector3D m_pointMax {0.0f, 0.0f, 0.0f};
        QVector3D m_pointCenter {0.0f, 0.0f, 0.0f};
        float m_pointScale = 1.0f;

        QVector3D m_cameraPosition {0.0f, 1.5f, 6.0f};
        float m_yawDegrees = -90.0f;
        float m_pitchDegrees = -10.0f;
        float m_moveSpeed = 5.0f;
        float m_mouseSensitivity = 0.15f;
        bool m_mouseLookActive = false;
        QPoint m_lastMousePos;
        QSet<int> m_pressedKeys;
        QTimer m_cameraTimer;
        QElapsedTimer m_frameTimer;
    };
}
