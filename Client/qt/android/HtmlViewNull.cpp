#include "External.h"
#include "HtmlView.h"
#include "InstanceExplorer.h"
#include "Resources.h"

#include <QDesktopServices>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QResizeEvent>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

// Android has no embeddable browser engine in Qt Widgets. Pages bundled with the client are shown
// as static HTML; scripts and window.external do not run, and other pages open in the system
// browser after the view's navigation hooks have seen them.

struct CHtmlView::Engine
{
	QTextBrowser* browser = nullptr;
};

QString CHtmlView::quoteScriptString(const QString& text)
{
	QString json = QString::fromUtf8(QJsonDocument(QJsonArray{text}).toJson(QJsonDocument::Compact));
	return json.mid(1, json.size() - 2);
}

CHtmlView::CHtmlView(QWidget* parent) : View(parent), m_host(new QWidget(this)), m_busy(false), m_customGeometry(false)
{
	m_host->setBackgroundRole(QPalette::Base);
	m_host->setAutoFillBackground(true);
}

CHtmlView::~CHtmlView()
{
	destroyBrowser();
}

void CHtmlView::destroyBrowser()
{
	if (m_engine) {
		delete m_engine->browser;
		m_engine->browser = nullptr;
	}
	m_ready = false;
}

bool CHtmlView::eventFilter(QObject* watched, QEvent* event)
{
	return View::eventFilter(watched, event);
}

void CHtmlView::SetBrowserGeometry(const QRect& rect)
{
	m_customGeometry = true;
	m_host->setGeometry(rect);
}

void CHtmlView::resizeEvent(QResizeEvent* event)
{
	View::resizeEvent(event);
	if (!m_customGeometry) {
		m_host->setGeometry(rect());
	}
}

std::shared_ptr<CExternal> CHtmlView::OnGetExternal()
{
	return nullptr;
}

void CHtmlView::OnBeforeNavigate2(const QString& url, const QString& targetFrameName, bool* cancel)
{
	if (!isTrustedUrl(url.toStdString().c_str())) {
		*cancel = true;
	}
}

void CHtmlView::OnNavigateComplete2(const QString& url)
{
}

void CHtmlView::OnDocumentComplete(const QString& url)
{
}

void CHtmlView::OnTitleChange(const QString& text)
{
}

void CHtmlView::OnCommandStateChange(int command, bool enable)
{
}

void CHtmlView::OnWindowClosing(const QString& dialogResult)
{
}

void CHtmlView::OnBrowserFocus()
{
}

void CHtmlView::OnHostEvent(const QString& name, const QJsonValue& value)
{
}

void CHtmlView::SetScriptsEnabled(bool enabled)
{
	m_scriptsEnabled = enabled;
}

void CHtmlView::InstallBridge()
{
}

void CHtmlView::ResetExternal()
{
	m_externalObjects.reset();
}

QByteArray CHtmlView::OnBridgeRequest(const QByteArray& request)
{
	return "{\"error\":\"window.external is not available\"}";
}

bool CHtmlView::create()
{
	m_engine.reset(new Engine);
	m_engine->browser = new QTextBrowser(m_host);
	m_engine->browser->setOpenLinks(false);
	m_engine->browser->setFrameShape(QFrame::NoFrame);
	QVBoxLayout* layout = new QVBoxLayout(m_host);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(m_engine->browser);
	QObject::connect(m_engine->browser, &QTextBrowser::anchorClicked, this, [this](const QUrl& url) {
		Navigate(url.toString());
	});
	m_ready = true;

	if (!m_pendingHtml.isEmpty()) {
		m_engine->browser->setHtml(m_pendingHtml);
		m_pendingHtml.clear();
	}
	else if (!m_pendingUrl.isEmpty()) {
		QString url = m_pendingUrl;
		m_pendingUrl.clear();
		Navigate(url);
	}
	return true;
}

void CHtmlView::Navigate(const QString& url, const QString& targetFrameName)
{
	if (!m_ready) {
		m_pendingUrl = url;
		return;
	}

	bool cancel = false;
	OnBeforeNavigate2(url, targetFrameName, &cancel);
	if (cancel) {
		return;
	}

	QUrl target(url);
	if (target.scheme() == "http" || target.scheme() == "https" || targetFrameName == "_blank") {
		QDesktopServices::openUrl(target);
		return;
	}
	m_locationUrl = url;
	OnNavigateComplete2(url);
	OnDocumentComplete(url);
}

void CHtmlView::LoadFromResource(unsigned id)
{
	QFile file(Resources::htmlPath(id));
	if (!file.open(QIODevice::ReadOnly)) {
		return;
	}

	QString html = QString::fromLatin1(file.readAll());
	if (!m_ready) {
		m_pendingHtml = html;
		return;
	}
	m_engine->browser->setHtml(html);
}

void CHtmlView::ExecuteScript(const QString& script)
{
}

void CHtmlView::GoBack()
{
}

void CHtmlView::GoForward()
{
}

void CHtmlView::Refresh()
{
}

void CHtmlView::Stop()
{
}
