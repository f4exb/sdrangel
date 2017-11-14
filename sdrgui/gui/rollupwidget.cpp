#include <QEvent>
#include <QPainter>
#include <QMouseEvent>
#include "gui/rollupwidget.h"
#include "ui_glspectrumgui.h"

RollupWidget::RollupWidget(QWidget* parent) :
	QWidget(parent),
	m_highlighted(false)
{
	setMinimumSize(250, 150);
	setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
	setBackgroundRole(QPalette::Window);

	setAutoFillBackground(false);
	setAttribute(Qt::WA_OpaquePaintEvent, true);

	// Vorgaben aus der Palette
	m_titleColor = palette().highlight().color();
	m_titleTextColor = palette().highlightedText().color();
}

QByteArray RollupWidget::saveState(int version) const
{
	QByteArray state;
	QDataStream stream(&state, QIODevice::WriteOnly);
	int count = 0;

	for(int i = 0; i < children().count(); ++i) {
		QWidget* r = qobject_cast<QWidget*>(children()[i]);
		if(r != NULL)
			count++;
	}

	stream << VersionMarker;
	stream << version;
	stream << count;

	for(int i = 0; i < children().count(); ++i) {
		QWidget* r = qobject_cast<QWidget*>(children()[i]);
		if(r != NULL) {
			stream << r->objectName();
			if(r->isHidden())
				stream << (int)0;
			else stream << (int)1;
		}
	}

	return state;
}

bool RollupWidget::restoreState(const QByteArray& state, int version)
{
	if(state.isEmpty())
		return false;
	QByteArray sd = state;
	QDataStream stream(&sd, QIODevice::ReadOnly);
	int marker, v;
	stream >> marker;
	stream >> v;
	if((stream.status() != QDataStream::Ok) || (marker != VersionMarker) || (v != version))
		return false;

	int count;
	stream >> count;

	if(stream.status() != QDataStream::Ok)
		return false;
	for(int i = 0; i < count; ++i) {
		QString name;
		int visible;

		stream >> name;
		stream >> visible;

		if(stream.status() != QDataStream::Ok)
			return false;

		for(int j = 0; j < children().count(); ++j) {
			QWidget* r = qobject_cast<QWidget*>(children()[j]);
			if(r != NULL) {
				if(r->objectName() == name) {
					if(visible)
						r->show();
					else r->hide();
					break;
				}
			}
		}
	}

	return true;
}

void RollupWidget::setTitleColor(const QColor& c)
{
	m_titleColor = c;
	float l = 0.2126*c.redF() + 0.7152*c.greenF() + 0.0722*c.blueF();
	m_titleTextColor = l < 0.5f ? Qt::white : Qt::black;
	update();
}

void RollupWidget::setHighlighted(bool highlighted)
{
    if (m_highlighted != highlighted)
    {
        m_highlighted = highlighted;
        update();
    }
}

int RollupWidget::arrangeRollups()
{
	QFontMetrics fm(font());
	int pos = fm.height() + 4;

	for(int i = 0; i < children().count(); ++i)
	{
		QWidget* r = qobject_cast<QWidget*>(children()[i]);
		if(r != NULL)
		{
			pos += fm.height() + 2;

			if(!r->isHidden() && (r->windowTitle() != "Basic channel settings"))
			{
				r->move(2, pos + 3);
				int h = 0;

				if(r->hasHeightForWidth()) {
					h = r->heightForWidth(width() - 4);
				} else {
				    h = r->sizeHint().height();
				}

				r->resize(width() - 4, h);
				pos += r->height() + 5;
			}
		}
	}

	setMinimumHeight(pos);
	setMaximumHeight(pos);

	return pos;
}

