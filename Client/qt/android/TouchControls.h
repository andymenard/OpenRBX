#ifndef TOUCHCONTROLS_H
#define TOUCHCONTROLS_H

#include <QPointF>
#include <QSizeF>

namespace G3D {
class RenderDevice;
}

// On-screen controls for phones without a keyboard, drawn into the 3D view. A thumb put down on the
// left half of the view becomes a joystick centred where it landed, holding W/A/S/D as it moves; a
// jump button in the lower right holds Space. Positions are in the view's widget units.
class TouchControls
{
public:
	~TouchControls();

	// Returns true when the controls take the new touch, which then belongs to them until released.
	bool press(int id, QPointF position, QSizeF view);
	void move(int id, QPointF position);
	void release(int id);
	void releaseAll();
	bool owns(int id) const { return id == m_stickTouch || id == m_jumpTouch; }

	// Draws in the current 2D projection, which must be in widget units.
	void render(G3D::RenderDevice* renderDevice, QSizeF view) const;

private:
	static QPointF restingStick(QSizeF view);
	static QPointF jumpCenter(QSizeF view);
	void setStick(QPointF position);
	void setJump(bool down);

	int m_stickTouch = -1;
	int m_jumpTouch = -1;
	QPointF m_stickCenter;
	QPointF m_knob;
	bool m_keys[4] = {}; // W, A, S, D
	bool m_jump = false;
};

#endif // TOUCHCONTROLS_H
