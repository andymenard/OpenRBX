#include "TouchControls.h"

#include "InputDevice.h"
#include "UserInputUtil.h"

#include <G3D/Color4.h>
#include <G3D/Vector2.h>
#include <GLG3D/RenderDevice.h>
#include <cmath>

static constexpr float stickRadius = 56;
static constexpr float knobRadius = 24;
static constexpr float jumpRadius = 34;
static constexpr float margin = 14;
// Fraction of the stick radius the thumb must move before a direction key is held.
static constexpr float deadZone = 0.3f;
// The joystick zone starts below the client's menu bar at the top of the view.
static constexpr float stickZoneTop = 40;

static const unsigned stickKeys[4] = {SCAN_W, SCAN_A, SCAN_S, SCAN_D};

static float distance(QPointF a, QPointF b)
{
	return static_cast<float>(std::hypot(a.x() - b.x(), a.y() - b.y()));
}

TouchControls::~TouchControls()
{
	releaseAll();
}

QPointF TouchControls::restingStick(QSizeF view)
{
	return QPointF(margin + stickRadius, view.height() - margin - stickRadius);
}

QPointF TouchControls::jumpCenter(QSizeF view)
{
	// Left of the client's health bar and above its camera buttons in the lower right corner.
	return QPointF(view.width() - 130, view.height() - 110 - jumpRadius);
}

bool TouchControls::press(int id, QPointF position, QSizeF view)
{
	if (m_jumpTouch < 0 && distance(position, jumpCenter(view)) <= jumpRadius + 6) {
		m_jumpTouch = id;
		setJump(true);
		return true;
	}
	if (m_stickTouch < 0 && position.x() < view.width() / 2 && position.y() > stickZoneTop) {
		m_stickTouch = id;
		m_stickCenter = position;
		setStick(position);
		return true;
	}
	return false;
}

void TouchControls::move(int id, QPointF position)
{
	if (id == m_stickTouch) {
		setStick(position);
	}
}

void TouchControls::release(int id)
{
	if (id == m_stickTouch) {
		m_stickTouch = -1;
		setStick(m_stickCenter);
	}
	if (id == m_jumpTouch) {
		m_jumpTouch = -1;
		setJump(false);
	}
}

void TouchControls::releaseAll()
{
	release(m_stickTouch);
	release(m_jumpTouch);
}

void TouchControls::setStick(QPointF position)
{
	QPointF offset = position - m_stickCenter;
	float length = distance(position, m_stickCenter);
	if (length > stickRadius) {
		offset *= stickRadius / length;
	}
	m_knob = offset;

	float threshold = stickRadius * deadZone;
	bool keys[4] = {offset.y() < -threshold, offset.x() < -threshold, offset.y() > threshold, offset.x() > threshold};
	for (int i = 0; i < 4; i++) {
		if (keys[i] != m_keys[i]) {
			m_keys[i] = keys[i];
			InputDevice::setVirtualKey(stickKeys[i], keys[i]);
		}
	}
}

void TouchControls::setJump(bool down)
{
	if (down != m_jump) {
		m_jump = down;
		InputDevice::setVirtualKey(SCAN_SPACE, down);
	}
}

static void disc(G3D::RenderDevice* rd, QPointF center, float radius, const G3D::Color4& color)
{
	const int segments = 40;
	rd->setColor(color);
	rd->beginPrimitive(G3D::RenderDevice::TRIANGLE_FAN);
	rd->sendVertex(G3D::Vector2(static_cast<float>(center.x()), static_cast<float>(center.y())));
	for (int i = 0; i <= segments; i++) {
		float angle = 2.0f * 3.14159265f * i / segments;
		rd->sendVertex(
			G3D::Vector2(
				static_cast<float>(center.x()) + radius * std::cos(angle),
				static_cast<float>(center.y()) + radius * std::sin(angle)
			)
		);
	}
	rd->endPrimitive();
}

static void ring(G3D::RenderDevice* rd, QPointF center, float radius, float width, const G3D::Color4& color)
{
	const int segments = 48;
	rd->setColor(color);
	rd->beginPrimitive(G3D::RenderDevice::QUAD_STRIP);
	for (int i = 0; i <= segments; i++) {
		float angle = 2.0f * 3.14159265f * i / segments;
		float c = std::cos(angle);
		float s = std::sin(angle);
		rd->sendVertex(
			G3D::Vector2(static_cast<float>(center.x()) + radius * c, static_cast<float>(center.y()) + radius * s)
		);
		rd->sendVertex(
			G3D::Vector2(
				static_cast<float>(center.x()) + (radius - width) * c,
				static_cast<float>(center.y()) + (radius - width) * s
			)
		);
	}
	rd->endPrimitive();
}

void TouchControls::render(G3D::RenderDevice* rd, QSizeF view) const
{
	rd->pushState();
	rd->setTexture(0, nullptr);
	rd->setBlendFunc(G3D::RenderDevice::BLEND_SRC_ALPHA, G3D::RenderDevice::BLEND_ONE_MINUS_SRC_ALPHA);

	// The joystick rests in the lower left as a hint until a thumb lands somewhere on the left half.
	bool active = m_stickTouch >= 0;
	QPointF stick = active ? m_stickCenter : restingStick(view);
	disc(rd, stick, stickRadius, G3D::Color4(0, 0, 0, active ? 0.3f : 0.18f));
	ring(rd, stick, stickRadius, 2, G3D::Color4(1, 1, 1, active ? 0.6f : 0.35f));
	disc(rd, stick + (active ? m_knob : QPointF()), knobRadius, G3D::Color4(1, 1, 1, active ? 0.75f : 0.4f));

	QPointF jump = jumpCenter(view);
	disc(rd, jump, jumpRadius, m_jump ? G3D::Color4(1, 1, 1, 0.65f) : G3D::Color4(0, 0, 0, 0.25f));
	ring(rd, jump, jumpRadius, 2, G3D::Color4(1, 1, 1, 0.6f));
	// An upward chevron for "jump".
	G3D::Color4 mark = m_jump ? G3D::Color4(0, 0, 0, 0.8f) : G3D::Color4(1, 1, 1, 0.9f);
	float cx = static_cast<float>(jump.x());
	float cy = static_cast<float>(jump.y());
	rd->setColor(mark);
	rd->beginPrimitive(G3D::RenderDevice::QUADS);
	rd->sendVertex(G3D::Vector2(cx - 14, cy + 8));
	rd->sendVertex(G3D::Vector2(cx, cy - 6));
	rd->sendVertex(G3D::Vector2(cx, cy - 13));
	rd->sendVertex(G3D::Vector2(cx - 14, cy + 1));
	rd->sendVertex(G3D::Vector2(cx, cy - 6));
	rd->sendVertex(G3D::Vector2(cx + 14, cy + 8));
	rd->sendVertex(G3D::Vector2(cx + 14, cy + 1));
	rd->sendVertex(G3D::Vector2(cx, cy - 13));
	rd->endPrimitive();

	rd->popState();
}