void RollupWidget::paintEvent(QPaintEvent*)
{
	QPainter p(this);
	QColor frame = palette().highlight().color();

	// Eigenbau
	QFontMetrics fm(font());

	p.setRenderHint(QPainter::Antialiasing, true);

	// Ecken
	p.setPen(Qt::NoPen);
	p.setBrush(palette().base());
	p.drawRect(0, 0, 5, 5);
	p.drawRect(width() - 5, 0, 5, 5);
	p.drawRect(0, height() - 5, 5, 5);
	p.drawRect(width() - 5, height() - 5, 5, 5);

	// Rahmen
	p.setPen(m_highlighted ? Qt::white : frame);
	p.setBrush(palette().window());
	QRectF r(rect());
	r.adjust(0.5, 0.5, -0.5, -0.5);
	p.drawRoundedRect(r, 3.0, 3.0, Qt::AbsoluteSize);

	// Titel-Hintergrund
	p.setPen(Qt::NoPen);
	p.setBrush(m_titleColor);
	QPainterPath path;
	path.moveTo(1.5, fm.height() + 2.5);
	path.lineTo(width() - 1.5, fm.height() + 2.5);
	path.lineTo(width() - 1.5, 3.5);
	path.arcTo(QRectF(width() - 3.5, 0, 2.5, 2.5), 270, -90);
	path.lineTo(3.5, 1.5);
	path.arcTo(QRectF(1.5, 2.5, 2.5, 2.5), 90, 90);
	p.drawPath(path);

	// Titel-Abschlusslinie
	p.setPen(frame);
	p.drawLine(QPointF(0.5, 2 + fm.height() + 1.5), QPointF(width() - 1.5, 2 + fm.height() + 1.5));

	// Aktiv-Button links
	p.setPen(QPen(palette().windowText().color(), 1.0));
	p.setBrush(palette().light());
	p.drawRoundedRect(QRectF(3.5, 3.5, fm.ascent(), fm.ascent()), 2.0, 2.0, Qt::AbsoluteSize);

	// Schließen-Button rechts
	p.setRenderHint(QPainter::Antialiasing, true);
	p.setPen(QPen(palette().windowText().color(), 1.0));
	p.setBrush(palette().light());
	r = QRectF(width() - 3.5 - fm.ascent(), 3.5, fm.ascent(), fm.ascent());
	p.drawRoundedRect(r, 2.0, 2.0, Qt::AbsoluteSize);
	p.setPen(QPen(palette().windowText().color(), 1.5));
	p.drawLine(r.topLeft() + QPointF(1, 1), r.bottomRight() + QPointF(-1, -1));
	p.drawLine(r.bottomLeft() + QPointF(1, -1), r.topRight() + QPointF(-1, 1));

	// Titel
	//p.setPen(palette().highlightedText().color());
	p.setPen(m_titleTextColor);
	p.drawText(QRect(2 + fm.height(), 2, width() - 4 - 2 * fm.height(), fm.height()),
		fm.elidedText(windowTitle(), Qt::ElideMiddle, width() - 4 - 2 * fm.height(), 0));

	// Rollups
	int pos = fm.height() + 4;

	const QObjectList& c = children();
	QObjectList::ConstIterator w = c.begin();
	QObjectList::ConstIterator n = c.begin();

	for(n = c.begin(); n != c.end(); ++n) {
		if(qobject_cast<QWidget*>(*n) != NULL)
			break;
	}
	for(w = n; w != c.end(); w = n) {
		if(n != c.end())
			++n;
		for(; n != c.end(); ++n) {
			if(qobject_cast<QWidget*>(*n) != NULL)
				break;
		}
		pos += paintRollup(qobject_cast<QWidget*>(*w), pos, &p, n == c.end(), frame);
	}
}

