#include "Roblox.h"

#include "AboutRobloxDialog.h"
#include "ActiveDocView.h"
#include "BrowserView.h"
#include "ChildFrm.h"
#include "CommandIds.h"
#include "DocView.h"
#include "ErrorUploader.h"
#include "HtmlDialog.h"
#include "LogManager.h"
#include "MainFrm.h"
#include "NullDoc.h"
#include "ObjectBrowser.h"
#include "Printing.h"
#include "Profile.h"
#include "ReflectionMetadata.h"
#include "RenderSettings.h"
#include "Resources.h"
#include "RobloxDoc.h"
#include "RobloxReportView.h"
#include "RobloxView.h"
#include "RobloxWnd.h"
#include "ScriptDoc.h"
#include "SystemMessageBox.h"
#include "VersionInfo.h"
#include "WhidbeyStyle.h"
#include "network/API.h"
#include "script/Script.h"
#include "security/SecurityContext.h"
#include "util/ContentProvider.h"
#include "util/Http.h"
#include "util/Profiling.h"
#include "util/standardout.h"
#include "v8datamodel/DebugSettings.h"
#include "v8datamodel/GameSettings.h"
#include "v8datamodel/GlobalSettings.h"
#include "v8datamodel/factoryregistration.h"

#include <QAbstractEventDispatcher>
#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeData>
#include <QPalette>
#include <QStyleHints>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QXmlStreamReader>

#ifdef __ANDROID__
#include "AndroidAssets.h"
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#include <algorithm>
#include <sstream>

static const unsigned installerQuitStringId = 196;
static const unsigned crashReportTemplateId = 124;
static const unsigned playDocumentResourceId = 129;
static const unsigned browserDocumentResourceId = 185;
static const unsigned objectBrowserResourceId = 138;
static const unsigned scriptDocumentResourceId = 207;
static const int recentFileListSize = 6;
static const unsigned recentFileCommandCount = 16;

static QString g_baseUrl;

static std::unique_ptr<std::runtime_error> g_deferredException;

void deferException(const std::exception& e)
{
	g_deferredException.reset(new std::runtime_error(e.what()));
}

void rethrowDeferredException()
{
	if (g_deferredException) {
		std::runtime_error error(g_deferredException->what());
		g_deferredException.reset();
		throw error;
	}
}

static void failedToCreateDocument()
{
	SystemMessageBox::warning(
		theApp.mainFrame(), Resources::string(appTitleStringId), "Failed to create empty document."
	);
}

class PlayDocumentTemplate : public DocumentTemplate
{
public:
	using DocumentTemplate::DocumentTemplate;

	void removeDocument(Document* document) override;
	Document* playDocument();

private:
	Document* m_playDocument = nullptr;
};

void PlayDocumentTemplate::removeDocument(Document* document)
{
	DocumentTemplate::removeDocument(document);

	if (document == m_playDocument) {
		m_playDocument = nullptr;
	}
}

Document* PlayDocumentTemplate::playDocument()
{
	if (!m_playDocument) {
		g_deferredException.reset();
		Document* document = openDocument(QString(), true);
		try {
			rethrowDeferredException();
		}
		catch (std::exception&) {
			failedToCreateDocument();
			if (document) {
				document->close();
			}
			throw;
		}

		if (!document) {
			if (!failure().empty()) {
				throw std::runtime_error(failure());
			}
			throw std::runtime_error("OpenDocumentFile returned NULL");
		}

		m_playDocument = document;
		return document;
	}

	if (!m_playDocument->saveModified()) {
		throw std::runtime_error("User cancelled");
	}

	setDefaultTitle(m_playDocument);
	if (!m_playDocument->newDocument()) {
		throw std::runtime_error("CDocument::OnNewDocument returned FALSE");
	}

	if (ActiveDocView::activeDocument() == m_playDocument) {
		ActiveDocView::activate(true, ActiveDocView::activeDocument());
	}

	std::vector<View*> views = m_playDocument->views();
	for (View* view : views) {
		g_deferredException.reset();
		view->initialUpdate();
	}

	return m_playDocument;
}

