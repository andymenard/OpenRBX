#include "UserInput.h"

#include "ActiveDocView.h"
#include "InputDevice.h"
#include "MainFrm.h"
#include "NativeInput.h"
#include "Resources.h"
#include "Roblox.h"
#include "UserInputUtil.h"
#include "rbxgraphics/TextureProxyBase.h"
#include "util/FrameInterpolator.h"
#include "v8datamodel/DataModel.h"
#include "v8datamodel/Workspace.h"

#include <atomic>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#endif

static std::atomic<UserInput*> mouseOwner;
static std::atomic<UserInput*> keyboardOwner;

bool UserInput::isInsideViewport(const G3D::Vector2& offset)
{
	G3D::Vector2 extent = m_viewportRect.center();
	return offset.x >= -extent.x && offset.x <= extent.x && offset.y >= -extent.y && offset.y <= extent.y;
}

bool UserInput::isViewportAtScreenOrigin()
{
	return NativeInput::clientToScreen(m_viewport, QPoint(0, 0)) == QPoint(0, 0);
}

G3D::Vector2int16 UserInput::viewportSize()
{
	return G3D::Vector2int16(
		static_cast<G3D::int16>(m_viewportRect.width()), static_cast<G3D::int16>(m_viewportRect.height())
	);
}

bool UserInput::isMovementKeyDown()
{
	return keyDown(SDLK_w) || keyDown(SDLK_s) || keyDown(SDLK_a) || keyDown(SDLK_d) || keyDown(SDLK_UP) ||
		   keyDown(SDLK_DOWN) || keyDown(SDLK_LEFT) || keyDown(SDLK_RIGHT);
}

void UserInput::setKeyState(SDLKey key, bool down)
{
	m_forcedKey = down ? key : SDLK_UNKNOWN;
}

void UserInput::updateViewportRect()
{
	QSize size = NativeInput::clientSize(m_viewport);
	m_viewportRect = G3D::Rect2D(G3D::Vector2(size.width(), size.height()));
}

bool UserInput::isFrameForeground() const
{
	return NativeInput::isForeground(m_frame);
}

void UserInput::acquireMouse(const G3D::Vector2& position)
{
	std::scoped_lock lock(m_mutex);

	if (m_mouseAcquired) {
		if (!isFrameForeground()) {
			m_mouseAcquired = false;
			m_mouseLost = true;
		}

		if (m_mouseAcquired) {
			return;
		}
	}

	finishMouseLoss();
	if (NativeInput::isLastActivePopupForeground(m_frame) && (!mouseOwner.load() || mouseOwner == this)) {
		QSize viewport = NativeInput::clientSize(m_viewport);
		QPoint center =
			NativeInput::mapToWindow(m_viewport, m_frame, QPoint(viewport.width() / 2, viewport.height() / 2));

		m_acquirePoint = NativeInput::cursorPosition();
		m_mouseAcquired = InputDevice::captureMouse(center.x(), center.y());

		if (m_mouseAcquired) {
			mouseOwner = this;
			NativeInput::hideCursor(m_viewport);
			G3D::Rect2D rect = m_viewportRect;
			m_cursorOffset = position - rect.center();
		}
	}
}

void UserInput::releaseMouse()
{
	InputDevice::releaseMouse();
	if (mouseOwner == this) {
		mouseOwner = nullptr;
	}
}

void UserInput::finishMouseLoss()
{
	if (m_mouseLost) {
		m_mouseLost = false;
		releaseMouse();
		NativeInput::setCursorPosition(m_acquirePoint);
	}
}

void UserInput::updateKeyboardAcquisition()
{
	std::scoped_lock lock(m_mutex);

	if (m_keyboardAcquired && !isFrameForeground()) {
		m_keyboardAcquired = false;
	}

	if (m_focused != m_keyboardAcquired) {
		if (m_focused) {
			m_exclusiveKeyboard = true;
			m_keyboardAcquired = isFrameForeground();
			if (m_keyboardAcquired) {
				InputDevice::flushKeyEvents();
				keyboardOwner = this;
			}
		}
		else {
			m_keyboardAcquired = false;
		}
	}

	if (!m_keyboardAcquired && keyboardOwner == this) {
		keyboardOwner = nullptr;
	}
}

