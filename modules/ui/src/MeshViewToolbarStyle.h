#pragma once

#include <QIconEngine>
#include <QPainter>
#include <QPixmap>
#include <QPolygonF>
#include <QToolBar>

namespace aerosim {
namespace meshToolbar {

enum class Glyph {
    Front, Back, Right, Left, Top, Bottom, Iso, ZoomIn, ZoomOut, Fit,
    Surface, Edges, Wire, Axes, Reset, Center, Background
};

// QIconEngine repaints these paths at the target size and screen resolution.
class Icon final : public QIconEngine {
public:
    explicit Icon(Glyph glyph) : glyph_(glyph) {}
    QIconEngine *clone() const override { return new Icon(glyph_); }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        QPixmap image(size);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        paint(&painter, image.rect(), mode, state);
        return image;
    }

    void paint(QPainter *p, const QRect &rect, QIcon::Mode mode, QIcon::State state) override
    {
        p->save();
        const qreal side = qMin(rect.width(), rect.height());
        p->translate(rect.x() + (rect.width() - side) / 2, rect.y() + (rect.height() - side) / 2);
        p->scale(side / 24, side / 24);
        p->setRenderHint(QPainter::Antialiasing);
        const bool disabled = mode == QIcon::Disabled;
        const QColor ink(disabled ? "#96a1af" : state == QIcon::On ? "#205da0" : "#314861");
        const QColor fill(disabled ? "#e8ebef" : "#d7e6f4");
        const QPen stroke(ink, 1.65, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p->setPen(stroke);
        p->setBrush(Qt::NoBrush);
        auto line = [p](qreal x1, qreal y1, qreal x2, qreal y2) {
            p->drawLine(QPointF(x1, y1), QPointF(x2, y2));
        };
        switch (glyph_) {
        case Glyph::Front: case Glyph::Back: case Glyph::Right:
        case Glyph::Left: case Glyph::Top: case Glyph::Bottom:
            p->setBrush(fill);
            p->drawRoundedRect(QRectF(1.7, 3.7, 20.6, 16.6), 3, 3);
            line(4.4, 12, 9.2, 12);
            if (glyph_ == Glyph::Right || glyph_ == Glyph::Back || glyph_ == Glyph::Top)
                line(6.8, 9.6, 6.8, 14.4);
            if (glyph_ == Glyph::Right || glyph_ == Glyph::Left) {
                line(12, 8.1, 18.6, 15.9); line(18.6, 8.1, 12, 15.9);
            } else if (glyph_ == Glyph::Front || glyph_ == Glyph::Back) {
                line(11.7, 8.1, 15.3, 12.2); line(18.9, 8.1, 15.3, 12.2); line(15.3, 12.2, 15.3, 16);
            } else {
                p->drawPolyline(QPolygonF() << QPointF(12, 8.1) << QPointF(18.6, 8.1)
                                            << QPointF(12, 15.9) << QPointF(18.6, 15.9));
            }
            break;
        case Glyph::Iso:
            p->setBrush(fill);
            p->drawPolygon(QPolygonF() << QPointF(12, 2.4) << QPointF(20.4, 7.1) << QPointF(20.4, 16.9)
                                       << QPointF(12, 21.6) << QPointF(3.6, 16.9) << QPointF(3.6, 7.1));
            line(3.6, 7.1, 12, 12); line(20.4, 7.1, 12, 12); line(12, 12, 12, 21.6);
            break;
        case Glyph::ZoomIn: case Glyph::ZoomOut:
            p->drawEllipse(QRectF(3, 3, 13.6, 13.6));
            line(14.9, 14.9, 21, 21); line(6.9, 9.8, 12.7, 9.8);
            if (glyph_ == Glyph::ZoomIn) line(9.8, 6.9, 9.8, 12.7);
            break;
        case Glyph::Fit:
            for (int i = 0; i < 4; ++i) {
                p->save();
                p->translate(12, 12);
                p->rotate(90 * i);
                p->drawPolyline(QPolygonF() << QPointF(-8.3, -3.4) << QPointF(-8.3, -8.3) << QPointF(-3.4, -8.3));
                p->restore();
            }
            p->setBrush(fill);
            p->drawRoundedRect(QRectF(7.8, 8.7, 8.4, 6.6), 0.8, 0.8);
            break;
        case Glyph::Surface: case Glyph::Edges: case Glyph::Wire: {
            const QPolygonF patch = QPolygonF() << QPointF(2.8, 15.5) << QPointF(8.2, 5.5)
                                                << QPointF(16.7, 4) << QPointF(21.2, 13) << QPointF(13, 20);
            if (glyph_ != Glyph::Wire) p->setBrush(fill);
            p->drawPolygon(patch);
            if (glyph_ != Glyph::Surface) {
                for (const auto &corner : patch) p->drawLine(QPointF(11.7, 12), corner);
            }
            if (glyph_ == Glyph::Wire) {
                p->setBrush(ink);
                for (const auto &corner : patch) p->drawEllipse(corner, 1.2, 1.2);
            }
            break;
        }
        case Glyph::Axes:
            line(10.6, 14.2, 10.6, 3); line(10.6, 14.2, 20.4, 16.5); line(10.6, 14.2, 3.5, 20.2);
            line(8.5, 5.6, 10.6, 3); line(12.7, 5.6, 10.6, 3);
            line(17.5, 13.8, 20.4, 16.5); line(16.8, 17.8, 20.4, 16.5);
            line(4.2, 16.5, 3.5, 20.2); line(7.2, 19.9, 3.5, 20.2);
            break;
        case Glyph::Reset:
            p->drawArc(QRectF(4.2, 4.2, 15.6, 15.6), -105 * 16, 240 * 16);
            p->drawPolyline(QPolygonF() << QPointF(5.6, 3.9) << QPointF(5.6, 8.7) << QPointF(10.3, 8.7));
            break;
        case Glyph::Center:
            p->drawEllipse(QRectF(5.5, 5.5, 13, 13));
            line(12, 2.2, 12, 7.5); line(12, 16.5, 12, 21.8);
            line(2.2, 12, 7.5, 12); line(16.5, 12, 21.8, 12);
            p->setBrush(ink);
            p->drawEllipse(QPointF(12, 12), 1.4, 1.4);
            break;
        case Glyph::Background: {
            QLinearGradient gradient(0, 4, 0, 20);
            gradient.setColorAt(0, ink);
            gradient.setColorAt(1, QColor(Qt::white));
            p->setBrush(gradient);
            p->drawRoundedRect(QRectF(3.3, 4.8, 17.4, 14.4), 2, 2);
            break;
        }
        }
        p->restore();
    }
private:
    Glyph glyph_;
};

inline QIcon icon(Glyph glyph) { return QIcon(new Icon(glyph)); }

inline void style(QToolBar *bar)
{
    bar->setIconSize(QSize(20, 20));
    bar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    bar->setStyleSheet(QStringLiteral(
        "QToolBar#meshViewerToolbar {background:#f7f9fc;border:1px solid #d6dfe9;padding:2px;spacing:0;}"
        "QToolBar::separator:horizontal {width:1px;background:#c4cfdb;margin:4px 3px;}"
        "QToolBar::separator:vertical {height:1px;background:#c4cfdb;margin:3px 4px;}"
        "QToolButton {background:transparent;border:1px solid transparent;border-radius:4px;padding:1px;}"
        "QToolButton:hover {background:#e9eff7;border-color:#c2cfdf;}"
        "QToolButton:checked {background:#dcebf9;border-color:#9ebcda;}"
        "QToolButton:checked:hover {background:#d1e3f6;border-color:#83a9d0;}"
        "QToolButton:pressed {background:#c5daef;border-color:#7599be;}"
        "QToolButton:focus {border-color:#245f9f;}"
        "QToolButton:disabled {background:transparent;border-color:transparent;}"
        "QToolButton#meshBackgroundMode::menu-indicator {width:5px;height:5px;"
        "subcontrol-origin:padding;subcontrol-position:bottom right;}"));
}

} // namespace meshToolbar
} // namespace aerosim