class IDEDocumentManager : public DocumentManager
{
public:
	explicit IDEDocumentManager(bool no3D);

	bool promptFileName(
		QString& fileName, unsigned titleId, unsigned flags, bool open, DocumentTemplate* documentTemplate
	) override;
	void newFile() override;

	PlayDocumentTemplate* playTemplate;
	DocumentTemplate* browserTemplate;
	DocumentTemplate* scriptTemplate;
	DocumentTemplate* objectBrowserTemplate;
};

IDEDocumentManager::IDEDocumentManager(bool no3D)
{
	playTemplate = new PlayDocumentTemplate(
		playDocumentResourceId,
		[]() -> Document* { return new CRobloxDoc; },
		[no3D]() -> View* {
			if (no3D) {
				return new CRobloxReportDocView;
			}
			return new CRobloxView;
		}
	);
	addTemplate(playTemplate);

	browserTemplate = new DocumentTemplate(
		browserDocumentResourceId,
		[]() -> Document* { return new CBrowserDoc; },
		[]() -> View* { return new CIDEBrowserView; }
	);
	addTemplate(browserTemplate);

	scriptTemplate = new DocumentTemplate(
		scriptDocumentResourceId,
		[]() -> Document* { return new CScriptDoc; },
		[]() -> View* { return new CScriptEditor; }
	);
	addTemplate(scriptTemplate);

	objectBrowserTemplate = new DocumentTemplate(
		objectBrowserResourceId,
		[]() -> Document* { return new CNullDoc; },
		[]() -> View* { return new CObjectBrowser; }
	);
	addTemplate(objectBrowserTemplate);
}

bool IDEDocumentManager::promptFileName(
	QString& fileName, unsigned titleId, unsigned flags, bool open, DocumentTemplate* documentTemplate
)
{
	if (documentTemplate == nullptr) {
		documentTemplate = playTemplate;
	}

	return DocumentManager::promptFileName(fileName, titleId, flags, open, documentTemplate);
}

void IDEDocumentManager::newFile()
{
	g_deferredException.reset();
	Document* document = playTemplate->openDocument(QString());
	try {
		rethrowDeferredException();
	}
	catch (std::exception& e) {
		RBX::StandardOut::singleton()->print(RBX::MESSAGE_ERROR, e);
		failedToCreateDocument();
		if (document) {
			document->close();
		}
	}
}

#ifdef _WIN32
class InstallerQuitFilter : public QAbstractNativeEventFilter
{
public:
	bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override
	{
		MSG* msg = static_cast<MSG*>(message);
		if (msg->hwnd == nullptr && msg->message == WM_USER + 2) {
			if (CMainFrame* frame = theApp.mainFrame()) {
				FlashWindow(reinterpret_cast<HWND>(frame->winId()), FALSE);
				SystemMessageBox::warning(
					frame, Resources::string(appTitleStringId), Resources::string(installerQuitStringId)
				);
			}

			QCoreApplication::quit();
			return true;
		}
		return false;
	}
};

static void createAtomWindow()
{
	WNDCLASSA wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = DefWindowProcA;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = GetModuleHandle(nullptr);
	wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = nullptr;
	wc.lpszMenuName = "Menu";
	wc.lpszClassName = "ROBLOX-ATOM";
	RegisterClassA(&wc);

	CreateWindowA(
		wc.lpszClassName,
		"ROBLOX",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		nullptr,
		nullptr,
		wc.hInstance,
		nullptr
	);
}
#endif

static void logProperty(RBX::Reflection::ConstProperty property)
{
	if (property.getDescriptor().hasStringValue()) {
		RBX::Log::current()->writeEntry(
			RBX::Log::Information,
			G3D::format(
				"\t\t%s.%s: %s",
				property.getDescriptor().category.c_str(),
				property.getName().c_str(),
				property.getStringValue().c_str()
			)
				.c_str()
		);
	}
}

static void logSettingsItem(std::shared_ptr<RBX::Instance> instance)
{
	RBX::Log::current()->writeEntry(RBX::Log::Information, G3D::format("\t%s", instance->getName().c_str()).c_str());
	std::for_each(instance->properties_begin(), instance->properties_end(), logProperty);
}