void UserInput::setFocused(bool focused)
{
	std::scoped_lock lock(m_mutex);
	m_focused = focused;
	updateKeyboardAcquisition();
}

bool UserInput::isMouseAcquired() const
{
	std::scoped_lock lock(m_mutex);
	return m_mouseAcquired;
}

bool UserInput::isMouseCaptured()
{
	return mouseOwner.load() != nullptr;
}

bool UserInput::isKeyboardAcquired()
{
	return keyboardOwner.load() != nullptr;
}

void UserInput::processKeyboardEvents()
{
	static thread_local InputEvent events[InputDevice::eventBufferSize];
	int count;
	{
		std::scoped_lock lock(m_mutex);
		if (!isFrameForeground()) {
			m_keyboardAcquired = false;
			if (keyboardOwner == this) {
				keyboardOwner = nullptr;
			}
			return;
		}
		count = InputDevice::takeKeyEvents(events, InputDevice::eventBufferSize);
	}

	RBX::DataModel::Lock dataModelLock(m_dataModel);
	for (int i = 0; i < count; i++) {
		unsigned scanCode = events[i].scanCode;
		bool down = events[i].down;
		SDLMod mod = modifiersFromKeyState(m_keys);
		SDLKey key = keyFromScanCode(scanCode);
		bool handled = false;
		if (m_exclusiveKeyboard && down && m_frame) {
			unsigned virtualKey = NativeInput::virtualKey(scanCode, key);

			if (virtualKey == VKEY_SNAPSHOT) {
				NativeInput::postCommand(m_frame, ID_PRINT_SCREEN);
			}
			else if (virtualKey == VKEY_F1 || virtualKey == VKEY_F4 || !theApp.mainFrame()->isGameLayout()) {
				bool alt = mod & KMOD_ALT;
				bool control = mod & KMOD_CTRL;
				bool shift = mod & KMOD_SHIFT;
				for (const Accelerator& accel : m_accelerators) {
					if (accel.key == virtualKey && accel.alt == alt && accel.control == control &&
						accel.shift == shift) {
						NativeInput::postCommand(m_frame, accel.command);
						handled = true;
						break;
					}
				}
			}
		}

		if (!handled) {
			m_dataModel->processGui(RBX::UIEvent(this, down ? RBX::UIEvent::KEY_DOWN : RBX::UIEvent::KEY_UP, key, mod));
		}
	}
}

void UserInput::sendMouseEvent(RBX::UIEvent::EventType eventType, const G3D::Vector2& position)
{
	G3D::Vector2int16 windowSize(m_viewportRect.width(), m_viewportRect.height());
	G3D::Vector2int16 mousePosition(position);
	RBX::UIEvent event(this, eventType, mousePosition, windowSize);

	RBX::DataModel::Lock lock(m_dataModel);
	m_dataModel->processGui(event);
}

G3D::ReferenceCountedPointer<RBX::TextureProxyBase> UserInput::getGameCursor(RBX::Adorn* adorn)
{
	if (m_mouseAcquired) {
		return UserInputBase::getGameCursor(adorn);
	}

	return nullptr;
}

G3D::Vector2 UserInput::virtualCursorPosition()
{
	std::scoped_lock lock(m_mutex);
	G3D::Rect2D rect = m_viewportRect;
	return rect.center() + m_cursorOffset;
}

G3D::Vector2 UserInput::systemCursorPosition()
{
	std::scoped_lock lock(m_mutex);

	QPoint origin = NativeInput::clientToScreen(m_viewport, QPoint(0, 0));
	QPoint point = NativeInput::cursorPosition() - origin;
	return G3D::Vector2(point.x(), point.y());
}

