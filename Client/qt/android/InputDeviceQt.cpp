#include "InputDevice.h"
#include "UserInputUtil.h"

#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QObject>
#include <QWheelEvent>
#include <algorithm>
#include <cstring>
#include <deque>
#include <mutex>

// Android has no SDL window to read raw input from, so the device is fed from Qt's events. While the
// mouse is captured, pointer movement (a touch drag on most devices) becomes relative motion.

struct KeyEntry
{
	int key;
	KeyboardScanCode scanCode;
};

static const KeyEntry keys[] = {
	{Qt::Key_Escape, SCAN_ESCAPE},
	{Qt::Key_1, SCAN_1},
	{Qt::Key_2, SCAN_2},
	{Qt::Key_3, SCAN_3},
	{Qt::Key_4, SCAN_4},
	{Qt::Key_5, SCAN_5},
	{Qt::Key_6, SCAN_6},
	{Qt::Key_7, SCAN_7},
	{Qt::Key_8, SCAN_8},
	{Qt::Key_9, SCAN_9},
	{Qt::Key_0, SCAN_0},
	{Qt::Key_Minus, SCAN_MINUS},
	{Qt::Key_Equal, SCAN_EQUALS},
	{Qt::Key_Backspace, SCAN_BACK},
	{Qt::Key_Tab, SCAN_TAB},
	{Qt::Key_Q, SCAN_Q},
	{Qt::Key_W, SCAN_W},
	{Qt::Key_E, SCAN_E},
	{Qt::Key_R, SCAN_R},
	{Qt::Key_T, SCAN_T},
	{Qt::Key_Y, SCAN_Y},
	{Qt::Key_U, SCAN_U},
	{Qt::Key_I, SCAN_I},
	{Qt::Key_O, SCAN_O},
	{Qt::Key_P, SCAN_P},
	{Qt::Key_BracketLeft, SCAN_LBRACKET},
	{Qt::Key_BracketRight, SCAN_RBRACKET},
	{Qt::Key_Return, SCAN_RETURN},
	{Qt::Key_A, SCAN_A},
	{Qt::Key_S, SCAN_S},
	{Qt::Key_D, SCAN_D},
	{Qt::Key_F, SCAN_F},
	{Qt::Key_G, SCAN_G},
	{Qt::Key_H, SCAN_H},
	{Qt::Key_J, SCAN_J},
	{Qt::Key_K, SCAN_K},
	{Qt::Key_L, SCAN_L},
	{Qt::Key_Semicolon, SCAN_SEMICOLON},
	{Qt::Key_Apostrophe, SCAN_APOSTROPHE},
	{Qt::Key_QuoteLeft, SCAN_GRAVE},
	{Qt::Key_Backslash, SCAN_BACKSLASH},
	{Qt::Key_Z, SCAN_Z},
	{Qt::Key_X, SCAN_X},
	{Qt::Key_C, SCAN_C},
	{Qt::Key_V, SCAN_V},
	{Qt::Key_B, SCAN_B},
	{Qt::Key_N, SCAN_N},
	{Qt::Key_M, SCAN_M},
	{Qt::Key_Comma, SCAN_COMMA},
	{Qt::Key_Period, SCAN_PERIOD},
	{Qt::Key_Slash, SCAN_SLASH},
	{Qt::Key_Space, SCAN_SPACE},
	{Qt::Key_CapsLock, SCAN_CAPITAL},
	{Qt::Key_F1, SCAN_F1},
	{Qt::Key_F2, SCAN_F2},
	{Qt::Key_F3, SCAN_F3},
	{Qt::Key_F4, SCAN_F4},
	{Qt::Key_F5, SCAN_F5},
	{Qt::Key_F6, SCAN_F6},
	{Qt::Key_F7, SCAN_F7},
	{Qt::Key_F8, SCAN_F8},
	{Qt::Key_F9, SCAN_F9},
	{Qt::Key_F10, SCAN_F10},
	{Qt::Key_F11, SCAN_F11},
	{Qt::Key_F12, SCAN_F12},
	{Qt::Key_NumLock, SCAN_NUMLOCK},
	{Qt::Key_ScrollLock, SCAN_SCROLL},
	{Qt::Key_Print, SCAN_SYSRQ},
	{Qt::Key_Pause, SCAN_PAUSE},
	{Qt::Key_Home, SCAN_HOME},
	{Qt::Key_Up, SCAN_UP},
	{Qt::Key_PageUp, SCAN_PRIOR},
	{Qt::Key_Left, SCAN_LEFT},
	{Qt::Key_Right, SCAN_RIGHT},
	{Qt::Key_End, SCAN_END},
	{Qt::Key_Down, SCAN_DOWN},
	{Qt::Key_PageDown, SCAN_NEXT},
	{Qt::Key_Insert, SCAN_INSERT},
	{Qt::Key_Delete, SCAN_DELETE},
	{Qt::Key_Menu, SCAN_APPS},
};