static void logGlobalSettings()
{
	RBX::Log::current()->writeEntry(
		RBX::Log::Information, G3D::format("GlobalSettings ---------------------------------------").c_str()
	);
	RBX::GlobalSettings::singleton()->visitChildren(logSettingsItem);
	RBX::Log::current()->writeEntry(
		RBX::Log::Information, G3D::format("End GlobalSettings -----------------------------------").c_str()
	);
}

static void writeProperty(
	const char* category, std::stringstream* stream, const RBX::Reflection::ConstProperty& property
)
{
	if (property.getDescriptor().category == category) {
		*stream << property.getName().c_str();
		*stream << ':';
		*stream << property.getStringValue();
		*stream << ';';
	}
}

static void postMachineConfiguration()
{
	std::string url((CRobloxApp::baseUrl() + "/Game/MachineConfiguration.ashx").toStdString());
	std::string response;

	try {
		std::stringstream stream;

		RBX::DebugSettings& debugSettings = RBX::DebugSettings::singleton();
		std::for_each(
			debugSettings.properties_begin(),
			debugSettings.properties_end(),
			[&stream](const RBX::Reflection::ConstProperty& property) { writeProperty("Profile", &stream, property); }
		);

		CRenderSettings& renderSettings = CRenderSettings::singleton();
		std::for_each(
			renderSettings.properties_begin(),
			renderSettings.properties_end(),
			[&stream](const RBX::Reflection::ConstProperty& property) { writeProperty("General", &stream, property); }
		);

		stream.flush();
		RBX::Http(url).post(stream, false, response);
	}
	catch (std::exception&) {
	}
}

CRobloxCommandLineInfo::CRobloxCommandLineInfo(int argc, char** argv)
{
	if (argc > 0) {
		std::string commandLine;
		for (int i = 0; i < argc; i++) {
			if (i != 0) {
				commandLine += " ";
			}
			commandLine += argv[i];
		}
		RBX::StandardOut::singleton()->print(RBX::MESSAGE_INFO, "%s", commandLine.c_str());
	}

	for (int i = 1; i < argc; i++) {
		const char* param = argv[i];
#ifdef _WIN32
		bool flag = param[0] == '-' || param[0] == '/';
#else
		bool flag = param[0] == '-';
#endif
		if (flag) {
			param++;
		}
		parseParam(param, flag);
	}

	if (shellCommand == FileNew && !fileName.isEmpty()) {
		shellCommand = FileOpen;
	}
}

void CRobloxCommandLineInfo::parseParam(const char* param, bool flag)
{
	if (m_expectBaseUrl) {
		CRobloxApp::setBaseUrl(QString::fromLocal8Bit(param));
		m_expectBaseUrl = false;
		return;
	}

	if (m_expectScript) {
		scripts.push_back(param);
		m_expectScript = false;
		return;
	}

	if (!flag) {
		if (fileName.isEmpty()) {
			fileName = QString::fromLocal8Bit(param);
		}
		else if (shellCommand == FilePrintTo && printerName.isEmpty()) {
			printerName = QString::fromLocal8Bit(param);
		}
		else if (shellCommand == FilePrintTo && driverName.isEmpty()) {
			driverName = QString::fromLocal8Bit(param);
		}
		else if (shellCommand == FilePrintTo && portName.isEmpty()) {
			portName = QString::fromLocal8Bit(param);
		}
		return;
	}

	QString name = QString::fromLocal8Bit(param);
	auto is = [&name](const char* value) { return name.compare(QLatin1String(value), Qt::CaseInsensitive) == 0; };

	if (is("No3D")) {
		no3D = true;
	}
	else if (is("Browser")) {
		browser = true;
	}
	else if (is("BaseUrl")) {
		m_expectBaseUrl = true;
	}
	else if (is("Script")) {
		m_expectScript = true;
	}
	else if (is("pt")) {
		shellCommand = FilePrintTo;
	}
	else if (is("p")) {
		shellCommand = FilePrint;
	}
	else if (is("Unregister") || is("Unregserver")) {
		shellCommand = AppUnregister;
	}
	else if (is("Register") || is("Regserver")) {
		shellCommand = AppRegister;
	}
	else if (is("dde")) {
		shellCommand = FileDDE;
	}
	else if (is("Embedding")) {
		runEmbedded = true;
	}
	else if (is("Automation")) {
		runAutomated = true;
	}
}