G3D::Vector2 UserInput::getCursorPosition()
{
	std::scoped_lock lock(m_mutex);
	if (m_mouseAcquired && RBX::FrameInterpolator::isRendering()) {
		return pendingCursorPosition();
	}

	return m_mouseAcquired ? virtualCursorPosition() : systemCursorPosition();
}

G3D::Vector2 UserInput::pendingCursorPosition()
{
	G3D::Vector2 offset = m_cursorOffset;

	static thread_local InputEvent events[InputDevice::eventBufferSize];
	int count = InputDevice::peekMouseMotion(events, InputDevice::eventBufferSize);
	G3D::Vector2 wrapMouseDelta(0.0f, 0.0f);
	for (int i = 0; i < count; i++) {
		if (events[i].x != 0.0f) {
			applyMouseDelta(G3D::Vector2(events[i].x, 0.0f), wrapMouseDelta, offset);
			if (!isInsideViewport(offset)) {
				break;
			}
		}
		if (events[i].y != 0.0f) {
			applyMouseDelta(G3D::Vector2(0.0f, events[i].y), wrapMouseDelta, offset);
			if (!isInsideViewport(offset)) {
				break;
			}
		}
	}

	G3D::Rect2D rect = m_viewportRect;
	return rect.center() + offset;
}

bool UserInput::keyDown(SDLKey key) const
{
	std::scoped_lock lock(m_mutex);

	if (m_forcedKey != SDLK_UNKNOWN && key == m_forcedKey) {
		return true;
	}

	return m_keys[scanCodeFromKey(key)] & 0x80;
}

bool UserInput::isToolboxHiddenInGameLayout()
{
	if (theApp.mainFrame()->isGameLayout()) {
		if (ActiveDocView::activeDataModel()) {
			RBX::DataModel::Lock lock(ActiveDocView::activeDataModel());
			RBX::Verb* verb = ActiveDocView::activeDataModel()->getVerb("ToggleToolbox");
			if (verb) {
				return !verb->isChecked();
			}
		}
	}

	return false;
}

void UserInput::applyMouseDelta(const G3D::Vector2& mouseDelta, G3D::Vector2& wrapMouseDelta, G3D::Vector2& offset)
{
	switch (m_wrapMode) {
	case WRAP_AUTO:
		if (isMovementKeyDown()) {
			wrapCursorAtEdges(mouseDelta, wrapMouseDelta, offset, G3D::Vector2(viewportSize()));
		}
		else if (!isToolboxHiddenInGameLayout()) {
			moveCursorFree(mouseDelta, wrapMouseDelta, offset);
		}
		else if (isViewportAtScreenOrigin()) {
			wrapCursorHorizontalClampTop(mouseDelta, wrapMouseDelta, offset, viewportSize());
		}
		else {
			wrapCursorHorizontalSticky(mouseDelta, wrapMouseDelta, offset, viewportSize());
		}
		break;
	case WRAP_CENTER:
		wrapCursorCenter(mouseDelta, wrapMouseDelta, offset);
		break;
	}
}

void UserInput::onDeactivate()
{
	std::scoped_lock lock(m_mutex);

	finishMouseLoss();
	if (m_mouseAcquired) {
		releaseMouse();

		G3D::Vector2 position = virtualCursorPosition();
		QPoint point(static_cast<int>(position.x), static_cast<int>(position.y));
		NativeInput::setCursorPosition(NativeInput::clientToScreen(m_viewport, point));

		NativeInput::showArrowCursor(m_viewport);

		NativeInput::invalidate(m_viewport);
		m_mouseAcquired = false;
	}
}

void UserInput::keyPressed()
{
	updateKeyboardAcquisition();
}

void UserInput::mouseMessage(RBX::UIEvent::EventType eventType, const G3D::Vector2& position)
{
	sendMouseEvent(eventType, position);

	QPoint point(static_cast<int>(position.x), static_cast<int>(position.y));
	if (NativeInput::isPointVisible(m_viewport, point)) {
		acquireMouse(position);
	}
}

