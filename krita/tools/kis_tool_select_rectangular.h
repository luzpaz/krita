/*
 *  Copyright (c) 1999 Michael Koch <koch@kde.org>
 *                2002 Patrick Julien <freak@codepimps.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#if !defined KIS_TOOL_SELECT_RECTANGULAR_H_
#define KIS_TOOL_SELECT_RECTANGULAR_H_

#include <qpoint.h>
#include "kis_tool.h"
#include "kis_tool_non_paint.h"

class KisToolRectangularSelect : public KisToolNonPaint {
	typedef KisToolNonPaint super;

public:
	KisToolRectangularSelect(KisView *view, KisDoc *doc);
	virtual ~KisToolRectangularSelect();

public:
	virtual void setup();
	virtual void paint(QPainter& gc);
	virtual void paint(QPainter& gc, const QRect& rc);
	virtual void mousePress(QMouseEvent *e);
	virtual void mouseMove(QMouseEvent *e);
	virtual void mouseRelease(QMouseEvent *e);

private:
	void clearSelection();
	void paintOutline();
	void paintOutline(QPainter& gc, const QRect& rc);

private:
	KisView *m_view;
	KisDoc *m_doc;
	QPoint m_startPos;
	QPoint m_endPos;
	bool m_selecting;
};

#endif // KIS_TOOL_SELECT_RECTANGULAR_H_