QString CRobloxApp::baseUrl()
{
	return g_baseUrl;
}

void CRobloxApp::setBaseUrl(const QString& url)
{
	g_baseUrl = url;
}

CRobloxApp::CRobloxApp()
{
	addCommand(ID_FILE_NEW, [this]() { m_documentManager->newFile(); });
	addCommand(ID_FILE_OPEN, [this]() { m_documentManager->openFile(); });
	addCommand(ID_APP_EXIT, [this]() { m_mainFrame->close(); });
	addCommand(ID_FILE_PRINT_SETUP, []() { Printing::filePrintSetup(); });
	addCommand(ID_APP_ABOUT, [this]() {
		CAboutRobloxDialog dialog(m_mainFrame);
		dialog.DoModal();
	});
	addCommand(ID_HELP_OBJECT_BROWSER, [this]() { m_documentManager->objectBrowserTemplate->openDocument(QString()); });
	addCommandUpdate(ID_HELP_OBJECT_BROWSER, [](CommandState& state) { state.enabled = true; });
	addCommand(ID_HELP, []() { QDesktopServices::openUrl(QUrl("http://wiki.roblox.com")); });
	addCommandUpdate(ID_HELP, [](CommandState& state) { state.enabled = true; });
	addCommand(ID_TOOLS_EXECUTE_SCRIPT, [this]() {
		if (Document* document = m_documentManager->playTemplate->openDocument(QString())) {
			static_cast<CRobloxDoc*>(document)->promptExecuteScript();
		}
	});
	addCommand(ID_TOOLS_REPORT_CRASHES, [this]() { reportCrashes(); });

	for (unsigned i = 0; i < recentFileCommandCount; i++) {
		addCommand(ID_FILE_MRU_FILE1 + i, [this, i]() { openRecentFile(i); });
	}
	addCommandUpdate(ID_FILE_MRU_FILE1, [this](CommandState& state) { state.enabled = !m_recentFiles.empty(); });
}

CRobloxApp::~CRobloxApp() = default;

bool CRobloxApp::showMainFrame(bool gameLayout)
{
	if (m_mainFrame != nullptr) {
		if (!m_mainFrame->isVisible()) {
			m_mainFrame->restorePlacement(true);
		}
		return true;
	}

	m_mainFrame = new CMainFrame;
	if (!m_mainFrame->create()) {
		return false;
	}

	if (gameLayout) {
		m_mainFrame->enterGameLayout();
	}
	else {
		m_mainFrame->leaveGameLayout();
	}
#ifdef __ANDROID__
	// The saved full screen setting was applied while the window was built; the layout chosen above
	// replaced it, so apply it again.
	if (m_mainFrame->isFullScreenMode()) {
		m_mainFrame->setFullScreen(true);
	}
#endif

	m_mainFrame->setAcceptDrops(true);
	m_mainFrame->restorePlacement(true);
	return true;
}

void CRobloxApp::openStartPage()
{
	m_documentManager->browserTemplate->openDocument(QString());
}

void CRobloxApp::openScriptDocument(std::shared_ptr<RBX::Script> script)
{
	CScriptDoc* document = static_cast<CScriptDoc*>(m_documentManager->scriptTemplate->openDocument(QString()));
	document->setScript(script);
}

CRobloxDoc* CRobloxApp::playDocument()
{
	return static_cast<CRobloxDoc*>(m_documentManager->playTemplate->playDocument());
}

bool CRobloxApp::executeScript(const std::string& script)
{
	std::vector<Document*> documents = m_documentManager->playTemplate->documents();
	return std::all_of(documents.begin(), documents.end(), [&script](Document* document) {
		return static_cast<CRobloxDoc*>(document)->executeScriptFile(script);
	});
}

bool CRobloxApp::executeScripts(const std::vector<std::string>& scripts)
{
	return std::all_of(scripts.begin(), scripts.end(), [this](const std::string& script) {
		return executeScript(script);
	});
}

