#include "QtGLWindow.h"

#include "QtGLCaps.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <QCoreApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QScreen>
#include <QWidget>
#include <QWindow>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <thread>

static QOpenGLContext* glContext;
// Bound while no view window can be drawn to yet (before it is first shown, or after it is hidden).
static QOffscreenSurface* placeholder;
static EGLSurface placeholderBuffer = EGL_NO_SURFACE;

static void bindPlaceholderBuffer()
{
	// Qt uses EGL_KHR_surfaceless_context for QOffscreenSurface on Android. G3D's
	// initialization clears the default framebuffer, so it needs a real pbuffer
	// until the new view has been exposed and can supply its window surface.
	EGLDisplay display = eglGetCurrentDisplay();
	EGLContext context = eglGetCurrentContext();
	if (placeholderBuffer == EGL_NO_SURFACE) {
		EGLint configId = 0;
		EGLConfig config;
		EGLint count = 0;
		if (!eglQueryContext(display, context, EGL_CONFIG_ID, &configId)) {
			throw std::runtime_error("Unable to query the Android OpenGL context");
		}
		const EGLint configAttributes[] = {EGL_CONFIG_ID, configId, EGL_NONE};
		if (!eglChooseConfig(display, configAttributes, &config, 1, &count) || count != 1) {
			throw std::runtime_error("Unable to find the Android offscreen buffer configuration");
		}
		const EGLint bufferAttributes[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
		placeholderBuffer = eglCreatePbufferSurface(display, config, bufferAttributes);
		if (placeholderBuffer == EGL_NO_SURFACE) {
			throw std::runtime_error("Unable to create the Android offscreen buffer");
		}
	}
	if (eglGetCurrentSurface(EGL_DRAW) != placeholderBuffer &&
		!eglMakeCurrent(display, placeholderBuffer, placeholderBuffer, context)) {
		throw std::runtime_error("Unable to bind the Android offscreen buffer");
	}
}

// The child window a view draws into. Android delivers input to it rather than to the view widget
// under it, so it passes every input event on to that widget, which keeps the client's usual mouse,
// touch and keyboard handling.
class GLSurfaceWindow : public QWindow
{
public:
	GLSurfaceWindow(QWidget* host, const QSurfaceFormat& format) : m_host(host)
	{
		setSurfaceType(QSurface::OpenGLSurface);
		setFormat(format);
	}

	void setContainer(QWidget* container) { m_container = container; }

protected:
	bool event(QEvent* event) override
	{
		switch (event->type()) {
		case QEvent::TouchBegin:
		case QEvent::TouchUpdate:
		case QEvent::TouchEnd:
		case QEvent::TouchCancel:
		case QEvent::KeyPress:
		case QEvent::KeyRelease:
		case QEvent::Wheel:
			// Qt maps touch points and wheel positions to the receiver from their global positions.
			if (m_host) {
				QCoreApplication::sendEvent(m_host, event);
			}
			return true;
		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonRelease:
		case QEvent::MouseButtonDblClick:
		case QEvent::MouseMove: {
			if (!m_host) {
				return true;
			}
			QMouseEvent* mouse = static_cast<QMouseEvent*>(event);
			QPointF position = m_container ? m_container->mapTo(m_host.data(), mouse->position()) : mouse->position();
			QMouseEvent forwarded(
				mouse->type(),
				position,
				mouse->globalPosition(),
				mouse->button(),
				mouse->buttons(),
				mouse->modifiers(),
				mouse->pointingDevice()
			);
			QCoreApplication::sendEvent(m_host, &forwarded);
			return true;
		}
		case QEvent::Expose:
			if (m_host) {
				m_host->update();
			}
			return QWindow::event(event);
		default:
			return QWindow::event(event);
		}
	}

private:
	QPointer<QWidget> m_host;
	QPointer<QWidget> m_container;
};

static QSurfaceFormat surfaceFormat(const G3D::GWindow::Settings& settings)
{
	QSurfaceFormat format;
	format.setRenderableType(QSurfaceFormat::OpenGLES);
	format.setVersion(2, 0);
	format.setRedBufferSize(8);
	format.setGreenBufferSize(8);
	format.setBlueBufferSize(8);
	format.setAlphaBufferSize(settings.alphaBits > 0 ? 8 : 0);
	format.setDepthBufferSize(settings.depthBits >= 24 ? 24 : 16);
	format.setStencilBufferSize(settings.stencilBits);
	format.setSwapInterval(1);
	return format;
}

static void initContext(const QSurfaceFormat& format)
{
	if (glContext) {
		return;
	}
	glContext = new QOpenGLContext;
	glContext->setFormat(format);
	if (!glContext->create()) {
		delete glContext;
		glContext = nullptr;
		throw std::runtime_error("Failed to create an OpenGL ES context");
	}
	placeholder = new QOffscreenSurface;
	placeholder->setFormat(glContext->format());
	placeholder->create();
}

QtGLWindow::QtGLWindow(const G3D::GWindow::Settings& settings, QWidget* host) : m_settings(settings), m_host(host)
{
	QSurfaceFormat format = surfaceFormat(settings);
	initContext(format);

	m_window = new GLSurfaceWindow(host, format);
	m_container = QWidget::createWindowContainer(m_window, host);
	m_window->setContainer(m_container);
	m_container->setFocusPolicy(Qt::NoFocus);
	m_container->setGeometry(0, 0, std::max(1, settings.width), std::max(1, settings.height));
	m_container->show();

	makeCurrent();
	initGLCaps();
}

QtGLWindow::~QtGLWindow()
{
	if (glContext && glContext->surface() == m_window) {
		glContext->makeCurrent(placeholder);
	}
	// Deletes the window with it.
	delete m_container;
}

float QtGLWindow::scale() const
{
	return m_host ? static_cast<float>(m_host->devicePixelRatioF()) : 1.0f;
}

void QtGLWindow::bind() const
{
	QSurface* surface = m_window && m_window->isExposed() ? static_cast<QSurface*>(m_window) : placeholder;
	if (QOpenGLContext::currentContext() != glContext || glContext->surface() != surface) {
		if (!glContext->makeCurrent(surface)) {
			// Android may still be recreating the window after the document picker
			// returns. Initialize/draw offscreen and retry its surface next frame.
			surface = placeholder;
			if (!glContext->makeCurrent(surface)) {
				throw std::runtime_error("Unable to bind the Android OpenGL context");
			}
		}
	}
	if (surface == placeholder) {
		bindPlaceholderBuffer();
	}
}

void QtGLWindow::reallyMakeCurrent() const
{
	bind();
}

void QtGLWindow::getSettings(G3D::GWindow::Settings& settings) const
{
	settings = m_settings;
	settings.width = width();
	settings.height = height();
}

int QtGLWindow::width() const
{
	return std::max(1, static_cast<int>(std::lround(m_settings.width * scale())));
}

int QtGLWindow::height() const
{
	return std::max(1, static_cast<int>(std::lround(m_settings.height * scale())));
}

G3D::Rect2D QtGLWindow::dimensions() const
{
	return G3D::Rect2D::xywh(0, 0, static_cast<float>(width()), static_cast<float>(height()));
}

void QtGLWindow::setDimensions(const G3D::Rect2D& dims)
{
	if (m_container) {
		m_container->move(static_cast<int>(dims.x0()), static_cast<int>(dims.y0()));
	}
	notifyResize(static_cast<int>(dims.width()), static_cast<int>(dims.height()));
}

void QtGLWindow::setPosition(int x, int y)
{
	if (m_container) {
		m_container->move(x, y);
	}
}

bool QtGLWindow::hasFocus() const
{
	return true;
}

std::string QtGLWindow::getAPIVersion() const
{
	return "2.0";
}

std::string QtGLWindow::getAPIName() const
{
	return "OpenGL ES (gl4es)";
}

void QtGLWindow::setGammaRamp(const G3D::Array<G3D::uint16>& gammaRamp)
{
}

void QtGLWindow::setCaption(const std::string& caption)
{
	m_caption = caption;
}

int QtGLWindow::numJoysticks() const
{
	return 0;
}

std::string QtGLWindow::joystickName(unsigned int sticknum)
{
	return std::string();
}

std::string QtGLWindow::caption()
{
	return m_caption;
}

void QtGLWindow::swapGLBuffers()
{
	if (!m_window || !m_window->isExposed() || glContext->surface() != m_window) {
		return;
	}
	// Through gl4es, so the draws it still batches are submitted before the swap.
	glFlush();

	// A TextureView does not block the swap on the display, so frames are paced to its refresh rate
	// here; otherwise the client renders hundreds of frames a second that are never shown.
	using Clock = std::chrono::steady_clock;
	static Clock::time_point due;
	qreal hz = m_window->screen() ? m_window->screen()->refreshRate() : 60.0;
	auto interval =
		std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / std::max<qreal>(hz, 30.0)));
	Clock::time_point now = Clock::now();
	if (now < due) {
		std::this_thread::sleep_until(due);
		now = due;
	}
	due = std::max(due + interval, now);

	glContext->swapBuffers(m_window);
}

