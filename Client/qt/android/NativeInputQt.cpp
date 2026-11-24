#include "MainFrm.h"
#include "NativeInput.h"
#include "Roblox.h"
#include "UserInputUtil.h"

#include <QApplication>
#include <QCoreApplication>
#include <QCursor>
#include <QWidget>

// Window queries answered through Qt, for platforms where the native window system is not
// reachable from the client (Android). The views are not native windows there, so a WId passed to
// these functions is the address of the QWidget it stands for.

namespace NativeInput {

static QWidget* widget(WId window)
{
	return reinterpret_cast<QWidget*>(window);
}

static WId id(QWidget* widget)
{
	return reinterpret_cast<WId>(widget);
}

WId rootWindow(WId window)
{
	QWidget* found = widget(window);
	return found ? id(found->window()) : window;
}

QSize clientSize(WId window)
{
	QWidget* found = widget(window);
	return found ? found->size() : QSize();
}

QPoint clientToScreen(WId window, QPoint point)
{
	QWidget* found = widget(window);
	return found ? found->mapToGlobal(point) : point;
}

QPoint mapToWindow(WId from, WId to, QPoint point)
{
	QWidget* source = widget(from);
	QWidget* target = widget(to);
	if (!source || !target) {
		return point;
	}
	return target->mapFromGlobal(source->mapToGlobal(point));
}

bool isForeground(WId frame)
{
	QWidget* active = QApplication::activeWindow();
	return active && id(active) == frame;
}

bool isLastActivePopupForeground(WId frame)
{
	QWidget* active = QApplication::activeWindow();
	if (!active) {
		return false;
	}
	if (id(active) == frame) {
		return true;
	}
	QWidget* owner = active->parentWidget();
	return owner && id(owner->window()) == frame;
}

QPoint cursorPosition()
{
	return QCursor::pos();
}

void setCursorPosition(QPoint point)
{
	QCursor::setPos(point);
}

void hideCursor(WId window)
{
	if (QWidget* found = widget(window)) {
		found->setCursor(Qt::BlankCursor);
	}
}

void showArrowCursor(WId window)
{
	if (QWidget* found = widget(window)) {
		found->setCursor(Qt::ArrowCursor);
	}
}

void invalidate(WId window)
{
	QMetaObject::invokeMethod(
		QCoreApplication::instance(),
		[window]() {
			if (QWidget* found = widget(window)) {
				found->update();
			}
		},
		Qt::QueuedConnection
	);
}

bool isPointVisible(WId window, QPoint point)
{
	QWidget* found = widget(window);
	return found && found->isVisible() && found->rect().contains(point);
}

void postCommand(WId frame, unsigned id)
{
	QMetaObject::invokeMethod(
		theApp.mainFrame(), [id]() { theApp.mainFrame()->sendCommand(id); }, Qt::QueuedConnection
	);
}

void postFocus(WId window)
{
	QMetaObject::invokeMethod(
		QCoreApplication::instance(),
		[window]() {
			if (QWidget* found = widget(window)) {
				found->setFocus();
			}
		},
		Qt::QueuedConnection
	);
}

unsigned virtualKey(unsigned scanCode, SDLKey key)
{
	if (key >= SDLK_a && key <= SDLK_z) {
		return 'A' + (key - SDLK_a);
	}
	if (key >= SDLK_0 && key <= SDLK_9) {
		return '0' + (key - SDLK_0);
	}
	return virtualKeyFromKey(key);
}

void forwardKey(WId window, bool press, unsigned keyCode, unsigned state, unsigned long time, QPoint root)
{
}

} // namespace NativeInput