void CRobloxApp::loadRecentFileList(int size)
{
	m_recentFileListSize = size;
	for (int i = 1; i <= size; i++) {
		QString file = Profile::GetProfileString("Recent File List", QStringLiteral("File%1").arg(i));
		if (!file.isEmpty()) {
			m_recentFiles.push_back(file);
		}
	}
}

void CRobloxApp::addToRecentFileList(const QString& pathName)
{
	if (m_recentFileListSize == 0) {
		return;
	}

	QString path = QDir::toNativeSeparators(pathName);
	std::erase_if(m_recentFiles, [&path](const QString& file) { return file.compare(path, Qt::CaseInsensitive) == 0; });
	m_recentFiles.insert(m_recentFiles.begin(), path);
	if (static_cast<int>(m_recentFiles.size()) > m_recentFileListSize) {
		m_recentFiles.resize(m_recentFileListSize);
	}

	for (int i = 0; i < m_recentFileListSize; i++) {
		Profile::WriteProfileString(
			"Recent File List",
			QStringLiteral("File%1").arg(i + 1),
			i < static_cast<int>(m_recentFiles.size()) ? m_recentFiles[i] : QString()
		);
	}
}

void CRobloxApp::openRecentFile(unsigned index)
{
	if (index >= m_recentFiles.size()) {
		return;
	}

	QString file = m_recentFiles[index];
	if (!m_documentManager->openDocument(file)) {
		m_recentFiles.erase(m_recentFiles.begin() + index);
	}
}

static QString abbreviateName(const QString& canon, int cchMax, bool atLeastName)
{
	QString fileName = QFileInfo(canon).fileName();
	int cchFullPath = canon.size();
	int cchFileName = fileName.size();

	if (cchMax >= cchFullPath) {
		return canon;
	}

	if (cchMax < cchFileName) {
		return atLeastName ? fileName : QString();
	}

	QChar separator = QDir::separator();
	qsizetype current;
	if (canon.startsWith(QStringLiteral("\\\\"))) {
		current = canon.indexOf(separator, 2);
	}
	else if (canon.startsWith(separator)) {
		current = 0;
	}
	else {
		current = 2;
	}

	if (current < 0) {
		return fileName;
	}

	if (cchFullPath - cchFileName > current + 1) {
		qsizetype next = canon.indexOf(separator, current + 1);
		if (next >= 0) {
			current = next;
		}
	}

	int cchVolName = static_cast<int>(current);
	if (cchMax < cchVolName + 5 + cchFileName) {
		return fileName;
	}

	while (cchVolName + 4 + (cchFullPath - current) > cchMax) {
		qsizetype next = canon.indexOf(separator, current + 1);
		if (next < 0) {
			break;
		}
		current = next;
	}

	return canon.left(cchVolName) + separator + "..." + canon.mid(current);
}

std::vector<QString> CRobloxApp::recentFileDisplayNames() const
{
	QString currentDirectory = QDir::toNativeSeparators(QDir::currentPath());
	std::vector<QString> names;
	for (const QString& file : m_recentFiles) {
		QFileInfo info(file);
		if (QDir::toNativeSeparators(info.absolutePath()).compare(currentDirectory, Qt::CaseInsensitive) == 0) {
			names.push_back(info.fileName());
		}
		else {
			names.push_back(abbreviateName(file, 30, true));
		}
	}
	return names;
}

