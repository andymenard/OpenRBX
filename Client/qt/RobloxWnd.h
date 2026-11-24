#ifndef ROBLOXWND_H
#define ROBLOXWND_H

#include "RenderSettings.h"
#include "UserInput.h"
#include "ViewBase.h"
#include "rbxgraphics/Adorn.h"
#include "util/IMetric.h"
#include "util/Profiling.h"
#include "util/RunStateOwner.h"
#include "v8datamodel/Stats.h"

#include <GLG3D/GWindow.h>
#include <GLG3D/RenderDevice.h>
#include <QEvent>
#include <QPointF>
#include <QWidget>
#ifdef __ANDROID__
#include "TouchControls.h"
#endif
#include <memory>
#include <mutex>

class CWorkspace;
class QTimer;
class QTouchEvent;
class RenderStats;

namespace Ogre {
class VisualEngine;
}

namespace RBX {
class DataModel;
}

class CRobloxWnd : public QWidget, public RBX::IRunView, public RBX::IMetric
{
public:
	CRobloxWnd(bool alpha, QWidget* parent);
	~CRobloxWnd() override;

	void createRbxGfxView();
	void resizeRbxGfx(int cx, int cy);
	void buildGui();
	void setActive(bool active);
	void createRenderDevice(G3D::GWindow* window);
	void destroyRenderDevice();
	void destroyGraphics();
	void swapBuffers();
	void applyThrottle();
	void createRbxGfx(int cx, int cy);
	void createG3D(int width, int height);
	void resizeG3D(int cx, int cy);
	void createG3DView();
	void renderG3D(bool draw2d);
	void renderRbxGfx(bool draw2d);
	void render(bool draw2d);
	bool paintRbxGfx();
	bool paintG3D();
	void setWorkspace(CWorkspace* workspace);

	void InvalidateRunView() override;
	void AdjustThrottle(double frameRate) override;

	static bool hasActiveWindow();
	static bool renderIdle(double& wait);

	std::string getMetric(const std::string& metric) const override;

	bool create();
	void OnDestroy();
	void OnTimer(unsigned nIDEvent);
	void OnSize(int cx, int cy);
	void OnPaint();
	void OnSetFocus();
	void OnKillFocus();

#ifndef __ANDROID__
	QPaintEngine* paintEngine() const override { return nullptr; }
#endif

protected:
#ifdef _WIN32
	bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
#else
	void paintEvent(QPaintEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void mouseDoubleClickEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;
#endif
#ifdef __ANDROID__
	bool event(QEvent* event) override;
#endif
	void focusInEvent(QFocusEvent* event) override;
	void focusOutEvent(QFocusEvent* event) override;

private:
#ifdef __ANDROID__
	// Touch gestures on the view: a tap clicks, a one-finger drag turns the camera (a right-button
	// drag), and a pinch zooms (the mouse wheel).
	void handleTouch(QTouchEvent* event);
	void sendMouse(QEvent::Type type, QPointF position, Qt::MouseButton button);
	void endTouchDrag();

	int m_touchId = -1;
	QPointF m_touchStart;
	QPointF m_touchLast;
	bool m_touchDragging = false;
	bool m_pinching = false;
	bool m_sendingTouchMouse = false;
	std::unique_ptr<TouchControls> m_touchControls = std::make_unique<TouchControls>();
	qreal m_pinchDistance = 0;
#endif
	class RenderStatsItem : public RBX::Stats::Item
	{
	public:
		RenderStatsItem(RenderStats* stats);

		void update() override;

	private:
		RBX::Stats::Item* m_aggregation;
		RBX::Stats::Item* m_textureMemory;
		RBX::Stats::Item* m_varMemory;
		RBX::Stats::Item* m_videoMemory;
		RBX::Stats::Item* m_majorStateChanges;
		RBX::Stats::Item* m_minorStateChanges;
		RenderStats* m_stats;
	};

	CRenderSettings::GraphicsMode m_graphicsMode;
	int m_fallbackCount;
	std::shared_ptr<RenderStatsItem> m_renderStatsItem;
	CWorkspace* m_workspace;
	std::shared_ptr<RBX::DataModel> m_dataModel;
	bool m_firstFrame;
	std::unique_ptr<UserInput> m_userInput;
	bool m_alpha;
	bool m_rendering;
	bool m_swapPending;
	bool m_swapImmediately;
	double m_throttle;
	std::unique_ptr<RBX::Profiling::CodeProfiler> m_profiler;
	G3D::GWindow* m_glWindow;
	std::unique_ptr<G3D::RenderDevice> m_renderDevice;
	std::unique_ptr<RBX::ViewBase> m_view;
	std::unique_ptr<RBX::Adorn> m_adorn;
	Ogre::VisualEngine* m_visualEngine;
	bool m_created;
#ifndef _WIN32
	QTimer* m_frameTimer = nullptr;
	QTimer* m_inputTimer = nullptr;

	void processMouse(RBX::UIEvent::EventType eventType, QMouseEvent* event, bool handled);
#endif

	bool isIdleActive() const;
};

#endif // ROBLOXWND_H