int RollupWidget::paintRollup(QWidget* rollup, int pos, QPainter* p, bool last, const QColor& frame)
{
    if (rollup->windowTitle() == "Basic channel settings") return 0;

	QFontMetrics fm(font());
	int height = 1;

	// Titel-Abschlusslinie
	if(!rollup->isHidden()) {
		p->setPen(palette().dark().color());
		p->drawLine(QPointF(1.5, pos + fm.height() + 1.5), QPointF(width() - 1.5, pos + fm.height() + 1.5));
		p->setPen(palette().light().color());
		p->drawLine(QPointF(1.5, pos + fm.height() + 2.5), QPointF(width() - 1.5, pos + fm.height() + 2.5));
		height += 2;
	} else {
		if(!last) {
			p->setPen(frame);
			p->drawLine(QPointF(1.5, pos + fm.height() + 1.5), QPointF(width() - 1.5, pos + fm.height() + 1.5));
			height++;
		}
	}

	// Titel
	p->setPen(palette().windowText().color());
	p->drawText(QRect(2 + fm.height(), pos, width() - 4 - fm.height(), fm.height()),
		fm.elidedText(rollup->windowTitle(), Qt::ElideMiddle, width() - 4 - fm.height(), 0));
	height += fm.height();

	// Ausklapp-Icon
	p->setPen(palette().windowText().color());
	p->setBrush(palette().windowText());
	if(!rollup->isHidden()) {
		QPolygonF a;
		a.append(QPointF(3.5, pos + 2));
		a.append(QPointF(3.5 + fm.ascent(), pos + 2));
		a.append(QPointF(3.5 + fm.ascent() / 2.0, pos + fm.height() - 2));
		p->drawPolygon(a);
	} else {
		QPolygonF a;
		a.append(QPointF(3.5, pos + 2));
		a.append(QPointF(3.5, pos + fm.height() - 2));
		a.append(QPointF(3.5 + fm.ascent(), pos + fm.height() / 2));
		p->drawPolygon(a);
	}

	// Inhalt
	if(!rollup->isHidden() && (!last)) {
		// Rollup-Abschlusslinie
		p->setPen(frame);
		p->drawLine(QPointF(1.5, pos + fm.height() + rollup->height() + 6.5),
					QPointF(width() - 1.5, pos + fm.height() + rollup->height() + 6.5));
		height += rollup->height() + 4;
	}

	return height;
}

void RollupWidget::resizeEvent(QResizeEvent* size)
{
	arrangeRollups();
	QWidget::resizeEvent(size);
}

void RollupWidget::mousePressEvent(QMouseEvent* event)
{
	QFontMetrics fm(font());

	// menu box left
	if(QRectF(3.5, 3.5, fm.ascent(), fm.ascent()).contains(event->pos())) {
		emit customContextMenuRequested(event->globalPos());
		return;
	}

	// close button right
	if(QRectF(width() - 3.5 - fm.ascent(), 3.5, fm.ascent(), fm.ascent()).contains(event->pos())) {
		close();
		return;
	}

	// check if we need to change a rollup widget
	int pos = fm.height() + 4;
	for(int i = 0; i < children().count(); ++i) {
		QWidget* r = qobject_cast<QWidget*>(children()[i]);
		if(r != NULL) {
			if((event->y() >= pos) && (event->y() < (pos + fm.height() + 3))) {
				if(r->isHidden()) {
					r->show();
					//emit widgetRolled(r, true);
				} else {
					r->hide();
					//emit widgetRolled(r, false);
				}
				arrangeRollups();
				repaint();
				return;
			} else {
				pos += fm.height() + 2;
				if(!r->isHidden())
					pos += r->height() + 5;
			}
		}
	}
}

bool RollupWidget::event(QEvent* event)
{
	if(event->type() == QEvent::ChildAdded) {
		((QChildEvent*)event)->child()->installEventFilter(this);
		arrangeRollups();
	} else if(event->type() == QEvent::ChildRemoved) {
		((QChildEvent*)event)->child()->removeEventFilter(this);
		arrangeRollups();
	}
	return QWidget::event(event);
}

bool RollupWidget::eventFilter(QObject* object, QEvent* event)
{
	if(event->type() == QEvent::Show) {
		if(children().contains(object)) {
			arrangeRollups();
			emit widgetRolled(qobject_cast<QWidget*>(object), true);
		}
	} else if(event->type() == QEvent::Hide) {
		if(children().contains(object)) {
			arrangeRollups();
			emit widgetRolled(qobject_cast<QWidget*>(object), false);
		}
	} else if(event->type() == QEvent::WindowTitleChange) {
		if(children().contains(object))
			repaint();
	}
	return QWidget::eventFilter(object, event);
}