void CRobloxApp::loadSettings()
{
	loadRecentFileList(recentFileListSize);

#ifdef __ANDROID__
	AndroidAssets::install();
	if (!AndroidAssets::hasContent() && !AndroidAssets::requestContent()) {
		std::exit(0);
	}
	QString directory = QString::fromStdString(AndroidAssets::dataDirectory());
#else
	QString directory = QCoreApplication::applicationDirPath();
#endif
	QFile file(directory + "/AppSettings.xml");
	QString contentFolder;
#ifdef _WIN32
	bool silentCrashReport = true;
	bool hasSilentCrashReport = false;
#endif
	if (file.open(QIODevice::ReadOnly)) {
		QXmlStreamReader reader(&file);
		if (reader.readNextStartElement() && reader.name() == QStringLiteral("Settings")) {
			while (reader.readNextStartElement()) {
#ifdef _WIN32
				if (reader.name() == QStringLiteral("SilentCrashReport")) {
					QString value = reader.readElementText();
					hasSilentCrashReport = true;
					silentCrashReport = value.compare("true", Qt::CaseInsensitive) == 0 || value == "1";
					continue;
				}
#endif
				if (reader.name() == QStringLiteral("BaseUrl")) {
					QString value = reader.readElementText();
					if (g_baseUrl.isEmpty()) {
						g_baseUrl = value;
					}
				}
				else if (reader.name() == QStringLiteral("ContentFolder")) {
					contentFolder = reader.readElementText();
				}
				else {
					reader.skipCurrentElement();
				}
			}
		}
	}

#ifdef _WIN32
	if (!hasSilentCrashReport || silentCrashReport) {
		silentCrashReport = Profile::GetProfileInt(QString(), "SilentCrashReport", 0) != 0;
	}
	RobloxCrashReporter::silent = silentCrashReport;

	if (g_baseUrl.isEmpty()) {
		HKEY key;
		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Roblox", 0, KEY_READ, &key) == ERROR_SUCCESS) {
			char value[1024];
			DWORD length = sizeof(value);
			DWORD type;
			if (RegQueryValueExA(key, "BaseUrl", nullptr, &type, reinterpret_cast<LPBYTE>(value), &length) ==
					ERROR_SUCCESS &&
				type == REG_SZ) {
				g_baseUrl = QString::fromLocal8Bit(value);
			}
			RegCloseKey(key);
		}
	}
#endif

	if (!contentFolder.isEmpty()) {
		QString assetFolder = QDir::toNativeSeparators(QDir(directory).filePath(contentFolder));
		RBX::ContentProvider::singleton().setAssetFolder(assetFolder.toLocal8Bit().constData());
	}

	CRenderSettings::singleton();
	RBX::DebugSettings::singleton();
	RBX::GameSettings::singleton();
	RBX::GlobalSettings::singleton()->loadState();
	logGlobalSettings();
}

void CRobloxApp::reportCrashes()
{
	if (m_logManager->hasCrashLogs()) {
		switch (RBX::DebugSettings::singleton().getErrorReporting()) {
		case RBX::DebugSettings::Prompt: {
			CRobloxDHtmlDialog dialog(crashReportTemplateId, baseUrl() + "/Error/ReportDialog.aspx", nullptr);
			dialog.DoModal();
			break;
		}
		case RBX::DebugSettings::Report:
			m_errorUploader->Upload((baseUrl() + "/Error/Dmp.ashx").toStdString(), false);
			break;
		default:
			break;
		}
	}
}

bool CRobloxApp::promptFileName(
	QString& fileName, unsigned titleId, unsigned flags, bool open, DocumentTemplate* documentTemplate
)
{
	return m_documentManager->promptFileName(fileName, titleId, flags, open, documentTemplate);
}

bool CRobloxApp::saveAllModified()
{
	return m_documentManager ? m_documentManager->saveAllModified() : true;
}

void CRobloxApp::closeAllDocuments()
{
	if (m_documentManager) {
		m_documentManager->closeAllDocuments();
	}
}

bool CRobloxApp::processShellCommand(const CRobloxCommandLineInfo& commandLine)
{
	switch (commandLine.shellCommand) {
	case CRobloxCommandLineInfo::FileNew:
		m_documentManager->newFile();
		return m_mainFrame != nullptr;
	case CRobloxCommandLineInfo::FileOpen:
		return m_documentManager->openDocument(commandLine.fileName) != nullptr;
	case CRobloxCommandLineInfo::FilePrintTo:
	case CRobloxCommandLineInfo::FilePrint:
		m_mainFrame->hide();
		if (m_documentManager->openDocument(commandLine.fileName)) {
			m_commandLineInfo = &commandLine;
			m_mainFrame->sendCommand(ID_FILE_PRINT_DIRECT);
			m_commandLineInfo = nullptr;
		}
		return false;
	default:
		return true;
	}
}

