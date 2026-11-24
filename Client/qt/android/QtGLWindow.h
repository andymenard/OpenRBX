#ifndef QTGLWINDOW_H
#define QTGLWINDOW_H

#include <G3D/Rect2D.h>
#include <GLG3D/GWindow.h>
#include <QPointer>

class GLSurfaceWindow;
class QWidget;

// A G3D window for Android. The client's desktop OpenGL calls reach OpenGL ES 2.0 through gl4es,
// which tracks GL state for one context only, so every view shares one Qt OpenGL context. Each view
// draws straight into its own child window, which Qt backs with a TextureView so it stacks with the
// rest of the activity's views (see Roblox.cpp). G3D renders at the screen's full resolution; the
// client's widget units are scaled to it by devicePixelRatio().
class QtGLWindow : public G3D::GWindow
{
public:
	QtGLWindow(const G3D::GWindow::Settings& settings, QWidget* host);
	~QtGLWindow() override;

	void getSettings(G3D::GWindow::Settings& settings) const override;
	// In render-device pixels.
	int width() const override;
	int height() const override;
	G3D::Rect2D dimensions() const override;
	void setDimensions(const G3D::Rect2D& dims) override;
	void setPosition(int x, int y) override;
	bool hasFocus() const override;
	std::string getAPIVersion() const override;
	std::string getAPIName() const override;
	void setGammaRamp(const G3D::Array<G3D::uint16>& gammaRamp) override;
	void setCaption(const std::string& caption) override;
	int numJoysticks() const override;
	std::string joystickName(unsigned int sticknum) override;
	std::string caption() override;
	void swapGLBuffers() override;
	// In widget units, like the rest of the client.
	void notifyResize(int w, int h) override;
	void setRelativeMousePosition(double x, double y) override;
	void setRelativeMousePosition(const G3D::Vector2& p) override;
	void getRelativeMouseState(G3D::Vector2& position, G3D::uint8& mouseButtons) const override;
	void getRelativeMouseState(int& x, int& y, G3D::uint8& mouseButtons) const override;
	void getRelativeMouseState(double& x, double& y, G3D::uint8& mouseButtons) const override;
	void getJoystickState(unsigned int stickNum, G3D::Array<float>& axis, G3D::Array<bool>& button) override;
	void setInputCapture(bool c) override;
	bool inputCapture() const override;
	void setMouseVisible(bool b) override;
	bool mouseVisible() const override;

	// Qt makes its own contexts current on the GUI thread between frames, which G3D's current-window
	// tracking does not see; this binds the view's surface and the shared context again if so.
	void bind() const;
	// Render-device pixels per widget unit.
	float scale() const;

protected:
	void reallyMakeCurrent() const override;

private:
	G3D::GWindow::Settings m_settings;
	QPointer<QWidget> m_host;
	QPointer<QWidget> m_container;
	GLSurfaceWindow* m_window = nullptr;
	std::string m_caption;
	bool m_inputCapture = false;
	bool m_mouseVisible = true;
};

#endif // QTGLWINDOW_H