static const KeyEntry keypadKeys[] = {
	{Qt::Key_7, SCAN_NUMPAD7},
	{Qt::Key_8, SCAN_NUMPAD8},
	{Qt::Key_9, SCAN_NUMPAD9},
	{Qt::Key_4, SCAN_NUMPAD4},
	{Qt::Key_5, SCAN_NUMPAD5},
	{Qt::Key_6, SCAN_NUMPAD6},
	{Qt::Key_1, SCAN_NUMPAD1},
	{Qt::Key_2, SCAN_NUMPAD2},
	{Qt::Key_3, SCAN_NUMPAD3},
	{Qt::Key_0, SCAN_NUMPAD0},
	{Qt::Key_Period, SCAN_DECIMAL},
	{Qt::Key_Minus, SCAN_SUBTRACT},
	{Qt::Key_Plus, SCAN_ADD},
	{Qt::Key_Asterisk, SCAN_MULTIPLY},
	{Qt::Key_Slash, SCAN_DIVIDE},
	{Qt::Key_Enter, SCAN_NUMPADENTER},
	{Qt::Key_Equal, SCAN_NUMPADEQUALS},
};

// Linux evdev codes, which Android reports as the native scan code of hardware keys.
enum
{
	evdevLeftCtrl = 29,
	evdevLeftShift = 42,
	evdevRightShift = 54,
	evdevLeftAlt = 56,
	evdevRightCtrl = 97,
	evdevRightAlt = 100,
	evdevLeftMeta = 125,
	evdevRightMeta = 126,
};

static unsigned scanCodeFromKey(const QKeyEvent* event)
{
	unsigned native = event->nativeScanCode();
	switch (event->key()) {
	case Qt::Key_Shift:
		return native == evdevRightShift ? SCAN_RSHIFT : SCAN_LSHIFT;
	case Qt::Key_Control:
		return native == evdevRightCtrl ? SCAN_RCONTROL : SCAN_LCONTROL;
	case Qt::Key_Alt:
		return native == evdevRightAlt ? SCAN_RMENU : SCAN_LMENU;
	case Qt::Key_AltGr:
		return SCAN_RMENU;
	case Qt::Key_Meta:
		return native == evdevRightMeta ? SCAN_RWIN : SCAN_LWIN;
	default:
		break;
	}

	if (event->modifiers() & Qt::KeypadModifier) {
		for (const KeyEntry& entry : keypadKeys) {
			if (entry.key == event->key()) {
				return entry.scanCode;
			}
		}
	}

	for (const KeyEntry& entry : keys) {
		if (entry.key == event->key()) {
			return entry.scanCode;
		}
	}
	return 0;
}

static std::mutex mutex;
static std::deque<InputEvent> mouseEvents;
static std::deque<InputEvent> keyEvents;
static unsigned char keyState[256];
static bool captured;
static bool havePointer;
static QPointF lastPointer;
static int users;