bool CRobloxApp::initInstance(int argc, char** argv)
{
	try {
		RBX::StandardOut::singleton()->print(RBX::MESSAGE_INFO, "ROBLOX InitInstance");

		m_errorUploader.reset(new ErrorUploader());

		m_version = fileVersionString();

#ifdef _WIN32
		{
			DWORD crashReport = 1;
			HKEY key;
			if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\ROBLOX Corporation\\Roblox", 0, KEY_READ, &key) ==
				ERROR_SUCCESS) {
				DWORD size = sizeof(crashReport);
				DWORD type;
				RegQueryValueExA(key, "CrashReport", nullptr, &type, reinterpret_cast<LPBYTE>(&crashReport), &size);
				RegCloseKey(key);
			}
			if (crashReport) {
				m_logManager->WriteCrashDump();
			}
		}

		createAtomWindow();
#endif

		CRobloxCommandLineInfo commandLine(argc, argv);
#ifdef __ANDROID__
		// Started from the launcher: there is no website to pick a place, so play the bundled one.
		if (commandLine.shellCommand == CRobloxCommandLineInfo::FileNew && commandLine.fileName.isEmpty() &&
			commandLine.scripts.empty()) {
			QString places = QString::fromStdString(AndroidAssets::dataDirectory()) + "/places/";
			commandLine.shellCommand = CRobloxCommandLineInfo::FileOpen;
			commandLine.fileName = places + "obby.rbxl";
			commandLine.scripts.push_back((places + "solo.lua").toStdString());
		}
#endif
		if (commandLine.shellCommand == CRobloxCommandLineInfo::AppUnregister ||
			commandLine.shellCommand == CRobloxCommandLineInfo::AppRegister) {
			return false;
		}

		RBX::Profiling::init(true);
		RBX::Reflection::Metadata::registerClasses();

		static RBX::FactoryRegistrator registrator;
		RBX::Network::API::init(m_version.c_str());

		loadSettings();
		postMachineConfiguration();

		if (CRenderSettings::singleton().getLatchedGraphicsMode() == CRenderSettings::c_graphicsModeNoGraphics) {
			commandLine.no3D = true;
		}

		m_documentManager.reset(new IDEDocumentManager(commandLine.no3D));
		if (!showMainFrame(false)) {
			return false;
		}

		if (commandLine.shellCommand == CRobloxCommandLineInfo::FileNew) {
			if (commandLine.scripts.size() > 0) {
				processShellCommand(commandLine);
				return executeScripts(commandLine.scripts);
			}
			if (!commandLine.no3D) {
				openStartPage();
			}
			return true;
		}

		if (commandLine.shellCommand == CRobloxCommandLineInfo::FileOpen) {
			return processShellCommand(commandLine) && executeScripts(commandLine.scripts);
		}

		return processShellCommand(commandLine);
	}
	catch (std::exception& e) {
		RBX::StandardOut::singleton()->print(RBX::MESSAGE_ERROR, e);
		SystemMessageBox::critical(nullptr, Resources::string(appTitleStringId), e.what());
	}

	return false;
}

void CRobloxApp::idle()
{
	if (m_idle) {
		return;
	}
	m_idle = true;

	if (m_mainFrame) {
		m_mainFrame->updateCommandUI();
	}
	scheduleFrame();

	m_idle = false;
}

void CRobloxApp::scheduleFrame()
{
	if (m_frameTimer && !m_frameTimer->isActive() && CRobloxWnd::hasActiveWindow()) {
		m_frameTimer->start(0);
	}
}

void CRobloxApp::renderFrame()
{
	double wait = 0.0;
	if (CRobloxWnd::renderIdle(wait)) {
		m_frameTimer->start(wait > 0.0 ? static_cast<int>(wait * 1000.0) + 1 : 0);
	}
}

