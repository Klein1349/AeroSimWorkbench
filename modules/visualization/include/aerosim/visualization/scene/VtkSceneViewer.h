#pragma once
#include <QWidget>
#include <QColor>
#include <QVariantMap>
#include <memory>
#include "aerosim/visualization/features/BoundarySurfaceExtractor.h"
namespace aerosim { namespace visualization {
class VtkSceneViewer final : public QWidget {
    Q_OBJECT
public:
    explicit VtkSceneViewer(QWidget* parent=nullptr);
    ~VtkSceneViewer() override;
    void addBoundary(const QString& id,std::shared_ptr<const BoundaryMesh> mesh);
    int objectCount() const;
    qint64 displayedFaces(const QString& id) const;
    QVariantMap objectStyle(const QString& id) const;
public slots:
    void removeObject(const QString& id);
    void setObjectStyle(const QString& id,const QVariantMap& style);
    void setBackground(const QColor& bottom,const QColor& top,bool gradient);
    void resetCamera();
    void setCameraOrientation(const QString& command);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    void render();
};
} }