void QtGLWindow::notifyResize(int w, int h)
{
	m_settings.width = std::max(1, w);
	m_settings.height = std::max(1, h);
	if (m_container && m_container->size() != QSize(m_settings.width, m_settings.height)) {
		m_container->resize(m_settings.width, m_settings.height);
	}
}

void QtGLWindow::setRelativeMousePosition(double x, double y)
{
	if (m_host) {
		QCursor::setPos(m_host->mapToGlobal(QPoint(static_cast<int>(x), static_cast<int>(y))));
	}
}

void QtGLWindow::setRelativeMousePosition(const G3D::Vector2& p)
{
	setRelativeMousePosition(p.x, p.y);
}

void QtGLWindow::getRelativeMouseState(G3D::Vector2& position, G3D::uint8& mouseButtons) const
{
	int x;
	int y;
	getRelativeMouseState(x, y, mouseButtons);
	position = G3D::Vector2(static_cast<float>(x), static_cast<float>(y));
}

void QtGLWindow::getRelativeMouseState(int& x, int& y, G3D::uint8& mouseButtons) const
{
	QPoint point = m_host ? m_host->mapFromGlobal(QCursor::pos()) : QPoint();
	x = point.x();
	y = point.y();
	Qt::MouseButtons buttons = QGuiApplication::mouseButtons();
	mouseButtons = static_cast<G3D::uint8>(
		((buttons & Qt::LeftButton) ? 1 : 0) | ((buttons & Qt::RightButton) ? 2 : 0) |
		((buttons & Qt::MiddleButton) ? 4 : 0)
	);
}

void QtGLWindow::getRelativeMouseState(double& x, double& y, G3D::uint8& mouseButtons) const
{
	int ix;
	int iy;
	getRelativeMouseState(ix, iy, mouseButtons);
	x = ix;
	y = iy;
}

void QtGLWindow::getJoystickState(unsigned int stickNum, G3D::Array<float>& axis, G3D::Array<bool>& button)
{
}

void QtGLWindow::setInputCapture(bool c)
{
	m_inputCapture = c;
}

bool QtGLWindow::inputCapture() const
{
	return m_inputCapture;
}

void QtGLWindow::setMouseVisible(bool b)
{
	m_mouseVisible = b;
}

bool QtGLWindow::mouseVisible() const
{
	return m_mouseVisible;
}