int CRobloxApp::run(int argc, char** argv)
{
	QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
#ifdef _WIN32
	qputenv("QT_USE_SYSTEM_LINE_METRICS", "1");
	static char platformName[] = "windows:fontengine=gdi";
#elif !defined(__ANDROID__)
	static char platformName[] = "xcb";
#endif
	static std::vector<char*> arguments(argv, argv + argc);
#ifdef __ANDROID__
	// Each 3D view draws into a child window. Qt gives a child of the only top-level window an Android
	// SurfaceView, which is composited behind the activity's window; a TextureView stacks normally.
	qputenv("QT_ANDROID_SURFACE_CONTAINER_TYPE", "1");
#endif
#ifndef __ANDROID__
	// Android has a single platform plugin, which Qt selects itself.
	static char platformOption[] = "-platform";
	arguments.push_back(platformOption);
	arguments.push_back(platformName);
#endif
	static int argumentCount = static_cast<int>(arguments.size());
	m_application.reset(new QApplication(argumentCount, arguments.data()));
	QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
	WhidbeyStyle::setPopupShadowsEnabled(false);
	QApplication::setEffectEnabled(Qt::UI_AnimateMenu, false);
	QApplication::setEffectEnabled(Qt::UI_FadeMenu, false);
	QApplication::setStyle(new WhidbeyStyle);
	QApplication::setPalette(WhidbeyStyle::systemPalette());
	QApplication::setQuitOnLastWindowClosed(false);
	m_logManager.reset(new MainLogManager("Roblox"));

#ifdef _WIN32
	InstallerQuitFilter installerQuitFilter;
	m_application->installNativeEventFilter(&installerQuitFilter);
#endif

	m_frameTimer.reset(new QTimer);
	m_frameTimer->setSingleShot(true);
	m_frameTimer->setTimerType(Qt::PreciseTimer);
	QObject::connect(m_frameTimer.get(), &QTimer::timeout, m_frameTimer.get(), [this]() { renderFrame(); });
#ifdef __ANDROID__
	QObject::connect(
		m_application.get(),
		&QGuiApplication::applicationStateChanged,
		m_frameTimer.get(),
		[this](Qt::ApplicationState state) {
			if (state == Qt::ApplicationActive) {
				scheduleFrame();
			}
		}
	);
#endif
	QObject::connect(
		QAbstractEventDispatcher::instance(), &QAbstractEventDispatcher::aboutToBlock, m_application.get(), [this]() {
			idle();
		}
	);

	int result = 0;
	if (initInstance(argumentCount, arguments.data())) {
		RBX::Security::Impersonator impersonate(RBX::Security::LocalGUI);
		result = QApplication::exec();
	}
	RBX::StandardOut::singleton()->print(RBX::MESSAGE_INFO, "ROBLOX ExitInstance");

	m_frameTimer.reset();
	closeAllDocuments();
	delete m_mainFrame;
	m_mainFrame = nullptr;
	m_documentManager.reset();
#ifdef _WIN32
	m_application->removeNativeEventFilter(&installerQuitFilter);
#endif
	m_application.reset();
	return result;
}

template <class Function>
static void runOnGuiThread(QObject* context, Function function)
{
	if (QThread::currentThread() == context->thread()) {
		function();
	}
	else {
		QMetaObject::invokeMethod(context, function, Qt::BlockingQueuedConnection);
	}
}

bool CRobloxApp::clipboardHasFormat(const char* format)
{
	bool result = false;
	runOnGuiThread(m_application.get(), [&]() {
		const QMimeData* data = QGuiApplication::clipboard()->mimeData();
		result = data && data->hasFormat(format);
	});
	return result;
}

bool CRobloxApp::readClipboard(const char* format, std::string& text)
{
	bool result = false;
	runOnGuiThread(m_application.get(), [&]() {
		const QMimeData* data = QGuiApplication::clipboard()->mimeData();
		if (data && data->hasFormat(format)) {
			text = data->data(format).toStdString();
			result = true;
		}
	});
	return result;
}

void CRobloxApp::writeClipboard(const char* format, const std::string& text)
{
	runOnGuiThread(m_application.get(), [&]() {
		QMimeData* data = new QMimeData;
		data->setData(format, QByteArray::fromStdString(text));
		QGuiApplication::clipboard()->setMimeData(data);
	});
}

CRobloxApp theApp;

int main(int argc, char** argv)
{
	return theApp.run(argc, argv);
}