class EventTap : public QObject
{
protected:
	bool eventFilter(QObject* watched, QEvent* event) override
	{
		switch (event->type()) {
		case QEvent::KeyPress:
		case QEvent::KeyRelease: {
			QKeyEvent* key = static_cast<QKeyEvent*>(event);
			unsigned scanCode = scanCodeFromKey(key);
			if (scanCode != 0 && scanCode < sizeof(keyState)) {
				std::scoped_lock lock(mutex);
				bool down = event->type() == QEvent::KeyPress;
				keyState[scanCode] = down ? 0x80 : 0;
				if (!key->isAutoRepeat() && keyEvents.size() < InputDevice::eventBufferSize) {
					InputEvent input = {};
					input.type = InputEvent::Key;
					input.down = down;
					input.scanCode = scanCode;
					keyEvents.push_back(input);
				}
			}
			break;
		}
		case QEvent::MouseMove: {
			QPointF position = static_cast<QMouseEvent*>(event)->globalPosition();
			std::scoped_lock lock(mutex);
			if (captured && havePointer && mouseEvents.size() < InputDevice::eventBufferSize) {
				InputEvent input = {};
				input.type = InputEvent::MouseMotion;
				input.x = static_cast<float>(position.x() - lastPointer.x());
				input.y = static_cast<float>(position.y() - lastPointer.y());
				mouseEvents.push_back(input);
			}
			lastPointer = position;
			havePointer = true;
			break;
		}
		case QEvent::MouseButtonPress: {
			std::scoped_lock lock(mutex);
			// A new touch starts somewhere else; it must not count as a jump from the last one.
			lastPointer = static_cast<QMouseEvent*>(event)->globalPosition();
			havePointer = true;
			break;
		}
		default:
			break;
		}
		return QObject::eventFilter(watched, event);
	}
};

static EventTap* tap;

namespace InputDevice {

void open(void* frame)
{
	if (users++ != 0) {
		return;
	}
	tap = new EventTap;
	QCoreApplication::instance()->installEventFilter(tap);
}

void close()
{
	if (--users == 0) {
		delete tap;
		tap = nullptr;
		std::scoped_lock lock(mutex);
		mouseEvents.clear();
		keyEvents.clear();
		std::memset(keyState, 0, sizeof(keyState));
		captured = false;
	}
}

void pump()
{
}

bool captureMouse(int x, int y)
{
	std::scoped_lock lock(mutex);
	mouseEvents.clear();
	captured = true;
	return true;
}

void releaseMouse()
{
	std::scoped_lock lock(mutex);
	captured = false;
}

int takeMouseEvents(InputEvent* events, int capacity)
{
	std::scoped_lock lock(mutex);
	int count = 0;
	while (count < capacity && !mouseEvents.empty()) {
		events[count++] = mouseEvents.front();
		mouseEvents.pop_front();
	}
	return count;
}

int peekMouseMotion(InputEvent* events, int capacity)
{
	std::scoped_lock lock(mutex);
	int count = 0;
	for (const InputEvent& event : mouseEvents) {
		if (count >= capacity) {
			break;
		}
		if (event.type == InputEvent::MouseMotion) {
			events[count++] = event;
		}
	}
	return count;
}

void flushMouseEvents()
{
	std::scoped_lock lock(mutex);
	mouseEvents.clear();
}

int takeKeyEvents(InputEvent* events, int capacity)
{
	std::scoped_lock lock(mutex);
	int count = 0;
	while (count < capacity && !keyEvents.empty()) {
		events[count++] = keyEvents.front();
		keyEvents.pop_front();
	}
	return count;
}

void flushKeyEvents()
{
	std::scoped_lock lock(mutex);
	keyEvents.clear();
}

void readKeyboardState(unsigned char* keys)
{
	std::scoped_lock lock(mutex);
	std::memcpy(keys, keyState, sizeof(keyState));
}

void flushOtherEvents()
{
}

void postMouseButton(InputEvent::Button button, bool down)
{
	std::scoped_lock lock(mutex);
	if (mouseEvents.size() < eventBufferSize) {
		InputEvent input = {};
		input.type = InputEvent::MouseButton;
		input.button = button;
		input.down = down;
		mouseEvents.push_back(input);
	}
}

void setVirtualKey(unsigned scanCode, bool down)
{
	if (scanCode == 0 || scanCode >= sizeof(keyState)) {
		return;
	}
	std::scoped_lock lock(mutex);
	if (((keyState[scanCode] & 0x80) != 0) == down) {
		return;
	}
	keyState[scanCode] = down ? 0x80 : 0;
	if (keyEvents.size() < eventBufferSize) {
		InputEvent input = {};
		input.type = InputEvent::Key;
		input.down = down;
		input.scanCode = scanCode;
		keyEvents.push_back(input);
	}
}

void postMouseWheel(float y)
{
	std::scoped_lock lock(mutex);
	if (mouseEvents.size() < eventBufferSize) {
		InputEvent input = {};
		input.type = InputEvent::MouseWheel;
		input.y = y;
		mouseEvents.push_back(input);
	}
}

} // namespace InputDevice