void UserInput::mouseWheel(bool forward, const G3D::Vector2& position)
{
	sendMouseEvent(forward ? RBX::UIEvent::MOUSE_WHEEL_FORWARD : RBX::UIEvent::MOUSE_WHEEL_BACKWARD, position);
}

#ifdef __ANDROID__
void UserInput::touchTap(const G3D::Vector2& position)
{
	RBX::DataModel::Lock modelLock(m_dataModel);
	std::scoped_lock inputLock(m_mutex);
	acquireMouse(position);
	m_cursorOffset = position - m_viewportRect.center();
	// Touch taps are absolute, even while camera input has captured the mouse.
	// Hover first so the GUI establishes focus before receiving the click.
	sendMouseEvent(RBX::UIEvent::MOUSE_MOVE, position);
	sendMouseEvent(RBX::UIEvent::MOUSE_LEFT_BUTTON_DOWN, position);
	sendMouseEvent(RBX::UIEvent::MOUSE_LEFT_BUTTON_UP, position);
}
#endif

#ifdef _WIN32
void UserInput::processMessage(unsigned message, int wParam, std::intptr_t lParam)
{
	switch (message) {
	case WM_KEYDOWN:
		keyPressed();
		break;
	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
		mouseMessage(mouseEventType(message), G3D::Vector2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
		break;
	case WM_MOUSEWHEEL:
		mouseWheel(wParam > 0.0, G3D::Vector2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
		break;
	case WM_ENTERMENULOOP:
	case WM_ENTERSIZEMOVE:
		onDeactivate();
		break;
	}
}
#endif

void UserInput::processMouseEvents()
{
	static thread_local InputEvent events[InputDevice::eventBufferSize];
	int count;
	{
		std::scoped_lock lock(m_mutex);
		if (!isFrameForeground()) {
			m_mouseAcquired = false;
			m_mouseLost = true;
			return;
		}
		count = InputDevice::takeMouseEvents(events, InputDevice::eventBufferSize);
	}

	G3D::Vector2 wrapMouseDelta(0.0f, 0.0f);
	bool moved = false;
	for (int i = 0; i < count; i++) {
		switch (events[i].type) {
		case InputEvent::MouseButton:
			if (events[i].button == InputEvent::LeftButton) {
				NativeInput::postFocus(m_viewport);
				sendMouseEvent(
					events[i].down ? RBX::UIEvent::MOUSE_LEFT_BUTTON_DOWN : RBX::UIEvent::MOUSE_LEFT_BUTTON_UP,
					getCursorPosition()
				);
			}
			else if (events[i].button == InputEvent::RightButton) {
				NativeInput::postFocus(m_viewport);
				sendMouseEvent(
					events[i].down ? RBX::UIEvent::MOUSE_RIGHT_BUTTON_DOWN : RBX::UIEvent::MOUSE_RIGHT_BUTTON_UP,
					getCursorPosition()
				);
			}
			break;
		case InputEvent::MouseMotion:
			if (events[i].x != 0.0f) {
				moved = true;
				applyMouseDelta(G3D::Vector2(events[i].x, 0.0f), wrapMouseDelta, m_cursorOffset);
			}
			if (events[i].y != 0.0f && m_mouseAcquired && isInsideViewport(m_cursorOffset)) {
				moved = true;
				applyMouseDelta(G3D::Vector2(0.0f, events[i].y), wrapMouseDelta, m_cursorOffset);
			}
			break;
		case InputEvent::MouseWheel:
			sendMouseEvent(
				events[i].y > 0.0f ? RBX::UIEvent::MOUSE_WHEEL_FORWARD : RBX::UIEvent::MOUSE_WHEEL_BACKWARD,
				getCursorPosition()
			);
			break;
		default:
			break;
		}

		if (!m_mouseAcquired || !isInsideViewport(m_cursorOffset)) {
			break;
		}
	}

	if (wrapMouseDelta != G3D::Vector2::zero()) {
		RBX::DataModel::Lock lock(m_dataModel);
		m_dataModel->getWorkspace()->onWrapMouse(wrapMouseDelta);
		sendMouseEvent(RBX::UIEvent::MOUSE_DELTA, wrapMouseDelta);
	}

	if (moved) {
		sendMouseEvent(RBX::UIEvent::MOUSE_MOVE, getCursorPosition());
	}
}

void UserInput::onTimer()
{
	updateViewportRect();
	InputDevice::pump();

	std::scoped_lock lock(m_mutex);
	finishMouseLoss();
	InputDevice::flushOtherEvents();
	if (!mouseOwner.load()) {
		InputDevice::flushMouseEvents();
	}
	if (!keyboardOwner.load()) {
		InputDevice::flushKeyEvents();
	}

	if (m_mouseAcquired) {
		G3D::Vector2 position = getCursorPosition();
		QPoint point(static_cast<int>(position.x), static_cast<int>(position.y));

		if (!NativeInput::isPointVisible(m_viewport, point)) {
			onDeactivate();
		}
	}
}

void UserInput::onEvent(const RBX::RunService* source, RBX::Heartbeat event)
{
	std::scoped_lock lock(m_mutex);

	if (m_mouseAcquired) {
		processMouseEvents();
	}

	if (m_keyboardAcquired && isFrameForeground()) {
		InputDevice::readKeyboardState(m_keys);
	}

	if (m_keyboardAcquired) {
		processKeyboardEvents();
	}
}

void UserInput::onEvent(const RBX::RunService* source, RBX::RunTransition event)
{
	updateKeyboardAcquisition();
}

UserInput::~UserInput()
{
	if (m_dataModel) {
		RBX::stopListening(
			m_dataModel->find<RBX::RunService>(), static_cast<RBX::Listener<RBX::RunService, RBX::Heartbeat>*>(this)
		);
		RBX::stopListening(
			m_dataModel->find<RBX::RunService>(), static_cast<RBX::Listener<RBX::RunService, RBX::RunTransition>*>(this)
		);
	}

	if (m_mouseAcquired || m_mouseLost) {
		releaseMouse();
	}
	if (mouseOwner == this) {
		mouseOwner = nullptr;
	}
	if (keyboardOwner == this) {
		keyboardOwner = nullptr;
	}
	InputDevice::close();
}

void UserInput::setWrapMode(WrapMode wrapMode)
{
	m_wrapMode = wrapMode;
}

void UserInput::centerCursor()
{
	m_cursorOffset = G3D::Vector2::zero();
}

UserInput::UserInput(WId viewport, RBX::DataModel* dataModel)
	: m_frame(NativeInput::rootWindow(viewport)), m_viewport(viewport), m_wrapMode(WRAP_AUTO), m_mouseAcquired(false),
	  m_mouseLost(false), m_focused(false), m_keyboardAcquired(false), m_exclusiveKeyboard(false), m_keys{},
	  m_forcedKey(SDLK_UNKNOWN), m_dataModel(RBX::shared_from(dataModel))
{
	updateViewportRect();

	for (const Resources::Accelerator& accel : Resources::accelerators(mainResourceId)) {
		m_accelerators.push_back(
			{accel.key,
			 accel.id,
			 (accel.flags & Resources::ACCEL_SHIFT) != 0,
			 (accel.flags & Resources::ACCEL_CONTROL) != 0,
			 (accel.flags & Resources::ACCEL_ALT) != 0}
		);
	}

	InputDevice::open(reinterpret_cast<void*>(m_frame));

	if (dataModel) {
		RBX::startListening(
			dataModel->create<RBX::RunService>(), static_cast<RBX::Listener<RBX::RunService, RBX::Heartbeat>*>(this)
		);
		RBX::startListening(
			dataModel->create<RBX::RunService>(), static_cast<RBX::Listener<RBX::RunService, RBX::RunTransition>*>(this)
		);
	}

	updateKeyboardAcquisition();
}
