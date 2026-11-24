#ifndef USERINPUT_H
#define USERINPUT_H

#include "util/Events.h"
#include "util/RunStateOwner.h"
#include "util/UIEvent.h"
#include "util/UserInputBase.h"

#include <G3D/Rect2D.h>
#include <QPoint>
#include <cstdint>
#include <memory>
#include <mutex>
#include <qwindowdefs.h>
#include <vector>

namespace RBX {
class DataModel;
}

class UserInput : public RBX::UserInputBase,
				  public RBX::Listener<RBX::RunService, RBX::RunTransition>,
				  public RBX::Listener<RBX::RunService, RBX::Heartbeat>
{
public:
	UserInput(WId viewport, RBX::DataModel* dataModel);
	~UserInput() override;

	void setWrapMode(WrapMode wrapMode) override;
	WrapMode getWrapMode() const override { return m_wrapMode; }
	void centerCursor() override;
	bool keyDown(SDLKey key) const override;
	void setKeyState(SDLKey key, bool down) override;

	void setFocused(bool focused);
	void onDeactivate();
	void keyPressed();
	void mouseMessage(RBX::UIEvent::EventType eventType, const G3D::Vector2& position);
	void mouseWheel(bool forward, const G3D::Vector2& position);
#ifdef __ANDROID__
	void touchTap(const G3D::Vector2& position);
#endif
#ifdef _WIN32
	void processMessage(unsigned message, int wParam, std::intptr_t lParam);
#endif
	void onTimer();

	bool isMouseAcquired() const;
	static bool isMouseCaptured();
	static bool isKeyboardAcquired();

protected:
	G3D::Vector2 getCursorPosition() override;
	G3D::ReferenceCountedPointer<RBX::TextureProxyBase> getGameCursor(RBX::Adorn* adorn) override;

	void onEvent(const RBX::RunService* source, RBX::Heartbeat event) override;
	void onEvent(const RBX::RunService* source, RBX::RunTransition event) override;

private:
	bool isInsideViewport(const G3D::Vector2& offset);
	bool isViewportAtScreenOrigin();
	G3D::Vector2int16 viewportSize();
	bool isMovementKeyDown();
	void updateViewportRect();
	void acquireMouse(const G3D::Vector2& position);
	void releaseMouse();
	void finishMouseLoss();
	void updateKeyboardAcquisition();
	bool isFrameForeground() const;
	void processKeyboardEvents();
	void processMouseEvents();
	void sendMouseEvent(RBX::UIEvent::EventType eventType, const G3D::Vector2& position);
	G3D::Vector2 virtualCursorPosition();
	G3D::Vector2 systemCursorPosition();
	G3D::Vector2 pendingCursorPosition();
	static bool isToolboxHiddenInGameLayout();
	void applyMouseDelta(const G3D::Vector2& mouseDelta, G3D::Vector2& wrapMouseDelta, G3D::Vector2& offset);

	mutable std::recursive_mutex m_mutex;
	struct Accelerator
	{
		unsigned key;
		unsigned command;
		bool shift;
		bool control;
		bool alt;
	};

	WId m_frame;
	WId m_viewport;
	std::vector<Accelerator> m_accelerators;
	G3D::Rect2D m_viewportRect;
	WrapMode m_wrapMode;
	G3D::Vector2 m_cursorOffset;
	QPoint m_acquirePoint;
	bool m_mouseAcquired;
	bool m_mouseLost;
	bool m_focused;
	bool m_keyboardAcquired;
	bool m_exclusiveKeyboard;
	unsigned char m_keys[0x100];
	SDLKey m_forcedKey;
	std::shared_ptr<RBX::DataModel> m_dataModel;
};

#endif // USERINPUT_H
