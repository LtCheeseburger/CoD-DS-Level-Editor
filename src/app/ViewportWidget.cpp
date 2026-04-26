#include "ViewportWidget.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>
#include <functional>

namespace app
{
    namespace
    {
        constexpr float kPi = 3.14159265358979323846f;

        float radians(float degrees)
        {
            return degrees * kPi / 180.0f;
        }

        float clampPitch(float pitch)
        {
            return std::clamp(pitch, -89.0f, 89.0f);
        }
    }

    ViewportWidget::ViewportWidget(QWidget* parent)
        : QOpenGLWidget(parent)
    {
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);

        m_frameTimer.start();
        m_cameraTimer.setInterval(16);
        connect(&m_cameraTimer, &QTimer::timeout, this, &ViewportWidget::tickCamera);
        m_cameraTimer.start();
    }

    void ViewportWidget::setPointCloud(std::vector<codds::ReversePoint> points, const codds::ReverseLayoutSettings& settings)
    {
        m_points = std::move(points);
        m_objects.clear();
        m_viewSettings = settings;
        m_pointCloudEnabled = settings.pointPreviewEnabled && !m_points.empty();
        m_objectCubesEnabled = false;
        recomputePointCloudBounds();
        update();
    }

    void ViewportWidget::setObjectCubes(std::vector<codds::ReverseObjectInstance> objects, const codds::ReverseLayoutSettings& settings)
    {
        m_objects = std::move(objects);
        m_points.clear();
        m_viewSettings = settings;
        m_objectCubesEnabled = settings.pointPreviewEnabled && settings.spawnObjectCubes && !m_objects.empty();
        m_pointCloudEnabled = false;
        recomputePointCloudBounds();
        update();
    }

    void ViewportWidget::clearPointCloud()
    {
        m_points.clear();
        m_objects.clear();
        m_pointCloudEnabled = false;
        m_objectCubesEnabled = false;
        recomputePointCloudBounds();
        update();
    }

    void ViewportWidget::initializeGL()
    {
        m_renderer.initialize();
    }

    void ViewportWidget::resizeGL(int width, int height)
    {
        m_renderer.resize(width, height);
    }

    void ViewportWidget::paintGL()
    {
        m_renderer.renderFrame();
        drawPointCloudOverlay();
    }

    void ViewportWidget::keyPressEvent(QKeyEvent* event)
    {
        if (!event->isAutoRepeat())
            m_pressedKeys.insert(event->key());

        QOpenGLWidget::keyPressEvent(event);
    }

    void ViewportWidget::keyReleaseEvent(QKeyEvent* event)
    {
        if (!event->isAutoRepeat())
            m_pressedKeys.remove(event->key());

        QOpenGLWidget::keyReleaseEvent(event);
    }

    void ViewportWidget::mousePressEvent(QMouseEvent* event)
    {
        if (event->button() == Qt::RightButton)
        {
            m_mouseLookActive = true;
            m_lastMousePos = event->pos();
            setCursor(Qt::BlankCursor);
            setFocus();
        }

        QOpenGLWidget::mousePressEvent(event);
    }

    void ViewportWidget::mouseReleaseEvent(QMouseEvent* event)
    {
        if (event->button() == Qt::RightButton)
        {
            m_mouseLookActive = false;
            unsetCursor();
        }

        QOpenGLWidget::mouseReleaseEvent(event);
    }

    void ViewportWidget::mouseMoveEvent(QMouseEvent* event)
    {
        if (m_mouseLookActive)
        {
            const QPoint delta = event->pos() - m_lastMousePos;
            m_lastMousePos = event->pos();

            m_yawDegrees += static_cast<float>(delta.x()) * m_mouseSensitivity;
            m_pitchDegrees -= static_cast<float>(delta.y()) * m_mouseSensitivity;
            m_pitchDegrees = clampPitch(m_pitchDegrees);
            update();
        }

        QOpenGLWidget::mouseMoveEvent(event);
    }

    void ViewportWidget::wheelEvent(QWheelEvent* event)
    {
        const float direction = event->angleDelta().y() >= 0 ? 1.0f : -1.0f;
        m_cameraPosition += forwardVector() * direction * 0.75f;
        update();
        QOpenGLWidget::wheelEvent(event);
    }

    void ViewportWidget::tickCamera()
    {
        const float dt = std::max(0.001f, static_cast<float>(m_frameTimer.restart()) / 1000.0f);
        QVector3D move(0.0f, 0.0f, 0.0f);

        if (m_pressedKeys.contains(Qt::Key_W)) move += forwardVector();
        if (m_pressedKeys.contains(Qt::Key_S)) move -= forwardVector();
        if (m_pressedKeys.contains(Qt::Key_D)) move += rightVector();
        if (m_pressedKeys.contains(Qt::Key_A)) move -= rightVector();
        if (m_pressedKeys.contains(Qt::Key_E) || m_pressedKeys.contains(Qt::Key_Space)) move += QVector3D(0.0f, 1.0f, 0.0f);
        if (m_pressedKeys.contains(Qt::Key_Q) || m_pressedKeys.contains(Qt::Key_Control)) move -= QVector3D(0.0f, 1.0f, 0.0f);

        if (!move.isNull())
        {
            move.normalize();
            const float speed = m_pressedKeys.contains(Qt::Key_Shift) ? m_moveSpeed * 3.0f : m_moveSpeed;
            m_cameraPosition += move * speed * dt;
            update();
        }
    }

    QVector3D ViewportWidget::forwardVector() const
    {
        const float yaw = radians(m_yawDegrees);
        const float pitch = radians(m_pitchDegrees);

        QVector3D forward(
            std::cos(yaw) * std::cos(pitch),
            std::sin(pitch),
            std::sin(yaw) * std::cos(pitch)
        );
        forward.normalize();
        return forward;
    }

    QVector3D ViewportWidget::rightVector() const
    {
        QVector3D right = QVector3D::crossProduct(forwardVector(), QVector3D(0.0f, 1.0f, 0.0f));
        if (!right.isNull())
            right.normalize();
        return right;
    }

    QVector3D ViewportWidget::upVector() const
    {
        QVector3D up = QVector3D::crossProduct(rightVector(), forwardVector());
        if (!up.isNull())
            up.normalize();
        return up;
    }

    QVector3D ViewportWidget::normalizedPointPosition(const codds::ReversePoint& point) const
    {
        const QVector3D raw(point.x, point.y, point.z);
        const float userScale = std::max(0.0001f, m_viewSettings.scaleMultiplier);

        if (m_viewSettings.normalizeScale)
            return (raw - m_pointCenter) * m_pointScale * userScale;

        return raw * userScale;
    }

    QVector3D ViewportWidget::normalizedObjectPosition(const codds::ReverseObjectInstance& object) const
    {
        const QVector3D raw(object.x, object.y, object.z);
        const float userScale = std::max(0.0001f, m_viewSettings.scaleMultiplier);

        if (m_viewSettings.normalizeScale)
            return (raw - m_pointCenter) * m_pointScale * userScale;

        return raw * userScale;
    }

    bool ViewportWidget::projectPoint(const QVector3D& world, QPointF& outScreen, float& outDepth) const
    {
        const QVector3D relative = world - m_cameraPosition;
        const QVector3D forward = forwardVector();
        const QVector3D right = rightVector();
        const QVector3D up = upVector();

        const float depth = QVector3D::dotProduct(relative, forward);
        if (depth <= 0.05f)
            return false;

        const float xCamera = QVector3D::dotProduct(relative, right);
        const float yCamera = QVector3D::dotProduct(relative, up);
        const float aspect = height() > 0 ? static_cast<float>(width()) / static_cast<float>(height()) : 1.0f;
        const float fovRadians = radians(65.0f);
        const float focal = 1.0f / std::tan(fovRadians * 0.5f);

        const float ndcX = (xCamera * focal / aspect) / depth;
        const float ndcY = (yCamera * focal) / depth;

        if (std::abs(ndcX) > 2.0f || std::abs(ndcY) > 2.0f)
            return false;

        outScreen = QPointF(
            (ndcX * 0.5f + 0.5f) * static_cast<float>(width()),
            (0.5f - ndcY * 0.5f) * static_cast<float>(height())
        );
        outDepth = depth;
        return true;
    }

    void ViewportWidget::recomputePointCloudBounds()
    {
        if (m_points.empty() && m_objects.empty())
        {
            m_pointMin = QVector3D(0.0f, 0.0f, 0.0f);
            m_pointMax = QVector3D(0.0f, 0.0f, 0.0f);
            m_pointCenter = QVector3D(0.0f, 0.0f, 0.0f);
            m_pointScale = 1.0f;
            return;
        }

        QVector3D minPoint(
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()
        );
        QVector3D maxPoint(
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest()
        );

        for (const auto& point : m_points)
        {
            const QVector3D p(point.x, point.y, point.z);
            minPoint.setX(std::min(minPoint.x(), p.x()));
            minPoint.setY(std::min(minPoint.y(), p.y()));
            minPoint.setZ(std::min(minPoint.z(), p.z()));
            maxPoint.setX(std::max(maxPoint.x(), p.x()));
            maxPoint.setY(std::max(maxPoint.y(), p.y()));
            maxPoint.setZ(std::max(maxPoint.z(), p.z()));
        }

        for (const auto& object : m_objects)
        {
            const QVector3D p(object.x, object.y, object.z);
            minPoint.setX(std::min(minPoint.x(), p.x()));
            minPoint.setY(std::min(minPoint.y(), p.y()));
            minPoint.setZ(std::min(minPoint.z(), p.z()));
            maxPoint.setX(std::max(maxPoint.x(), p.x()));
            maxPoint.setY(std::max(maxPoint.y(), p.y()));
            maxPoint.setZ(std::max(maxPoint.z(), p.z()));
        }

        m_pointMin = minPoint;
        m_pointMax = maxPoint;
        m_pointCenter = (minPoint + maxPoint) * 0.5f;

        const QVector3D extent = maxPoint - minPoint;
        const float largestAxis = std::max({extent.x(), extent.y(), extent.z(), 0.001f});
        m_pointScale = 8.0f / largestAxis;
    }

    void ViewportWidget::drawWorldLine(QPainter& painter, const QVector3D& a, const QVector3D& b, const QColor& color, int width)
    {
        QPointF aScreen;
        QPointF bScreen;
        float aDepth = 0.0f;
        float bDepth = 0.0f;

        if (!projectPoint(a, aScreen, aDepth) || !projectPoint(b, bScreen, bDepth))
            return;

        painter.setPen(QPen(color, width));
        painter.drawLine(aScreen, bScreen);
    }

    void ViewportWidget::drawGridAndAxes(QPainter& painter)
    {
        if (!m_viewSettings.drawGrid)
            return;

        const float scale = std::max(0.0001f, m_viewSettings.scaleMultiplier);
        const bool normalized = m_viewSettings.normalizeScale;
        const float gridHalf = normalized ? 10.0f : 100.0f * scale;
        const float step = normalized ? 1.0f : 10.0f * scale;

        for (float v = -gridHalf; v <= gridHalf + 0.001f; v += step)
        {
            const QColor major = std::abs(v) < 0.001f ? QColor(90, 90, 90) : QColor(38, 38, 38);
            drawWorldLine(painter, QVector3D(-gridHalf, 0.0f, v), QVector3D(gridHalf, 0.0f, v), major, 1);
            drawWorldLine(painter, QVector3D(v, 0.0f, -gridHalf), QVector3D(v, 0.0f, gridHalf), major, 1);
        }

        drawWorldLine(painter, QVector3D(-gridHalf, 0.0f, 0.0f), QVector3D(gridHalf, 0.0f, 0.0f), QColor(210, 80, 80), 2);
        drawWorldLine(painter, QVector3D(0.0f, -gridHalf, 0.0f), QVector3D(0.0f, gridHalf, 0.0f), QColor(80, 210, 80), 2);
        drawWorldLine(painter, QVector3D(0.0f, 0.0f, -gridHalf), QVector3D(0.0f, 0.0f, gridHalf), QColor(80, 140, 230), 2);
    }

    void ViewportWidget::drawWireCube(QPainter& painter, const QVector3D& center, float halfSize, const QColor& color)
    {
        const QVector3D v[8] = {
            center + QVector3D(-halfSize, -halfSize, -halfSize),
            center + QVector3D( halfSize, -halfSize, -halfSize),
            center + QVector3D( halfSize,  halfSize, -halfSize),
            center + QVector3D(-halfSize,  halfSize, -halfSize),
            center + QVector3D(-halfSize, -halfSize,  halfSize),
            center + QVector3D( halfSize, -halfSize,  halfSize),
            center + QVector3D( halfSize,  halfSize,  halfSize),
            center + QVector3D(-halfSize,  halfSize,  halfSize)
        };

        const int edges[12][2] = {
            {0,1},{1,2},{2,3},{3,0},
            {4,5},{5,6},{6,7},{7,4},
            {0,4},{1,5},{2,6},{3,7}
        };

        for (const auto& edge : edges)
            drawWorldLine(painter, v[edge[0]], v[edge[1]], color, 1);
    }


    QVector3D ViewportWidget::modelProxyHalfExtents(const codds::ReverseObjectInstance& object) const
    {
        // Until the full NSBMD display-list decoder is in the renderer, resolved
        // model instances are shown as stable per-model wire proxies. The shape is
        // deterministic per NSBMD name so repeated object IDs are visually grouped.
        const float base = std::max(0.02f, m_viewSettings.objectCubeSize);
        std::size_t h = object.modelName.empty()
            ? static_cast<std::size_t>(object.modelIndex)
            : std::hash<std::string>{}(object.modelName);

        const float sx = base * (0.75f + static_cast<float>((h >> 0) & 0x3) * 0.25f);
        const float sy = base * (0.50f + static_cast<float>((h >> 4) & 0x3) * 0.20f);
        const float sz = base * (0.75f + static_cast<float>((h >> 8) & 0x3) * 0.25f);

        return QVector3D(sx, sy, sz);
    }

    const nitro::DecodedNsbmdMesh* ViewportWidget::decodedMeshForObject(const codds::ReverseObjectInstance& object)
    {
        if (!object.modelResolved || object.modelPath.empty())
            return nullptr;

        const std::string key = object.modelPath.string();
        auto it = m_meshCache.find(key);
        if (it == m_meshCache.end())
        {
            auto decoded = nitro::NsbmdGeometryDecoder::decodeWireframeMesh(object.modelPath);
            it = m_meshCache.emplace(key, std::move(decoded)).first;
        }

        if (!it->second.valid || it->second.vertices.empty() || it->second.edges.empty())
            return nullptr;

        return &it->second;
    }

    bool ViewportWidget::drawDecodedModelMesh(QPainter& painter, const codds::ReverseObjectInstance& object, const QVector3D& center)
    {
        const auto* mesh = decodedMeshForObject(object);
        if (!mesh)
            return false;

        const QVector3D minB = mesh->minBounds;
        const QVector3D maxB = mesh->maxBounds;
        const QVector3D meshCenter = (minB + maxB) * 0.5f;
        const QVector3D extent = maxB - minB;
        const float largest = std::max({std::abs(extent.x()), std::abs(extent.y()), std::abs(extent.z()), 0.0001f});

        const float targetSize = std::max(0.05f, m_viewSettings.objectCubeSize * 2.5f);
        const float scale = targetSize / largest;

        int drawn = 0;
        const int maxEdges = 2000;
        for (const auto& edge : mesh->edges)
        {
            if (edge.a >= mesh->vertices.size() || edge.b >= mesh->vertices.size())
                continue;

            const QVector3D aLocal = (mesh->vertices[edge.a] - meshCenter) * scale;
            const QVector3D bLocal = (mesh->vertices[edge.b] - meshCenter) * scale;
            drawWorldLine(painter, center + aLocal, center + bLocal, QColor(255, 220, 80), 1);

            if (++drawn >= maxEdges)
                break;
        }

        return drawn > 0;
    }

    void ViewportWidget::drawModelProxy(QPainter& painter, const codds::ReverseObjectInstance& object, const QVector3D& center)
    {
        const QVector3D half = modelProxyHalfExtents(object);
        const QVector3D v[8] = {
            center + QVector3D(-half.x(), -half.y(), -half.z()),
            center + QVector3D( half.x(), -half.y(), -half.z()),
            center + QVector3D( half.x(),  half.y(), -half.z()),
            center + QVector3D(-half.x(),  half.y(), -half.z()),
            center + QVector3D(-half.x(), -half.y(),  half.z()),
            center + QVector3D( half.x(), -half.y(),  half.z()),
            center + QVector3D( half.x(),  half.y(),  half.z()),
            center + QVector3D(-half.x(),  half.y(),  half.z())
        };

        const int edges[12][2] = {
            {0,1},{1,2},{2,3},{3,0},
            {4,5},{5,6},{6,7},{7,4},
            {0,4},{1,5},{2,6},{3,7}
        };

        const QColor body = object.modelResolved ? QColor(255, 210, 90) : QColor(190, 190, 190);
        for (const auto& edge : edges)
            drawWorldLine(painter, v[edge[0]], v[edge[1]], body, object.modelResolved ? 2 : 1);

        // Add a small origin stem/cross so model-instance proxies read as placed assets,
        // not just generic point cubes.
        drawWorldLine(painter, center + QVector3D(-half.x(), 0.0f, 0.0f), center + QVector3D(half.x(), 0.0f, 0.0f), QColor(255, 235, 160), 1);
        drawWorldLine(painter, center + QVector3D(0.0f, 0.0f, -half.z()), center + QVector3D(0.0f, 0.0f, half.z()), QColor(255, 235, 160), 1);
        drawWorldLine(painter, center, center + QVector3D(0.0f, half.y() * 1.75f, 0.0f), QColor(255, 235, 160), 1);
    }

    void ViewportWidget::drawPointCloudOverlay()
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);

        drawGridAndAxes(painter);

        painter.setPen(QPen(QColor(70, 70, 70), 1));
        painter.drawText(12, 22, "Viewport: RMB + mouse = look | WASD = move | Q/E = down/up | Shift = fast");

        if (!m_pointCloudEnabled && !m_objectCubesEnabled)
        {
            painter.setPen(QPen(QColor(110, 110, 110), 1));
            painter.drawText(12, 42, "Point/object preview disabled");
            return;
        }

        if (m_objectCubesEnabled)
        {
            const int maxDrawCount = 10000;
            const int drawCount = std::min(static_cast<int>(m_objects.size()), maxDrawCount);
            int visibleCount = 0;
            int resolvedCount = 0;
            const float halfSize = std::max(0.01f, m_viewSettings.objectCubeSize * 0.5f);

            for (int i = 0; i < drawCount; ++i)
            {
                const auto& object = m_objects[static_cast<std::size_t>(i)];
                const QVector3D world = normalizedObjectPosition(object);
                QPointF screen;
                float depth = 0.0f;
                const bool visible = projectPoint(world, screen, depth);
                if (object.modelResolved)
                    ++resolvedCount;

                if (m_viewSettings.renderResolvedModels && object.modelResolved)
                {
                    if (!drawDecodedModelMesh(painter, object, world))
                        drawModelProxy(painter, object, world);
                }
                else
                {
                    drawWireCube(painter, world, halfSize, object.modelResolved ? QColor(255, 210, 90) : QColor(190, 190, 190));
                }

                if (visible && i < 250)
                {
                    painter.setPen(QPen(QColor(230, 230, 230), 1));
                    const QString label = (m_viewSettings.renderResolvedModels && object.modelResolved && !object.modelName.empty())
                        ? QString("%1: %2").arg(object.modelIndex).arg(QString::fromStdString(object.modelName))
                        : QString("%1").arg(object.modelIndex);
                    painter.drawText(screen + QPointF(4.0, -4.0), label);
                    ++visibleCount;
                }
            }

            painter.setPen(QPen(QColor(210, 210, 210), 1));
            painter.drawText(12, 42, QString("%5: %1 scanned | %2 labeled | %3 model hits | index offset %4")
                                      .arg(static_cast<int>(m_objects.size()))
                                      .arg(visibleCount)
                                      .arg(resolvedCount)
                                      .arg(m_viewSettings.modelIndexOffset)
                                      .arg(m_viewSettings.renderResolvedModels ? "NSBMD model instances" : "Object cubes"));
            painter.drawText(12, 62, QString("Raw bounds X[%1, %2] Y[%3, %4] Z[%5, %6]")
                                      .arg(m_pointMin.x(), 0, 'f', 3)
                                      .arg(m_pointMax.x(), 0, 'f', 3)
                                      .arg(m_pointMin.y(), 0, 'f', 3)
                                      .arg(m_pointMax.y(), 0, 'f', 3)
                                      .arg(m_pointMin.z(), 0, 'f', 3)
                                      .arg(m_pointMax.z(), 0, 'f', 3));
            return;
        }

        const int maxDrawCount = 50000;
        const int drawCount = std::min(static_cast<int>(m_points.size()), maxDrawCount);

        int visibleCount = 0;

        if (m_viewSettings.drawStrideLines)
        {
            for (int i = 1; i < drawCount; ++i)
            {
                const auto& prev = m_points[static_cast<std::size_t>(i - 1)];
                const auto& curr = m_points[static_cast<std::size_t>(i)];

                // Only connect points that came from adjacent records in the same scanned stream.
                // Large jumps usually mean invalid layout, but seeing them is still useful.
                if (curr.recordIndex == prev.recordIndex + 1)
                {
                    drawWorldLine(
                        painter,
                        normalizedPointPosition(prev),
                        normalizedPointPosition(curr),
                        QColor(120, 120, 90),
                        1
                    );
                }
            }
        }

        painter.setPen(QPen(QColor(180, 220, 255), 2));
        for (int i = 0; i < drawCount; ++i)
        {
            const QVector3D world = normalizedPointPosition(m_points[static_cast<std::size_t>(i)]);
            QPointF screen;
            float depth = 0.0f;
            if (!projectPoint(world, screen, depth))
                continue;

            painter.drawPoint(screen);
            ++visibleCount;
        }

        painter.setPen(QPen(QColor(210, 210, 210), 1));
        painter.drawText(12, 42, QString("Point cloud: %1 scanned | %2 visible | %3 | stride %4")
                                  .arg(static_cast<int>(m_points.size()))
                                  .arg(visibleCount)
                                  .arg(m_viewSettings.normalizeScale ? "normalized" : "raw scale")
                                  .arg(m_viewSettings.drawStrideLines ? "on" : "off"));
        painter.drawText(12, 62, QString("Raw bounds X[%1, %2] Y[%3, %4] Z[%5, %6]")
                                  .arg(m_pointMin.x(), 0, 'f', 3)
                                  .arg(m_pointMax.x(), 0, 'f', 3)
                                  .arg(m_pointMin.y(), 0, 'f', 3)
                                  .arg(m_pointMax.y(), 0, 'f', 3)
                                  .arg(m_pointMin.z(), 0, 'f', 3)
                                  .arg(m_pointMax.z(), 0, 'f', 3));
    }
}
