#include "MainFrm.h"

#include "ActiveDocView.h"
#include "ChildFrm.h"
#include "ColorSelector.h"
#include "CommandLineComboBox.h"
#include "CustomizeSheet.h"
#include "DisplayMode.h"
#include "DocView.h"
#include "ExplorerPane.h"
#include "ImageEditorDialog.h"
#include "ImageManager.h"
#include "LayoutImport.h"
#include "MenuEditItem.h"
#include "MenuExpandButton.h"
#include "MenuIconGrid.h"
#include "PaneCaption.h"
#include "PopupToolBar.h"
#include "Profile.h"
#include "PropGrid.h"
#include "RenderSettings.h"
#include "Resources.h"
#include "Roblox.h"
#include "RobloxReportView.h"
#include "SettingsDialog.h"
#include "StandardOutputView.h"
#include "StatusBar.h"
#include "TearOffGripper.h"
#include "UserInput.h"
#include "WhidbeyStyle.h"
#include "WorkspaceTabButtons.h"
#include "util/FileSystem.h"
#include "util/Log.h"
#include "util/scopedassign.h"
#include "v8datamodel/Accoutrement.h"
#include "v8datamodel/ToolsPart.h"

#include <G3D/Color3uint8.h>
#include <GLG3D/Texture.h>
#include <GLG3D/VARArea.h>
#include <QAbstractNativeEventFilter>
#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QChildEvent>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDir>
#include <QDockWidget>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMenuBar>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QScreen>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QToolTip>
#include <QWidgetAction>
#include <QtEndian>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#elif !defined(__ANDROID__)
#include "NativeInput.h"

#include <xcb/xcb.h>
#endif

static QString escapeMnemonics(QString text)
{
	return text.replace(QLatin1Char('&'), QLatin1String("&&"));
}

bool g_overrideLockGameLayout;

#if !defined(_WIN32) && !defined(__ANDROID__)
class AcquiredKeyFilter : public QAbstractNativeEventFilter
{
public:
	explicit AcquiredKeyFilter(QWidget* frame) : m_frame(frame) {}

	bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override
	{
		const xcb_generic_event_t* event = static_cast<const xcb_generic_event_t*>(message);
		unsigned type = event->response_type & ~0x80;
		if ((type != XCB_KEY_PRESS && type != XCB_KEY_RELEASE) || !UserInput::isKeyboardAcquired()) {
			return false;
		}

		const xcb_key_press_event_t* key = reinterpret_cast<const xcb_key_press_event_t*>(event);
		if (!(event->response_type & 0x80) && key->event != m_frame->winId()) {
			NativeInput::forwardKey(
				m_frame->winId(),
				type == XCB_KEY_PRESS,
				key->detail,
				key->state,
				key->time,
				QPoint(key->root_x, key->root_y)
			);
		}
		return true;
	}

private:
	QWidget* m_frame;
};
#endif

static const unsigned indicators[] = {
	0, ID_INDICATOR_TIME, ID_INDICATOR_FPS, ID_INDICATOR_TEXTURE_MEMORY, ID_INDICATOR_VAR_MEMORY
};

static const unsigned mouseToolBarId = 135;
static const unsigned testToolBarId = 137;
static const unsigned cameraToolBarId = 145;
static const unsigned commandToolBarId = 150;
static const unsigned undoMenuId = 139;
static const unsigned redoMenuId = 140;
static const unsigned idleMessageStringId = 57345;
static const unsigned recentFileCount = 16;
static const int layoutVersion = 4;
static const char* const menuBarTitle = "Menu Bar";
static const unsigned xtpAddOrRemoveStringId = 9122;
static const unsigned xtpCustomizeStringId = 35001;
static const unsigned customizeMenuId = 9121;
static const unsigned customizeIconsBitmapId = 9103;
static const unsigned dragCopyCursorId = 9105;
static const unsigned dragMoveCursorId = 9106;
static const unsigned dragDeleteCursorId = 9107;
static const int customIconSize = 16;
static const int editImageSize = 16;
static const QRgb customIconMask = 0xff00ff00;
static const unsigned customizeResetId = 9123;
static const unsigned customizeDeleteId = 9124;
static const unsigned customizeNameId = 9125;
static const unsigned customizeCopyImageId = 9126;
static const unsigned customizePasteImageId = 9127;
static const unsigned customizeResetImageId = 9128;
static const unsigned customizeEditImageId = 9129;
static const unsigned customizeChangeImageId = 9130;
static const unsigned customizeDefaultStyleId = 9131;
static const unsigned customizeImageAndTextId = 9132;
static const unsigned customizeTextOnlyId = 9133;
static const unsigned customizeBeginGroupId = 9134;
static const Qt::Corner dockCorners[] = {
	Qt::TopLeftCorner, Qt::TopRightCorner, Qt::BottomLeftCorner, Qt::BottomRightCorner
};
static const int defaultIconSize = 16;
static const int largeIconSize = 32;
static const int expandDelay = 4500;
static const int menuReplayInterval = 100;
static const unsigned firstCustomBarId = 0xe800;
static const unsigned hiddenCommands[] = {
	0xe100, 0xe102, 0x8025, 0x80b7, 0xe123, 0xe120, 0x8005, 0x8010, 0x8011, 0x805d, 0x805e, 0xe800, 0x88b9,
	0xe801, 0x8042, 0x8018, 0x800f, 0x809f, 0x80bf, 0x803d, 0x8061, 0x80a5, 0x8013, 0x8014, 0x8047, 0xe130,
};
static const int windowListItemCount = 9;
static const unsigned windowListPromptStringId = 61215;
static const int iconDirectorySize = 6;
static const int iconEntrySize = 16;
static const int indicatorPadding = 6;
static const int statusBarHeight = 20;
static const unsigned menuBarId = 1;
static const int emptyLabelWidth = 22;

static bool isRecentFileCommand(unsigned id)
{
	return id >= ID_FILE_MRU_FILE1 && id < ID_FILE_MRU_FILE1 + recentFileCount;
}

CMainFrame::CMainFrame()
{
	g_overrideLockGameLayout = Profile::GetProfileInt(QString(), "OverrideLockViewGamelayout", 0) == 1;
	RBX::Accoutrement::writeUnlocked = g_overrideLockGameLayout;

#if !defined(_WIN32) && !defined(__ANDROID__)
	m_acquiredKeyFilter = std::make_unique<AcquiredKeyFilter>(this);
	QCoreApplication::instance()->installNativeEventFilter(m_acquiredKeyFilter.get());
#endif

	std::string directory = RBX::FileSystem::getUserAppDataDirectory(true, nullptr);
	if (directory.size() > 0) {
		m_layoutFile = QDir(QString::fromStdString(directory)).filePath("IDESettings.ini");
	}

	RBX::FillTool::color.addListener(this);

	addCommand(ID_VIEW_OUTPUT, [this]() { showPane(PANE_OUTPUT, Qt::BottomDockWidgetArea, QSize(640, 200)); });
	addCommand(ID_TOOL_TOOLBOX_PANE, [this]() { showPane(PANE_TOOLBOX, Qt::LeftDockWidgetArea, QSize(280, 400)); });
	addCommand(ID_VIEW_EXPLORER, [this]() { showExplorerPane(); });
	addCommand(ID_VIEW_PROPERTIES, [this]() { showPropertiesPane(); });
	addCommand(ID_VIEW_REPORT, [this]() { showPane(PANE_REPORT, Qt::LeftDockWidgetArea, QSize(150, 400)); });
	addCommand(ID_TOOLS_SETTINGS, [this]() { CSettingsDialog::show(this); });
	addCommand(ID_VIEW_START_PAGE, []() { theApp.openStartPage(); });
	addCommand(ID_LOCK_GAME_LAYOUT, [this]() {
		if (!g_overrideLockGameLayout) {
			m_gameLayoutLocked = true;
			enterGameLayout();
		}
	});
	addCommand(ID_VIEW_GAME_LAYOUT, [this]() {
		if (m_gameLayout) {
			leaveGameLayout();
		}
		else {
			enterGameLayout();
		}
	});
#ifdef __ANDROID__
	// Full screen is the game layout here (see setFullScreen), which other commands also change.
	addCommand(ID_VIEW_FULL_SCREEN, [this]() { setFullScreen(!m_gameLayout); });
#else
	addCommand(ID_VIEW_FULL_SCREEN, [this]() { setFullScreen(!m_fullScreen); });
#endif
	addCommand(ID_TOOLS_CUSTOMIZE, [this]() { showCustomize(); });

	for (unsigned id :
		 {ID_VIEW_OUTPUT,
		  ID_TOOL_TOOLBOX_PANE,
		  ID_VIEW_EXPLORER,
		  ID_VIEW_PROPERTIES,
		  ID_VIEW_REPORT,
		  ID_VIEW_START_PAGE,
		  ID_TOOLS_SETTINGS}) {
		addCommandUpdate(id, [](CommandState& state) { state.enabled = true; });
	}
	addCommandUpdate(ID_INDICATOR_TIME, [](CommandState& state) { state.text = QString(); });
	addCommandUpdate(ID_INDICATOR_FPS, [](CommandState& state) { state.text = QString(); });
	addCommandUpdate(ID_INDICATOR_VAR_MEMORY, [](CommandState& state) {
		std::string memory = RBX::Log::formatMem(G3D::VARArea::sizeOfAllVARAreasInMemory());
		state.text = QStringLiteral("VAR %1").arg(memory.c_str());
	});
	addCommandUpdate(ID_INDICATOR_TEXTURE_MEMORY, [](CommandState& state) {
		std::string memory = RBX::Log::formatMem(G3D::Texture::sizeOfAllTexturesInMemory());
		state.text = QStringLiteral("TEX %1").arg(memory.c_str());
	});
	addCommandUpdate(ID_VIEW_GAME_LAYOUT, [this](CommandState& state) { state.checked = m_gameLayout; });
	addCommandUpdate(ID_VIEW_FULL_SCREEN, [this](CommandState& state) { state.checked = m_fullScreen; });
}

CMainFrame::~CMainFrame()
{
	RBX::FillTool::color.removeListener(this);
}

static QIcon loadFrameIcon(const QString& path)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) {
		return QIcon();
	}

	QByteArray data = file.readAll();
	const uchar* bytes = reinterpret_cast<const uchar*>(data.constData());
	int count = data.size() >= iconDirectorySize ? qFromLittleEndian<quint16>(bytes + 4) : 0;
	for (int index = 0; index < count; index++) {
		const uchar* entry = bytes + iconDirectorySize + index * iconEntrySize;
		if (entry + iconEntrySize > bytes + data.size()) {
			break;
		}

		quint32 size = qFromLittleEndian<quint32>(entry + 8);
		quint32 offset = qFromLittleEndian<quint32>(entry + 12);
		if (entry[0] != largeIconSize || qFromLittleEndian<quint16>(entry + 6) != 32 ||
			offset + size > static_cast<quint32>(data.size())) {
			continue;
		}

		const uchar* pixels = bytes + offset + qFromLittleEndian<quint32>(bytes + offset);
		QImage large(largeIconSize, largeIconSize, QImage::Format_ARGB32);
		for (int y = 0; y < largeIconSize; y++) {
			memcpy(large.scanLine(largeIconSize - 1 - y), pixels + y * largeIconSize * 4, largeIconSize * 4);
		}

		QImage small(largeIconSize / 2, largeIconSize / 2, QImage::Format_ARGB32);
		for (int y = 0; y < small.height(); y++) {
			for (int x = 0; x < small.width(); x++) {
				int sums[4] = {};
				for (int corner = 0; corner < 4; corner++) {
					QRgb pixel = large.pixel(2 * x + (corner & 1), 2 * y + (corner >> 1));
					sums[0] += qRed(pixel);
					sums[1] += qGreen(pixel);
					sums[2] += qBlue(pixel);
					sums[3] += qAlpha(pixel);
				}
				small.setPixel(x, y, qRgba(sums[0] / 4, sums[1] / 4, sums[2] / 4, sums[3] / 4));
			}
		}

		QIcon icon(QPixmap::fromImage(large));
		icon.addPixmap(QPixmap::fromImage(small));
		return icon;
	}
	return QIcon();
}

bool CMainFrame::create()
{
	setWindowIcon(loadFrameIcon(Resources::iconPath(mainResourceId)));
	setWindowTitle(Resources::string(appTitleStringId));

	m_workspace = new QTabWidget(this);
	m_workspace->setDocumentMode(true);
	m_workspace->setMovable(true);
	m_workspaceButtons = new WorkspaceTabButtons(m_workspace);
	m_workspaceButtons->onClose([this]() {
		if (CChildFrame* frame = activeChildFrame()) {
			frame->requestClose();
		}
	});
	m_workspace->setCornerWidget(m_workspaceButtons, Qt::TopRightCorner);
	setCentralWidget(m_workspace);
	connect(m_workspace, &QTabWidget::currentChanged, this, [this](int index) {
		childFrameActivated(static_cast<CChildFrame*>(m_workspace->widget(index)));
	});

	createStatusBar();
	createAccelerators();

	ImageManager::instance().loadToolBar(cameraToolBarId);
	ImageManager::instance().loadToolBar(mainResourceId);
	ImageManager::instance().loadToolBar(mouseToolBarId);
	ImageManager::instance().loadToolBar(testToolBarId);
	ImageManager::instance().loadToolBar(commandToolBarId);
	ImageManager::instance().loadCommandImages();

	createMenuBar();
	createToolBars();

	m_gameLayout = true;
	leaveGameLayout();

	if (Profile::GetProfileInt("Settings", "fullscreen", 0) == 1) {
		setFullScreen(true);
	}

	connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
		applicationActivated(state == Qt::ApplicationActive);
	});

	postCommand(ID_TOOLS_REPORT_CRASHES);
	return true;
}

void CMainFrame::createStatusBar()
{
	StatusBar* bar = new StatusBar(this);
	setStatusBar(bar);
	bar->setFixedHeight(statusBarHeight * bar->logicalDpiY() / 96);
	m_indicators[0] = new QLabel(Resources::string(idleMessageStringId), bar);
	bar->addPane(m_indicators[0], QGuiApplication::primaryScreen()->geometry().width() / 4);
	connect(bar, &QStatusBar::messageChanged, this, [this](const QString& message) {
		if (message.isEmpty()) {
			m_indicators[0]->setText(Resources::string(idleMessageStringId));
		}
	});
	for (int i = 1; i < 5; i++) {
		m_indicators[i] = new QLabel(bar);
		QFont font = m_indicators[i]->font();
		font.setKerning(false);
		bar->addPane(
			m_indicators[i],
			QFontMetrics(font).horizontalAdvance(Resources::string(indicators[i])) + indicatorPadding - 2
		);
	}
}

void CMainFrame::setMessageText(const QString& text)
{
	m_indicators[0]->setText(text);
}

static QKeySequence acceleratorSequence(const Resources::Accelerator& accelerator)
{
	int key;
	if (accelerator.key >= 'A' && accelerator.key <= 'Z') {
		key = Qt::Key_A + (accelerator.key - 'A');
	}
	else if (accelerator.key >= 112 && accelerator.key <= 123) {
		key = Qt::Key_F1 + (accelerator.key - 112);
	}
	else {
		switch (accelerator.key) {
		case 8:
			key = Qt::Key_Backspace;
			break;
		case 45:
			key = Qt::Key_Insert;
			break;
		case 46:
			key = Qt::Key_Delete;
			break;
		default:
			return QKeySequence();
		}
	}

	if (accelerator.flags & Resources::ACCEL_SHIFT) {
		key |= Qt::ShiftModifier;
	}
	if (accelerator.flags & Resources::ACCEL_CONTROL) {
		key |= Qt::ControlModifier;
	}
	if (accelerator.flags & Resources::ACCEL_ALT) {
		key |= Qt::AltModifier;
	}
	return QKeySequence(key);
}

void CMainFrame::createAccelerators()
{
	for (const Resources::Accelerator& accelerator : Resources::accelerators(mainResourceId)) {
		QKeySequence sequence = acceleratorSequence(accelerator);
		if (sequence.isEmpty()) {
			continue;
		}

		QShortcut* shortcut = new QShortcut(sequence, this);
		unsigned id = accelerator.id;
		connect(shortcut, &QShortcut::activated, this, [this, id]() { sendCommand(id); });
	}
}

QAction* CMainFrame::toolBarAction(unsigned id)
{
	QAction* action = new QAction(this);
	QString prompt = Resources::string(id);
	action->setStatusTip(prompt.section('\n', 0, 0));
	action->setToolTip(prompt.section('\n', 1, 1));
	action->setText(prompt.count('\n') >= 2 ? prompt.section('\n', 2, 2) : prompt.section('\n', 1, 1));
	action->setIcon(ImageManager::instance().icon(id));
	action->setData(id);
	connect(action, &QAction::triggered, this, [this, id]() {
		markCommandUsed(id);
		sendCommand(id);
	});
	m_toolBarActions.emplace(id, action);
	return action;
}

static const char* const tearOffPopupCaption = "Camera Positioning";

QAction* CMainFrame::menuCommandAction(QObject* parent, unsigned id, const QString& text)
{
	QAction* action = new QAction(text, parent);
	action->setData(id);
	action->setStatusTip(Resources::string(id).section('\n', 0, 0));
	action->setIcon(ImageManager::instance().icon(id));
	connect(action, &QAction::triggered, this, [this, id]() {
		markCommandUsed(id);
		sendCommand(id);
	});
	return action;
}

void CMainFrame::buildMenu(QMenu* menu, MenuIterator& it, MenuIterator end)
{
	setupMenu(menu);

	while (it != end) {
		const Resources::MenuEntry& entry = *it++;
		switch (entry.kind) {
		case Resources::MENU_ITEM:
			menu->addAction(menuCommandAction(menu, entry.id, QString::fromLatin1(entry.text)));
			break;
		case Resources::MENU_SEPARATOR:
			menu->addSeparator();
			break;
		case Resources::MENU_POPUP: {
			QMenu* popup = menu->addMenu(QString::fromLatin1(entry.text));
			popup->setProperty("menuKey", menu->property("menuKey").toString() + QLatin1Char('/') + entry.text);
			if (qstrcmp(entry.text, tearOffPopupCaption) == 0) {
				TearOffGripper::addTo(popup);
			}
			buildMenu(popup, it, end);
			break;
		}
		case Resources::MENU_END:
			return;
		}
	}
}

void CMainFrame::createMenuBar()
{
#ifdef __ANDROID__
	// A native menu bar becomes the activity's action bar, which shows the app title above the client
	// and cannot be hidden with the rest of the IDE; keep the menus inside the window instead.
	menuBar()->setNativeMenuBar(false);
#endif
	std::span<const Resources::MenuEntry> entries = Resources::menu(mainResourceId);
	MenuIterator it = entries.begin();
	while (it != entries.end()) {
		const Resources::MenuEntry& entry = *it++;
		QMenu* menu = menuBar()->addMenu(QString::fromLatin1(entry.text));
		menu->setProperty("menuKey", QString::fromLatin1(entry.text));
		buildMenu(menu, it, entries.end());
	}
	menuBar()->installEventFilter(this);
}

QToolButton* CMainFrame::createFillColorButton(QToolBar* toolBar)
{
	QToolButton* button = new QToolButton(toolBar);
	button->setPopupMode(QToolButton::InstantPopup);
	button->setIcon(ImageManager::instance().icon(ID_TOOL_FILL));
	QString prompt = Resources::string(ID_TOOL_FILL);
	button->setStatusTip(prompt.section('\n', 0, 0));
	button->setToolTip(prompt.section('\n', 1, 1));

	QMenu* popup = new QMenu(button);
	TearOffGripper::addTo(popup);
	ColorSelector* selector = new ColorSelector(popup);
	QWidgetAction* action = new QWidgetAction(popup);
	action->setDefaultWidget(selector);
	popup->addAction(action);
	button->setMenu(popup);

	connect(popup, &QMenu::aboutToShow, this, [selector]() { selector->setColor(RBX::FillTool::color.get()); });
	selector->onColorSelected([popup](RBX::BrickColor color) {
		popup->close();
		RBX::FillTool::color.set(color);

		if (std::shared_ptr<RBX::DataModel> dataModel = ActiveDocView::activeDataModel()) {
			RBX::DataModel::Lock lock(dataModel);
			RBX::Verb* verb = dataModel->getVerb("FillTool");
			verb->doIt(dataModel.get());
		}
	});
	return button;
}

QToolBar* CMainFrame::addCommandToolBar(const QString& caption, unsigned id, Qt::ToolBarArea area)
{
	const Resources::ToolBar* resource = Resources::toolBar(id);

	QToolBar* toolBar = new QToolBar(caption, this);
	toolBar->setObjectName(caption);
	QSize iconSize = resource->width != defaultIconSize ? QSize(resource->width, resource->height)
														: QSize(defaultIconSize, defaultIconSize);
	toolBar->setProperty("baseIconSize", iconSize);
	toolBar->setIconSize(iconSize);

	std::vector<ToolBarControl>& items = m_toolBarItems[toolBar];
	QAction* group = nullptr;
	for (unsigned button : resource->buttons) {
		qsizetype before = toolBar->actions().size();
		switch (button) {
		case 0:
			group = toolBar->addSeparator();
			break;
		case ID_EDIT_UNDO:
		case ID_EDIT_REDO: {
			QToolButton* split = new QToolButton(toolBar);
			split->setDefaultAction(toolBarAction(button));
			split->setPopupMode(QToolButton::MenuButtonPopup);
			QMenu* popup = new QMenu(split);
			std::span<const Resources::MenuEntry> entries =
				Resources::menu(button == ID_EDIT_UNDO ? undoMenuId : redoMenuId);
			MenuIterator it = entries.begin() + 1;
			buildMenu(popup, it, entries.end());
			split->setMenu(popup);
			toolBar->addWidget(split);
			break;
		}
		case ID_COMMAND_HEAP:
		case ID_COMMAND_THREADS: {
			QLabel* label = new QLabel(toolBar);
			label->setMinimumWidth(emptyLabelWidth);
			label->setProperty("commandId", button);
			toolBar->addWidget(label);
			m_toolBarLabels.push_back(label);
			label->installEventFilter(this);
			break;
		}
		case ID_COMMAND_LINE: {
			m_commandLineLabel = new QLabel("Command >", toolBar);
			m_commandLineLabel->installEventFilter(this);
			toolBar->addWidget(m_commandLineLabel);
			m_commandLine = new CommandLineComboBox(m_commandLineLabel, toolBar);
			m_commandLine->installEventFilter(this);
			m_commandLine->lineEdit()->installEventFilter(this);
			toolBar->addWidget(m_commandLine);
			connect(m_commandLine->lineEdit(), &QLineEdit::returnPressed, this, [this]() {
				sendCommand(ID_COMMAND_LINE);
			});
			break;
		}
		case ID_TOOL_FILL:
			m_fillColorButton = createFillColorButton(toolBar);
			toolBar->addWidget(m_fillColorButton);
			break;
		default:
			toolBar->addAction(toolBarAction(button));
			break;
		}
		if (button != 0) {
			QList<QAction*> actions = toolBar->actions();
			ToolBarControl control;
			control.id = button;
			control.actions.assign(actions.begin() + before, actions.end());
			control.beginGroup = group != nullptr;
			control.separator = group;
			if (button == ID_COMMAND_LINE) {
				control.caption = m_commandLineLabel->text();
			}
			items.push_back(control);
			group = nullptr;
		}
	}
	for (const ToolBarControl& control : items) {
		ToolBarControl original;
		original.id = control.id;
		original.actions = control.actions;
		original.beginGroup = control.beginGroup;
		original.caption = control.caption;
		m_toolBarDefaults[toolBar].push_back(original);
	}
	toolBar->installEventFilter(this);
	for (QToolButton* button : toolBar->findChildren<QToolButton*>()) {
		button->installEventFilter(this);
	}

	QEvent styleChange(QEvent::StyleChange);
	QApplication::sendEvent(toolBar, &styleChange);

	addToolBar(area, toolBar);
	m_toolBars.push_back(toolBar);
	m_toolBarsById[id] = toolBar;
	return toolBar;
}

void CMainFrame::createToolBars()
{
	addCommandToolBar("Standard", mainResourceId, Qt::TopToolBarArea);
	QToolBar* mouse = addCommandToolBar("Mouse", mouseToolBarId, Qt::TopToolBarArea);
	QToolBar* test = addCommandToolBar("Test", testToolBarId, Qt::TopToolBarArea);
	insertToolBar(mouse, test);
	addToolBarBreak(Qt::TopToolBarArea);
	addCommandToolBar("Camera", cameraToolBarId, Qt::TopToolBarArea);
	m_commandToolBar = addCommandToolBar("Command", commandToolBarId, Qt::BottomToolBarArea);
	m_commandToolBar->setAllowedAreas(Qt::TopToolBarArea | Qt::BottomToolBarArea);
}

void CMainFrame::resetCommandBars()
{
	menuBar()->show();
	for (QToolBar* toolBar : m_toolBars) {
		toolBar->setVisible(toolBar != m_commandToolBar);
	}
}

void CMainFrame::onEvent(const RBX::FillToolColor* source, RBX::BrickColor event)
{
	if (m_fillColorButton == nullptr) {
		return;
	}

	QPixmap pixmap = ImageManager::instance().icon(ID_TOOL_FILL).pixmap(16, 16);
	QPainter painter(&pixmap);
	int height = pixmap.height() / 5;
	painter.setCompositionMode(QPainter::CompositionMode_Source);
	painter.fillRect(0, pixmap.height() - height, pixmap.width(), height, brickColorToQColor(event));
	painter.end();
	m_fillColorButton->setIcon(QIcon(pixmap));
}

QWidget* CMainFrame::createPaneContents(PaneId id)
{
	switch (id) {
	case PANE_OUTPUT:
		return new CStandardOutputView;
	case PANE_REPORT:
		return new ReportPane;
	case PANE_EXPLORER:
		return new ExplorerPane;
	case PANE_PROPERTIES:
		return new PropertiesPane;
	default:
		return new QWidget;
	}
}

QDockWidget* CMainFrame::createPane(PaneId id, Qt::DockWidgetArea area, QSize size)
{
	QDockWidget* pane = new QDockWidget(Resources::string(id), this);
	pane->setObjectName(QStringLiteral("Pane%1").arg(static_cast<unsigned>(id)));
	pane->setTitleBarWidget(new PaneCaption(pane));
	pane->setWidget(createPaneContents(id));
	addDockWidget(area, pane);
	if (area == Qt::LeftDockWidgetArea || area == Qt::RightDockWidgetArea) {
		resizeDocks({pane}, {size.width()}, Qt::Horizontal);
	}
	else {
		resizeDocks({pane}, {size.height()}, Qt::Vertical);
	}
	m_panes[id] = pane;
	return pane;
}

void CMainFrame::showPane(PaneId id, Qt::DockWidgetArea area, QSize size)
{
	auto it = m_panes.find(id);
	if (it == m_panes.end()) {
		auto remembered = m_paneAreas.find(id);
		if (remembered != m_paneAreas.end()) {
			area = remembered->second;
		}
	}

	QDockWidget* pane = it != m_panes.end() ? it->second : nullptr;
	if (pane == nullptr) {
		bool occupied = std::any_of(m_panes.begin(), m_panes.end(), [this, area](const auto& entry) {
			return entry.second->isVisible() && !entry.second->isFloating() && dockWidgetArea(entry.second) == area;
		});
		if (!occupied) {
			nestDockArea(area, true);
		}
		pane = createPane(id, area, size);
	}
	pane->show();
	pane->raise();
	pane->widget()->setFocus();
}

void CMainFrame::nestDockArea(Qt::DockWidgetArea area, bool outermost)
{
	QVariantList nesting = property("dockNesting").toList();
	if (nesting.contains(static_cast<int>(area))) {
		if (!outermost) {
			return;
		}
		nesting.removeAll(static_cast<int>(area));
	}
	if (outermost) {
		nesting.prepend(static_cast<int>(area));
	}
	else {
		nesting.append(static_cast<int>(area));
	}
	setProperty("dockNesting", nesting);
}

void CMainFrame::showExplorerPane()
{
	showPane(PANE_EXPLORER, Qt::RightDockWidgetArea, QSize(250, 300));
}

void CMainFrame::showPropertiesPane()
{
	bool created = !m_panes.contains(PANE_PROPERTIES);
	showPane(PANE_PROPERTIES, Qt::RightDockWidgetArea, QSize(250, 300));

	if (created) {
		auto neighbour = m_panes.find(PANE_EXPLORER);
		if (neighbour != m_panes.end()) {
			splitDockWidget(neighbour->second, m_panes[PANE_PROPERTIES], Qt::Vertical);
		}
	}
}

void CMainFrame::saveLayout()
{
	if (m_layoutFile.isEmpty()) {
		return;
	}

	QSettings settings(m_layoutFile, QSettings::IniFormat);
	QVariantList panes;
	for (auto& [id, pane] : m_panes) {
		panes.append(static_cast<unsigned>(id));
	}
	settings.setValue("Version", layoutVersion);
	settings.setValue("DockingPane/Panes", panes);

	QVariantList corners;
	for (Qt::Corner dockCorner : dockCorners) {
		corners.append(static_cast<int>(corner(dockCorner)));
	}
	settings.setValue("DockingPane/Corners", corners);
	settings.setValue("DockingPane/Nesting", property("dockNesting"));

	QVariantMap areas;
	for (auto& [id, area] : m_paneAreas) {
		areas.insert(QString::number(id), static_cast<int>(area));
	}
	settings.setValue("DockingPane/Areas", areas);
	settings.setValue("CommandBars/State", saveState());
	QVariantList custom;
	for (auto& [id, toolBar] : m_toolBarsById) {
		if (toolBar->property("customBar").toBool()) {
			custom.append(QVariantList{id, toolBar->windowTitle()});
		}
	}
	settings.setValue("CommandBars/Custom", custom);

	QVariantMap controls;
	for (auto& [id, toolBar] : m_toolBarsById) {
		if (isToolBarCustomized(toolBar)) {
			controls.insert(QString::number(id), saveControls(m_toolBarItems[toolBar]));
		}
	}
	settings.setValue("CommandBars/Controls", controls);

	QVariantMap menus;
	for (QMenu* menu : m_customizedMenus) {
		ensureMenuControls(menu);
		menus.insert(menu->property("menuKey").toString(), saveControls(m_menuItems[menu]));
	}
	settings.setValue("CommandBars/Menus", menus);

	QVariantMap created;
	for (auto& [key, info] : m_createdMenus) {
		created.insert(key, QVariantList{info.caption, info.resource, info.index});
	}
	settings.setValue("CommandBars/CreatedMenus", created);
	if (m_menuBarCustomized) {
		ensureMenuBarControls();
		settings.setValue("CommandBars/MenuBar", saveControls(m_menuBarItems));
	}
	else {
		settings.remove("CommandBars/MenuBar");
	}

	QVariantMap offsets;
	offsets.insert(QString::number(menuBarId), menuBar()->property("dockOffset").toInt());
	for (auto& [id, toolBar] : m_toolBarsById) {
		QVariant mru = toolBar->property("dockMru");
		offsets.insert(QString::number(id), mru.isValid() ? mru.toInt() : toolBar->property("dockOffset").toInt());
	}
	settings.setValue("CommandBars/Offsets", offsets);

	QVariantMap positions;
	for (auto& [id, toolBar] : m_toolBarsById) {
		if (QVariant position = toolBar->property("dockPosition"); position.isValid()) {
			positions.insert(QString::number(id), position);
		}
	}
	settings.setValue("CommandBars/Positions", positions);

	QVariantMap hidden;
	for (auto& [id, toolBar] : m_toolBarsById) {
		QVariantList ids;
		for (const ToolBarControl& control : m_toolBarItems[toolBar]) {
			if (!control.actions.empty() && !control.actions.front()->isVisible()) {
				ids.append(control.id);
			}
		}
		if (!ids.isEmpty()) {
			hidden.insert(QString::number(id), ids);
		}
	}
	settings.setValue("CommandBars/Hidden", hidden);

	QStringList history;
	for (int i = 0; i < m_commandLine->count(); i++) {
		history.append(m_commandLine->itemText(i));
	}
	settings.setValue("CommandBars/CommandHistory", history);
	settings.setValue("Options/AlwaysShowFullMenus", m_options.alwaysShowFullMenus);
	settings.setValue("Options/ShowFullAfterDelay", m_options.showFullAfterDelay);
	settings.setValue("Options/ToolBarScreenTips", m_options.toolBarScreenTips);
	settings.setValue("Options/ToolBarAccelTips", m_options.toolBarAccelTips);
	settings.setValue("Options/LargeIcons", m_options.largeIcons);
	settings.setValue("Options/Animation", m_options.animation);
	QVariantList used;
	for (unsigned id : m_options.usedCommands) {
		used.append(id);
	}
	settings.setValue("Options/UsedCommands", used);
}

static QVariantList importedNesting(const ImportedLayout& layout)
{
	QVariantList nesting;
	int node = layout.topContainer;
	while (true) {
		auto it = layout.panes.find(node);
		if (it == layout.panes.end() || it->second.type != ImportedPaneNode::Splitter || !layout.containsClient(node)) {
			break;
		}
		const ImportedPaneNode& pane = it->second;
		bool beforeClient = true;
		int clientChild = 0;
		for (int child : pane.children) {
			if (layout.containsClient(child)) {
				beforeClient = false;
				clientChild = child;
				continue;
			}
			int area = pane.horizontal ? (beforeClient ? Qt::LeftDockWidgetArea : Qt::RightDockWidgetArea)
									   : (beforeClient ? Qt::TopDockWidgetArea : Qt::BottomDockWidgetArea);
			if (!nesting.contains(area)) {
				nesting.append(area);
			}
		}
		node = clientChild;
	}
	return nesting;
}

bool CMainFrame::loadLayout()
{
	if (m_layoutFile.isEmpty() || !QFile::exists(m_layoutFile)) {
		return false;
	}

	QSettings settings(m_layoutFile, QSettings::IniFormat);
	if (settings.value("Version").toInt() != layoutVersion) {
		return false;
	}

	QVariantList corners = settings.value("DockingPane/Corners").toList();
	for (qsizetype i = 0; i < corners.size() && i < static_cast<qsizetype>(std::size(dockCorners)); i++) {
		setCorner(dockCorners[i], static_cast<Qt::DockWidgetArea>(corners[i].toInt()));
	}
	if (settings.contains("DockingPane/Nesting")) {
		setProperty("dockNesting", settings.value("DockingPane/Nesting").toList());
	}
	else {
		ImportedLayout imported;
		if (importLayout(QFileInfo(m_layoutFile).dir().filePath("IDESettings.xml"), imported)) {
			setProperty("dockNesting", importedNesting(imported));
		}
	}

	QVariantMap areas = settings.value("DockingPane/Areas").toMap();
	for (auto area = areas.begin(); area != areas.end(); ++area) {
		m_paneAreas[static_cast<PaneId>(area.key().toUInt())] = static_cast<Qt::DockWidgetArea>(area.value().toInt());
	}

	for (const QVariant& pane : settings.value("DockingPane/Panes").toList()) {
		PaneId id = static_cast<PaneId>(pane.toUInt());
		if (!m_panes.contains(id)) {
			createPane(id, Qt::RightDockWidgetArea, QSize(250, 300));
		}
	}
	QVariantMap offsets = settings.value("CommandBars/Offsets").toMap();
	for (auto offset = offsets.begin(); offset != offsets.end(); ++offset) {
		unsigned id = offset.key().toUInt();
		if (id == menuBarId) {
			menuBar()->setProperty("dockOffset", offset.value());
		}
		else if (auto toolBar = m_toolBarsById.find(id); toolBar != m_toolBarsById.end()) {
			toolBar->second->setProperty("dockOffset", offset.value());
			toolBar->second->setProperty("dockMru", offset.value());
		}
	}

	CommandBarsOptions defaults;
	if (!settings.contains("Options/AlwaysShowFullMenus")) {
		ImportedLayout original;
		if (importLayout(QFileInfo(m_layoutFile).dir().filePath("IDESettings.xml"), original) && original.options) {
			defaults = *original.options;
		}
	}
	m_options.alwaysShowFullMenus =
		settings.value("Options/AlwaysShowFullMenus", defaults.alwaysShowFullMenus).toBool();
	m_options.showFullAfterDelay = settings.value("Options/ShowFullAfterDelay", defaults.showFullAfterDelay).toBool();
	m_options.toolBarScreenTips = settings.value("Options/ToolBarScreenTips", defaults.toolBarScreenTips).toBool();
	m_options.toolBarAccelTips = settings.value("Options/ToolBarAccelTips", defaults.toolBarAccelTips).toBool();
	m_options.largeIcons = settings.value("Options/LargeIcons", defaults.largeIcons).toBool();
	m_options.animation = settings.value("Options/Animation", defaults.animation).toInt();
	m_options.usedCommands = defaults.usedCommands;
	if (settings.contains("Options/UsedCommands")) {
		m_options.usedCommands.clear();
		for (const QVariant& id : settings.value("Options/UsedCommands").toList()) {
			m_options.usedCommands.insert(id.toUInt());
		}
	}

	m_commandLine->clear();
	m_commandLine->addItems(settings.value("CommandBars/CommandHistory").toStringList());
	m_commandLine->setCurrentIndex(-1);
	for (const QVariant& entry : settings.value("CommandBars/Custom").toList()) {
		QVariantList fields = entry.toList();
		if (fields.size() == 2 && !m_toolBarsById.contains(fields[0].toUInt())) {
			addCustomToolBar(fields[1].toString(), fields[0].toUInt());
		}
	}

	if (settings.contains("CommandBars/Positions")) {
		QVariantMap positions = settings.value("CommandBars/Positions").toMap();
		for (auto entry = positions.begin(); entry != positions.end(); ++entry) {
			if (auto toolBar = m_toolBarsById.find(entry.key().toUInt()); toolBar != m_toolBarsById.end()) {
				toolBar->second->setProperty("dockPosition", entry.value().toInt());
			}
		}
	}
	else if (
		ImportedLayout original; importLayout(QFileInfo(m_layoutFile).dir().filePath("IDESettings.xml"), original)
	) {
		for (auto& [id, position] : original.barPositions) {
			if (auto toolBar = m_toolBarsById.find(id); toolBar != m_toolBarsById.end()) {
				toolBar->second->setProperty("dockPosition", position.x());
			}
		}
	}

	QVariantMap created = settings.value("CommandBars/CreatedMenus").toMap();
	for (auto entry = created.begin(); entry != created.end(); ++entry) {
		QVariantList info = entry.value().toList();
		if (info.size() == 3 && !menuByKey(entry.key())) {
			createPopupMenu(info[0].toString(), info[1].toUInt(), info[2].toInt(), entry.key());
		}
	}

	QVariantMap controls = settings.value("CommandBars/Controls").toMap();
	for (auto entry = controls.begin(); entry != controls.end(); ++entry) {
		if (auto toolBar = m_toolBarsById.find(entry.key().toUInt()); toolBar != m_toolBarsById.end()) {
			restoreToolBarControls(toolBar->second, entry.value().toList());
		}
	}

	QVariantMap menus = settings.value("CommandBars/Menus").toMap();
	for (QMenu* menu : findChildren<QMenu*>()) {
		auto entry = menus.find(menu->property("menuKey").toString());
		if (menu->property("customizable").toBool() && entry != menus.end()) {
			restoreMenuControls(menu, entry.value().toList());
		}
	}

	if (settings.contains("CommandBars/MenuBar")) {
		restoreMenuBar(settings.value("CommandBars/MenuBar").toList());
		m_menuBarCustomized = true;
	}

	QVariantMap hidden = settings.value("CommandBars/Hidden").toMap();
	for (auto entry = hidden.begin(); entry != hidden.end(); ++entry) {
		if (auto toolBar = m_toolBarsById.find(entry.key().toUInt()); toolBar != m_toolBarsById.end()) {
			for (const QVariant& id : entry.value().toList()) {
				setToolBarItemVisible(toolBar->second, id.toUInt(), false);
			}
		}
	}
	return restoreState(settings.value("CommandBars/State").toByteArray());
}

void CMainFrame::claimCorner(Qt::Corner corner, Qt::DockWidgetArea area)
{
	if (!(m_claimedCorners & (1 << corner))) {
		m_claimedCorners |= 1 << corner;
		setCorner(corner, area);
	}
}

void CMainFrame::importPanes(const ImportedLayout& layout, int node, Qt::DockWidgetArea area, QDockWidget*& previous)
{
	auto it = layout.panes.find(node);
	if (it == layout.panes.end()) {
		return;
	}

	const ImportedPaneNode& pane = it->second;
	switch (pane.type) {
	case ImportedPaneNode::Splitter:
		if (layout.containsClient(node)) {
			bool beforeClient = true;
			int clientChild = 0;
			for (int child : pane.children) {
				if (layout.containsClient(child)) {
					beforeClient = false;
					clientChild = child;
					continue;
				}

				Qt::DockWidgetArea childArea = pane.horizontal
												   ? (beforeClient ? Qt::LeftDockWidgetArea : Qt::RightDockWidgetArea)
												   : (beforeClient ? Qt::TopDockWidgetArea : Qt::BottomDockWidgetArea);
				nestDockArea(childArea, false);
				bool leading = childArea == Qt::LeftDockWidgetArea || childArea == Qt::TopDockWidgetArea;
				if (pane.horizontal) {
					claimCorner(leading ? Qt::TopLeftCorner : Qt::TopRightCorner, childArea);
					claimCorner(leading ? Qt::BottomLeftCorner : Qt::BottomRightCorner, childArea);
				}
				else {
					claimCorner(leading ? Qt::TopLeftCorner : Qt::BottomLeftCorner, childArea);
					claimCorner(leading ? Qt::TopRightCorner : Qt::BottomRightCorner, childArea);
				}
				QDockWidget* none = nullptr;
				importPanes(layout, child, childArea, none);
			}

			QDockWidget* none = nullptr;
			importPanes(layout, clientChild, area, none);
		}
		else {
			QDockWidget* group = previous;
			for (int child : pane.children) {
				QDockWidget* first = nullptr;
				importPanes(layout, child, area, first);
				if (first && group) {
					splitDockWidget(group, first, pane.horizontal ? Qt::Horizontal : Qt::Vertical);
				}
				if (first) {
					group = first;
					if (previous == nullptr) {
						previous = first;
					}
				}
			}
		}
		break;
	case ImportedPaneNode::Tabbed: {
		for (const auto& [index, candidate] : layout.panes) {
			if (candidate.type == ImportedPaneNode::Pane && candidate.dockingHolder == node) {
				m_paneAreas[static_cast<PaneId>(candidate.paneId)] = area;
			}
		}

		QDockWidget* first = nullptr;
		for (int child : pane.children) {
			auto childIt = layout.panes.find(child);
			if (childIt == layout.panes.end() || childIt->second.type != ImportedPaneNode::Pane) {
				continue;
			}

			PaneId id = static_cast<PaneId>(childIt->second.paneId);
			QSize size(childIt->second.dockingWidth, childIt->second.dockingHeight);
			QDockWidget* dock = m_panes.contains(id) ? m_panes[id] : createPane(id, area, size);
			dock->show();
			if (first) {
				tabifyDockWidget(first, dock);
			}
			else {
				first = dock;
			}
			if (child == pane.selected) {
				dock->raise();
			}
		}
		if (previous == nullptr) {
			previous = first;
		}
		break;
	}
	default:
		break;
	}
}

bool CMainFrame::importOriginalLayout()
{
	if (m_layoutFile.isEmpty()) {
		return false;
	}

	ImportedLayout layout;
	if (!importLayout(QFileInfo(m_layoutFile).dir().filePath("IDESettings.xml"), layout)) {
		return false;
	}

	if (!layout.dockBars.empty()) {
		static const Qt::ToolBarArea areas[] = {
			Qt::TopToolBarArea, Qt::BottomToolBarArea, Qt::LeftToolBarArea, Qt::RightToolBarArea
		};

		for (QToolBar* toolBar : m_toolBars) {
			removeToolBar(toolBar);
		}
		for (auto& [index, rows] : layout.dockBars) {
			if (index < 0 || index > 3) {
				continue;
			}

			bool firstRow = true;
			for (std::vector<unsigned> row : rows) {
				auto position = [&layout](unsigned id) {
					auto found = layout.barPositions.find(id);
					return found != layout.barPositions.end() ? found->second.x() : 0;
				};
				std::stable_sort(row.begin(), row.end(), [&position](unsigned left, unsigned right) {
					return position(left) < position(right);
				});

				bool placed = false;
				for (unsigned id : row) {
					if (id == menuBarId) {
						menuBar()->setProperty("dockOffset", position(id));
						continue;
					}

					auto it = m_toolBarsById.find(id);
					if (it == m_toolBarsById.end()) {
						continue;
					}
					if (!placed && !firstRow) {
						addToolBarBreak(areas[index]);
					}
					it->second->setProperty("dockOffset", placed ? 0 : position(id));
					it->second->setProperty("dockMru", placed ? 0 : position(id));
					if (placed) {
						it->second->setProperty("dockPosition", position(id));
					}
					addToolBar(areas[index], it->second);
					it->second->show();
					placed = true;
				}
				if (placed) {
					firstRow = false;
				}
			}
		}
	}

	if (layout.topContainer != 0) {
		QDockWidget* none = nullptr;
		m_claimedCorners = 0;
		setProperty("dockNesting", QVariantList());
		importPanes(layout, layout.topContainer, Qt::RightDockWidgetArea, none);
	}

	if (layout.options) {
		m_options = *layout.options;
	}

	m_commandLine->clear();
	m_commandLine->addItems(layout.commandHistory);
	m_commandLine->setCurrentIndex(-1);
	return true;
}

void CMainFrame::enterGameLayout()
{
	if (m_gameLayout) {
		return;
	}

	saveLayout();

	statusBar()->hide();
	m_workspace->tabBar()->hide();
	m_workspaceButtons->hide();
	menuBar()->hide();
	for (QToolBar* toolBar : m_toolBars) {
		toolBar->hide();
	}
	for (auto& [id, pane] : m_panes) {
		pane->hide();
	}

	m_gameLayout = true;
	updateMaximumSize();
}

void CMainFrame::leaveGameLayout()
{
	if (!m_gameLayout || m_gameLayoutLocked) {
		return;
	}

	m_gameLayout = false;
	updateMaximumSize();

	statusBar()->show();
	resetCommandBars();
	m_workspace->tabBar()->show();
	m_workspaceButtons->show();

	if (!loadLayout() && !importOriginalLayout()) {
		showExplorerPane();
		showPropertiesPane();
	}
	applyCommandBarsOptions();
}

QToolBar* CMainFrame::addCustomToolBar(const QString& title, unsigned id)
{
	if (id == 0) {
		id = firstCustomBarId;
		while (m_toolBarsById.contains(id)) {
			id++;
		}
	}
	QToolBar* toolBar = new QToolBar(title, this);
	toolBar->setObjectName(QStringLiteral("Custom%1").arg(id));
	toolBar->setProperty("customBar", true);
	toolBar->setProperty("baseIconSize", QSize(defaultIconSize, defaultIconSize));
	toolBar->setIconSize(QSize(defaultIconSize, defaultIconSize));
	m_toolBarItems[toolBar];
	toolBar->setMinimumSize(WhidbeyStyle::emptyToolBarExtent, m_toolBars.front()->sizeHint().height());
	toolBar->installEventFilter(this);
	addToolBarBreak(Qt::TopToolBarArea);
	addToolBar(Qt::TopToolBarArea, toolBar);
	m_toolBars.push_back(toolBar);
	m_toolBarsById[id] = toolBar;
	applyCommandBarsOptions();
	return toolBar;
}

void CMainFrame::removeCustomToolBar(QToolBar* toolBar)
{
	removeToolBar(toolBar);
	std::erase(m_toolBars, toolBar);
	m_toolBarItems.erase(toolBar);
	m_toolBarDefaults.erase(toolBar);
	std::erase_if(m_toolBarsById, [toolBar](const auto& entry) { return entry.second == toolBar; });
	toolBar->deleteLater();
}

void CMainFrame::setCustomizeMode(bool customize, bool quick)
{
	WhidbeyStyle::setCustomizeMode(customize);
	setProperty("toolBarWrapRows", customize && !quick);
	layout()->invalidate();
	for (QMenu* menu : findChildren<QMenu*>()) {
		if (menu->property("customizable").toBool()) {
			menu->setWindowFlag(Qt::NoDropShadowWindowHint, customize);
		}
	}
	updateCommandUI();
	menuBar()->update();
	for (QToolBar* toolBar : m_toolBars) {
		toolBar->update();
		for (QWidget* child : toolBar->findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) {
			child->update();
		}
	}
}

void CMainFrame::applyCommandBarsOptions()
{
	WhidbeyStyle::setMenuAnimation(m_options.animation);
	for (QToolBar* toolBar : m_toolBars) {
		QSize size =
			m_options.largeIcons ? QSize(largeIconSize, largeIconSize) : toolBar->property("baseIconSize").toSize();
		toolBar->setProperty("largeIcons", m_options.largeIcons);
		toolBar->setIconSize(size);
		for (QToolButton* button : toolBar->findChildren<QToolButton*>(Qt::FindDirectChildrenOnly)) {
			button->setIconSize(size);
		}
		for (ToolBarControl& control : m_toolBarItems[toolBar]) {
			if (!control.customIcon.isNull()) {
				applyToolBarControl(toolBar, control);
			}
		}
	}
}

void CMainFrame::restorePlacement(bool show)
{
	bool maximized = false;
	bool minimized = false;

	QByteArray placement;
	if (Profile::GetProfileBinary("Settings", "CMainFrameV2", placement)) {
#ifdef _WIN32
		if (placement.size() == sizeof(WINDOWPLACEMENT)) {
			WINDOWPLACEMENT wp;
			memcpy(&wp, placement.constData(), sizeof(wp));
			maximized = wp.showCmd == SW_SHOWMAXIMIZED;
			minimized = wp.showCmd == SW_SHOWMINIMIZED;
			wp.showCmd = SW_HIDE;
			SetWindowPlacement(reinterpret_cast<HWND>(winId()), &wp);
		}
#else
		restoreGeometry(placement);
		maximized = isMaximized();
#endif
	}
	else {
#ifdef _WIN32
		HWND probe = CreateWindowExW(
			0,
			L"Static",
			L"",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			nullptr,
			nullptr,
			nullptr,
			nullptr
		);
		WINDOWPLACEMENT wp = {sizeof(wp)};
		GetWindowPlacement(probe, &wp);
		DestroyWindow(probe);
		wp.showCmd = SW_HIDE;
		SetWindowPlacement(reinterpret_cast<HWND>(winId()), &wp);
#else
		QRect work = screen()->availableGeometry();
		resize(work.width() * 3 / 4, work.height() * 3 / 4);
#endif
	}

	if (!show || m_fullScreen) {
		return;
	}

#ifdef __ANDROID__
	// A phone or tablet has no desktop to place the window on; the client fills the screen.
	showFullScreen();
	return;
#endif

	if (maximized) {
		showMaximized();
	}
	else if (minimized) {
		showMinimized();
	}
	else {
		showNormal();
	}
}

void CMainFrame::savePlacement()
{
#ifdef _WIN32
	WINDOWPLACEMENT wp;
	wp.length = sizeof(wp);
	if (GetWindowPlacement(reinterpret_cast<HWND>(winId()), &wp)) {
		if (wp.showCmd != SW_SHOWMAXIMIZED) {
			wp.showCmd = SW_SHOWNORMAL;
		}
		Profile::WriteProfileBinary(
			"Settings", "CMainFrameV2", QByteArray(reinterpret_cast<const char*>(&wp), sizeof(wp))
		);
	}
#else
	Profile::WriteProfileBinary("Settings", "CMainFrameV2", saveGeometry());
#endif
}

void CMainFrame::changeResolution()
{
	RBX::ScopedAssign<bool> changing(m_changingResolution, true);

	G3D::Vector2int16 size = CRenderSettings::singleton().getFullscreenSize();
	if (changeDisplayMode(this, QSize(size.x, size.y))) {
		showFullScreen();
	}
}

void CMainFrame::restoreResolution()
{
	RBX::ScopedAssign<bool> restoring(m_changingResolution, true);

	hide();
	restoreDisplayMode(this);
}

void CMainFrame::setFullScreen(bool fullScreen)
{
#ifdef __ANDROID__
	// The window already fills the screen, so full screen hides the IDE around the game view instead.
	if (fullScreen) {
		enterGameLayout();
	}
	else {
		RBX::ScopedAssign<bool> unlocked(m_gameLayoutLocked, false);
		leaveGameLayout();
	}
	m_fullScreen = m_gameLayout;
	// Let the game view run under the camera cutout too; the IDE keeps clear of it.
	setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, !m_fullScreen);
	return;
#endif
	if (m_fullScreen == fullScreen) {
		return;
	}

	m_fullScreen = fullScreen;
	updateMaximumSize();

	if (fullScreen) {
		m_normalGeometry = isMaximized() ? QRect() : geometry();
		changeResolution();
	}
	else {
		restoreResolution();

		RBX::ScopedAssign<bool> restoring(m_changingResolution, true);
		if (m_normalGeometry.isEmpty()) {
			showMaximized();
		}
		else {
			showNormal();
			setGeometry(m_normalGeometry);
		}
	}

	updateMaximumSize();
}

void CMainFrame::updateMaximumSize()
{
#ifdef __ANDROID__
	// The window always fills the device screen, whatever full screen size the settings name.
	if (false) {
#else
	if (!m_changingResolution && !m_fullScreen && m_gameLayout) {
#endif
		G3D::Vector2int16 size = CRenderSettings::singleton().getFullscreenSize();
		setMaximumSize(size.x, size.y);
	}
	else {
		setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
	}
}

void CMainFrame::applicationActivated(bool active)
{
#ifdef __ANDROID__
	// No display mode to restore, and minimizing would hide the activity's only window.
	return;
#endif
	if (m_fullScreen && !m_changingResolution && !active) {
		restoreResolution();
		showMinimized();
	}
}

void CMainFrame::changeEvent(QEvent* event)
{
	QMainWindow::changeEvent(event);

	if (event->type() != QEvent::ActivationChange) {
		return;
	}

	bool active = QApplication::activeWindow() == this;
	if (m_activeView) {
		m_activeView->activateFrame(active);
	}

#ifndef __ANDROID__
	if (m_fullScreen && !m_changingResolution && active) {
		changeResolution();
	}
#endif
}

bool CMainFrame::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef _WIN32
	MSG* msg = static_cast<MSG*>(message);
	switch (msg->message) {
	case WM_COMMAND:
		if (msg->lParam == 0) {
			sendCommand(LOWORD(msg->wParam));
			*result = 0;
			return true;
		}
		break;
	case WM_WINDOWPOSCHANGING:
		if (m_fullScreen && !m_changingResolution) {
			reinterpret_cast<WINDOWPOS*>(msg->lParam)->flags |= SWP_NOSIZE | SWP_NOMOVE;
		}
		break;
	case WM_MOUSEACTIVATE:
		if (WhidbeyStyle::customizeMode() && LOWORD(msg->lParam) == HTCLIENT) {
			QWidget* target = childAt(mapFromGlobal(QCursor::pos()));
			while (target && !qobject_cast<QToolBar*>(target) && !qobject_cast<QMenuBar*>(target)) {
				target = target->parentWidget();
			}
			if (target) {
				*result = MA_NOACTIVATE;
				return true;
			}
		}
		break;
	case WM_MOUSEWHEEL:
		if (UserInput::isMouseCaptured()) {
			*result = 0;
			return true;
		}
		break;
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_CHAR:
	case WM_DEADCHAR:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_SYSCHAR:
	case WM_SYSDEADCHAR:
		if (UserInput::isKeyboardAcquired()) {
			*result = 0;
			return true;
		}
		break;
	}
#endif
	return QMainWindow::nativeEvent(eventType, message, result);
}

class CustomizePopupKeys : public QObject
{
public:
	CustomizePopupKeys(QMenu* menu, QWidget* sheet) : m_menu(menu), m_sheet(sheet) {}

protected:
	bool eventFilter(QObject* watched, QEvent* event) override
	{
		if (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease) {
			return false;
		}
		if (qobject_cast<QLineEdit*>(watched) || !insideMenu(watched)) {
			return false;
		}
		QWidget* target = m_sheet->focusWidget() ? m_sheet->focusWidget() : m_sheet;
		QKeyEvent* key = static_cast<QKeyEvent*>(event);
		QKeyEvent forwarded(
			key->type(),
			key->key(),
			key->modifiers(),
			key->nativeScanCode(),
			key->nativeVirtualKey(),
			key->nativeModifiers(),
			key->text(),
			key->isAutoRepeat(),
			key->count()
		);
		QApplication::sendEvent(target, &forwarded);
		if (!m_sheet->isVisible()) {
			while (QWidget* popup = QApplication::activePopupWidget()) {
				popup->close();
			}
		}
		return true;
	}

private:
	bool insideMenu(QObject* object) const
	{
		for (; object; object = object->parent()) {
			if (object == m_menu) {
				return true;
			}
		}
		return false;
	}

	QMenu* m_menu;
	QWidget* m_sheet;
};

class CustomizeInputFilter : public QObject
{
public:
	CustomizeInputFilter(QWidget* frame, QWidget* sheet) : m_frame(frame), m_sheet(sheet) {}

	bool eventFilter(QObject* watched, QEvent* event) override
	{
		if (event->type() == QEvent::WindowActivate && watched == m_frame) {
			QTimer::singleShot(0, m_sheet, [sheet = m_sheet]() { sheet->activateWindow(); });
			return false;
		}
		switch (event->type()) {
		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonRelease:
		case QEvent::MouseButtonDblClick:
		case QEvent::Wheel:
			break;
		default:
			return false;
		}
		QWidget* widget = qobject_cast<QWidget*>(watched);
		if (!widget) {
			return false;
		}
		bool owned = false;
		for (QWidget* ancestor = widget; ancestor; ancestor = ancestor->parentWidget()) {
			if (ancestor == m_sheet) {
				return false;
			}
			owned = owned || ancestor == m_frame;
		}
		if (!owned || widget->window()->isActiveWindow()) {
			return false;
		}
		for (QWidget* ancestor = widget; ancestor; ancestor = ancestor->parentWidget()) {
			if (qobject_cast<QToolBar*>(ancestor) || qobject_cast<QMenuBar*>(ancestor) ||
				qobject_cast<QMenu*>(ancestor)) {
				return false;
			}
		}
		return true;
	}

private:
	QWidget* m_frame;
	QWidget* m_sheet;
};

void CMainFrame::showCustomize()
{
	std::vector<CommandBarEntry> bars;
	bars.push_back({menuBarTitle, menuBar(), true, false, {}});
	for (QToolBar* toolBar : m_toolBars) {
		bool builtIn = !toolBar->property("customBar").toBool();
		bars.push_back({toolBar->windowTitle(), toolBar, builtIn, true, [this, toolBar]() { resetToolBar(toolBar); }});
	}
	CustomToolBarActions actions;
	actions.add = [this](const QString& title) {
		return CommandBarEntry{title, addCustomToolBar(title), false, true, {}};
	};
	actions.remove = [this](QWidget* bar) { removeCustomToolBar(static_cast<QToolBar*>(bar)); };
	actions.dragCommand = [this](
							  unsigned id,
							  const QString& caption,
							  bool popup,
							  unsigned menuResource,
							  int menuIndex,
							  QWidget* grabber,
							  QPoint start
						  ) { dragCommand(id, caption, popup, menuResource, menuIndex, grabber, start); };
	CustomizeSheet sheet(std::move(bars), std::move(actions), m_options, [this]() { applyCommandBarsOptions(); }, this);
	CustomizeInputFilter filter(this, &sheet);
	qApp->installEventFilter(&filter);
	m_customizeSheet = &sheet;
	setCustomizeMode(true);
	sheet.RunModeless();
	setCustomizeMode(false);
	m_customizeSheet = nullptr;
	qApp->removeEventFilter(&filter);
}

void CMainFrame::showToolBarOptions(QToolBar* toolBar)
{
	QRect expand = WhidbeyStyle::toolBarExpandRect(toolBar);
	QRect control(toolBar->mapToGlobal(expand.topLeft()), expand.size());
	control = toolBar->orientation() == Qt::Horizontal ? control.adjusted(0, 1, 0, -1) : control.adjusted(1, 0, -1, 0);

	QMenu* popup = new QMenu(toolBar);
	popup->setAttribute(Qt::WA_DeleteOnClose);
	popup->setProperty("toolBarPopup", true);
	popup->setProperty("popupControl", control);
	QMenu* addOrRemove = popup->addMenu(Resources::string(xtpAddOrRemoveStringId).section('\n', 1, 1));
	connect(addOrRemove, &QMenu::aboutToShow, this, [this, addOrRemove, toolBar]() {
		buildAddOrRemoveMenu(addOrRemove, toolBar);
	});

	toolBar->setProperty("expandPopuped", true);
	toolBar->update();
	connect(popup, &QMenu::aboutToHide, toolBar, [toolBar]() {
		toolBar->setProperty("expandPopuped", false);
		toolBar->update();
	});

	popup->ensurePolished();
	int frame = popup->style()->pixelMetric(QStyle::PM_MenuPanelWidth, nullptr, popup);
	QSize size = popup->sizeHint();
	int panelWidth = size.width() - (popup->contentsMargins().right() - popup->contentsMargins().left());

	PopupToolBar* expandBar = new PopupToolBar(popup);
	expandBar->setProperty("largeIcons", m_options.largeIcons);
	getHiddenControls(toolBar, expandBar, popup);
	if (expandBar->isEmpty()) {
		delete expandBar;
	}
	else {
		QAction* addOrRemoveAction = addOrRemove->menuAction();
		QRect addOrRemoveRect = popup->actionGeometry(addOrRemoveAction);
		expandBar->layoutControls(QSize(panelWidth - 2 * frame, addOrRemoveRect.height()));
		QWidgetAction* expandAction = new QWidgetAction(popup);
		expandAction->setDefaultWidget(expandBar);
		popup->insertAction(addOrRemoveAction, expandAction);
		size = popup->sizeHint();
		panelWidth = size.width() - (popup->contentsMargins().right() - popup->contentsMargins().left());
	}

	popup->popup(QPoint(control.right() + 1 - panelWidth, control.bottom() + 1));
}

void CMainFrame::getHiddenControls(QToolBar* toolBar, PopupToolBar* expandBar, QMenu* popup)
{
	if (toolBar->isFloating()) {
		return;
	}

	bool horizontal = toolBar->orientation() == Qt::Horizontal;
	Qt::ToolBarArea area = toolBarArea(toolBar);
	std::vector<QToolBar*> row;
	for (QToolBar* bar : m_toolBars) {
		if (bar->isVisible() && !bar->isFloating() && toolBarArea(bar) == area &&
			(horizontal ? bar->y() == toolBar->y() : bar->x() == toolBar->x())) {
			row.push_back(bar);
		}
	}
	std::sort(row.begin(), row.end(), [horizontal](QToolBar* a, QToolBar* b) {
		return horizontal ? a->x() < b->x() : a->y() < b->y();
	});

	for (QToolBar* bar : row) {
		for (const ToolBarControl& control : m_toolBarItems[bar]) {
			QAction* action = control.actions.front();
			QWidget* widget = bar->widgetForAction(action);
			QToolButton* source = qobject_cast<QToolButton*>(widget);
			if (!action->isVisible() || !source || !source->isHidden()) {
				continue;
			}

			QToolButton* clone = new QToolButton(expandBar);
			if (QAction* defaultAction = source->defaultAction()) {
				clone->setDefaultAction(defaultAction);
			}
			else {
				clone->setIcon(source->icon());
				clone->setToolTip(source->toolTip());
				clone->setStatusTip(source->statusTip());
				clone->setEnabled(source->isEnabled());
			}
			clone->setMenu(source->menu());
			clone->setPopupMode(source->popupMode());
			clone->setToolButtonStyle(source->toolButtonStyle());
			clone->setIconSize(
				m_options.largeIcons ? QSize(largeIconSize, largeIconSize) : QSize(defaultIconSize, defaultIconSize)
			);
			clone->setAutoRaise(true);
			connect(clone, &QToolButton::triggered, popup, &QMenu::close);
			expandBar->addControl(clone);
		}
	}
}

void CMainFrame::buildAddOrRemoveMenu(QMenu* menu, QToolBar* toolBar)
{
	qDeleteAll(menu->findChildren<QMenu*>(Qt::FindDirectChildrenOnly));
	menu->clear();

	Qt::ToolBarArea area = toolBarArea(toolBar);
	bool horizontal = toolBar->orientation() == Qt::Horizontal;
	std::vector<QToolBar*> row;
	for (QToolBar* other : m_toolBars) {
		if (other->isVisible() && toolBarArea(other) == area &&
			(horizontal ? other->y() == toolBar->y() : other->x() == toolBar->x())) {
			row.push_back(other);
		}
	}
	std::sort(row.begin(), row.end(), [horizontal](QToolBar* a, QToolBar* b) {
		return horizontal ? a->x() < b->x() : a->y() < b->y();
	});

	for (QToolBar* rowBar : row) {
		QMenu* customize = menu->addMenu(escapeMnemonics(rowBar->windowTitle()));
		customize->setProperty("doubleGripper", true);
		customize->installEventFilter(this);
		connect(customize, &QMenu::aboutToShow, this, [this, customize, rowBar]() {
			buildCustomizeMenu(customize, rowBar);
		});
	}

	menu->addSeparator();
	unsigned id = ID_TOOLS_CUSTOMIZE;
	QAction* action = menu->addAction(Resources::string(xtpCustomizeStringId).section('\n', 1, 1));
	action->setData(id);
	connect(action, &QAction::triggered, this, [this, id]() { sendCommand(id); });
	updateAction(action, id, false);
}

void CMainFrame::buildCustomizeMenu(QMenu* menu, QToolBar* toolBar)
{
	menu->clear();
	for (const ToolBarControl& control : m_toolBarItems[toolBar]) {
		unsigned id = control.id;
		const std::vector<QAction*>& actions = control.actions;
		QString caption = Resources::string(id).section('\n', 1, 1);
		QString shortcut = acceleratorText(id);
		QAction* action = menu->addAction(shortcut.isEmpty() ? caption : caption + QLatin1Char('\t') + shortcut);
		action->setIcon(ImageManager::instance().icon(id));
		action->setCheckable(true);
		action->setChecked(!actions.empty() && actions.front()->isVisible());
		action->setProperty("customizeItem", id);
		action->setProperty("customizeToolBar", QVariant::fromValue(static_cast<QObject*>(toolBar)));
	}

	menu->addSeparator();
	QAction* reset = menu->addAction(Resources::string(customizeResetId).section('\n', 1, 1));
	connect(reset, &QAction::triggered, this, [this, toolBar]() { resetToolBar(toolBar); });
}

bool CMainFrame::toggleCustomizeItem(QMenu* menu, QAction* action)
{
	if (!action || !action->property("customizeItem").isValid()) {
		return false;
	}

	QToolBar* toolBar = qobject_cast<QToolBar*>(action->property("customizeToolBar").value<QObject*>());
	unsigned id = action->property("customizeItem").toUInt();
	action->setChecked(!action->isChecked());
	setToolBarItemVisible(toolBar, id, action->isChecked());
	menu->update();
	return true;
}

void CMainFrame::setToolBarItemVisible(QToolBar* toolBar, unsigned id, bool visible)
{
	for (const ToolBarControl& control : m_toolBarItems[toolBar]) {
		if (control.id == id) {
			for (QAction* action : control.actions) {
				action->setVisible(visible);
			}
			if (control.separator) {
				control.separator->setVisible(visible);
			}
		}
	}
	toolBar->update();
}

ToolBarControl* CMainFrame::toolBarControl(QWidget* widget, QToolBar** toolBar)
{
	for (auto& [bar, controls] : m_toolBarItems) {
		for (ToolBarControl& control : controls) {
			for (QAction* action : control.actions) {
				if (bar->widgetForAction(action) == widget) {
					*toolBar = bar;
					return &control;
				}
			}
		}
	}
	return nullptr;
}

QToolButton* CMainFrame::controlButton(QToolBar* toolBar, const ToolBarControl& control) const
{
	for (QAction* action : control.actions) {
		if (QToolButton* button = qobject_cast<QToolButton*>(toolBar->widgetForAction(action))) {
			return button;
		}
	}
	return nullptr;
}

QWidget* CMainFrame::controlWidget(QToolBar* toolBar, const ToolBarControl& control) const
{
	for (QAction* action : control.actions) {
		if (QWidget* widget = toolBar->widgetForAction(action); widget && widget->isVisible()) {
			return widget;
		}
	}
	return nullptr;
}

ToolBarControl* CMainFrame::controlForWidget(QWidget* widget, QToolBar** toolBar, QWidget** owner)
{
	for (QWidget* child = widget; child; child = child->parentWidget()) {
		QToolBar* bar = qobject_cast<QToolBar*>(child->parentWidget());
		if (bar && m_toolBarItems.contains(bar)) {
			QToolBar* found = nullptr;
			if (ToolBarControl* control = toolBarControl(child, &found)) {
				*toolBar = found;
				*owner = child;
				return control;
			}
			return nullptr;
		}
	}
	return nullptr;
}

void CMainFrame::selectControl(QToolBar* toolBar, const ToolBarControl& control)
{
	QWidget* widget = controlWidget(toolBar, control);
	WhidbeyStyle::setCustomizeSelection(
		widget, qobject_cast<QToolButton*>(widget) ? QRect() : controlRect(toolBar, control)
	);
}

QString CMainFrame::controlCaption(const ToolBarControl& control) const
{
	if (!control.customCaption.isEmpty()) {
		return control.customCaption;
	}
	if (!control.caption.isEmpty()) {
		return control.caption;
	}
	QString prompt = Resources::string(control.id);
	return prompt.count('\n') >= 2 ? prompt.section('\n', 2, 2) : prompt.section('\n', 1, 1);
}

void CMainFrame::applyToolBarControl(QToolBar* toolBar, ToolBarControl& control)
{
	if (control.id == ID_COMMAND_LINE && m_commandLineLabel) {
		m_commandLineLabel->setText(controlCaption(control));
	}
	QToolButton* button = controlButton(toolBar, control);
	if (!button) {
		return;
	}
	if (QAction* action = button->defaultAction()) {
		action->setText(controlCaption(control));
		action->setIconText(controlCaption(control));
		QIcon icon = control.customIcon.isNull() ? ImageManager::instance().icon(control.id) : control.customIcon;
		if (!control.customIcon.isNull() && !control.customIconAlpha &&
			!control.customIcon.availableSizes().isEmpty()) {
			QImage source = control.customIcon.pixmap(control.customIcon.availableSizes().first()).toImage();
			int width = button->iconSize().width();
			if (source.width() > width) {
				QImage scaled(width, source.height() * width / source.width(), QImage::Format_ARGB32);
				for (int y = 0; y < scaled.height(); y++) {
					for (int x = 0; x < scaled.width(); x++) {
						scaled.setPixel(
							x, y, source.pixel(x * source.width() / width, y * source.height() / scaled.height())
						);
					}
				}
				icon = QIcon(QPixmap::fromImage(scaled));
			}
		}
		action->setIcon(icon);
	}
	bool image = button->defaultAction() ? !button->defaultAction()->icon().isNull() : !button->icon().isNull();
	if (button->defaultAction() && button->defaultAction()->menu()) {
		button->setPopupMode(QToolButton::InstantPopup);
	}
	button->setToolButtonStyle(control.customStyle.value_or(image ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextOnly));
}

void CMainFrame::setControlBeginGroup(QToolBar* toolBar, ToolBarControl& control, bool beginGroup)
{
	control.beginGroup = beginGroup;
	if (beginGroup && !control.separator && !control.actions.empty()) {
		control.separator = toolBar->insertSeparator(control.actions.front());
		control.separator->setVisible(control.actions.front()->isVisible());
	}
	else if (!beginGroup && control.separator) {
		toolBar->removeAction(control.separator);
		delete control.separator;
		control.separator = nullptr;
	}
}

void CMainFrame::deleteToolBarControl(QToolBar* toolBar, ToolBarControl* control)
{
	WhidbeyStyle::setCustomizeSelection(nullptr);
	setControlBeginGroup(toolBar, *control, false);
	for (QAction* action : control->actions) {
		toolBar->removeAction(action);
	}
	std::vector<ToolBarControl>& controls = m_toolBarItems[toolBar];
	controls.erase(controls.begin() + (control - controls.data()));
}

void CMainFrame::resetToolBar(QToolBar* toolBar)
{
	auto defaults = m_toolBarDefaults.find(toolBar);
	if (defaults == m_toolBarDefaults.end()) {
		return;
	}
	WhidbeyStyle::setCustomizeSelection(nullptr);
	std::vector<ToolBarControl>& controls = m_toolBarItems[toolBar];
	for (ToolBarControl& control : controls) {
		setControlBeginGroup(toolBar, control, false);
		for (QAction* action : control.actions) {
			toolBar->removeAction(action);
		}
	}
	controls = defaults->second;
	for (ToolBarControl& control : controls) {
		for (QAction* action : control.actions) {
			action->setVisible(true);
			toolBar->addAction(action);
		}
		if (control.beginGroup) {
			control.beginGroup = false;
			setControlBeginGroup(toolBar, control, true);
		}
		applyToolBarControl(toolBar, control);
	}
	applyCommandBarsOptions();
}

bool CMainFrame::isToolBarCustomized(QToolBar* toolBar)
{
	const std::vector<ToolBarControl>& controls = m_toolBarItems[toolBar];
	const std::vector<ToolBarControl>& defaults = m_toolBarDefaults[toolBar];
	if (controls.size() != defaults.size()) {
		return true;
	}
	for (size_t i = 0; i < controls.size(); i++) {
		const ToolBarControl& control = controls[i];
		if (control.id != defaults[i].id || control.beginGroup != defaults[i].beginGroup ||
			!control.customCaption.isEmpty() || !control.customIcon.isNull() || control.customStyle) {
			return true;
		}
	}
	return false;
}

QVariantList CMainFrame::saveControls(const std::vector<ToolBarControl>& controls)
{
	QVariantList entries;
	for (const ToolBarControl& control : controls) {
		QVariantMap entry;
		entry.insert(QStringLiteral("Id"), control.id);
		if (QMenu* popup = control.actions.front()->menu()) {
			entry.insert(QStringLiteral("Menu"), popup->property("menuKey"));
		}
		entry.insert(QStringLiteral("BeginGroup"), control.beginGroup);
		if (!control.caption.isEmpty()) {
			entry.insert(QStringLiteral("BaseCaption"), control.caption);
		}
		if (!control.customCaption.isEmpty()) {
			entry.insert(QStringLiteral("Caption"), control.customCaption);
		}
		if (control.customStyle) {
			entry.insert(QStringLiteral("Style"), static_cast<int>(*control.customStyle));
		}
		if (!control.customIcon.isNull() && !control.customIcon.availableSizes().isEmpty()) {
			QByteArray png;
			QBuffer buffer(&png);
			buffer.open(QIODevice::WriteOnly);
			control.customIcon.pixmap(control.customIcon.availableSizes().first()).save(&buffer, "PNG");
			entry.insert(QStringLiteral("Icon"), png);
			entry.insert(QStringLiteral("IconAlpha"), control.customIconAlpha);
		}
		entries.append(entry);
	}
	return entries;
}

void CMainFrame::restoreToolBarControls(QToolBar* toolBar, const QVariantList& entries)
{
	std::vector<ToolBarControl>& controls = m_toolBarItems[toolBar];
	for (ToolBarControl& control : controls) {
		setControlBeginGroup(toolBar, control, false);
		for (QAction* action : control.actions) {
			toolBar->removeAction(action);
		}
	}
	controls.clear();

	std::vector<ToolBarControl> unused = m_toolBarDefaults[toolBar];
	for (const QVariant& value : entries) {
		QVariantMap entry = value.toMap();
		unsigned id = entry.value(QStringLiteral("Id")).toUInt();
		ToolBarControl control;
		auto original = std::find_if(unused.begin(), unused.end(), [id](const ToolBarControl& candidate) {
			return candidate.id == id;
		});
		if (QString key = entry.value(QStringLiteral("Menu")).toString(); !key.isEmpty()) {
			QMenu* popup = menuByKey(key);
			if (!popup) {
				continue;
			}
			control.id = 0;
			control.actions = {popup->menuAction()};
			toolBar->addAction(popup->menuAction());
		}
		else if (original != unused.end()) {
			control.id = id;
			control.actions = original->actions;
			unused.erase(original);
			for (QAction* action : control.actions) {
				action->setVisible(true);
				toolBar->addAction(action);
			}
		}
		else if (Resources::string(id).isEmpty() && ImageManager::instance().icon(id).isNull()) {
			continue;
		}
		else {
			control.id = id;
			QAction* action = toolBarAction(id);
			toolBar->addAction(action);
			control.actions.push_back(action);
		}
		control.caption = entry.value(QStringLiteral("BaseCaption")).toString();
		control.customCaption = entry.value(QStringLiteral("Caption")).toString();
		if (entry.contains(QStringLiteral("Style"))) {
			control.customStyle = static_cast<Qt::ToolButtonStyle>(entry.value(QStringLiteral("Style")).toInt());
		}
		QPixmap icon;
		if (icon.loadFromData(entry.value(QStringLiteral("Icon")).toByteArray(), "PNG")) {
			control.customIcon = QIcon(icon);
			control.customIconAlpha = entry.value(QStringLiteral("IconAlpha")).toBool();
		}
		controls.push_back(control);
		if (entry.value(QStringLiteral("BeginGroup")).toBool()) {
			setControlBeginGroup(toolBar, controls.back(), true);
		}
		applyToolBarControl(toolBar, controls.back());
	}
	reparentPopups(toolBar);
}

class DropMarker : public QWidget
{
public:
	explicit DropMarker(QWidget* parent) : QWidget(parent)
	{
		setAttribute(Qt::WA_TransparentForMouseEvents);
		setObjectName(QStringLiteral("DropMarker"));
	}

	void setMarker(QRect marker, bool vertical)
	{
		m_marker = marker;
		m_vertical = vertical;
		update();
	}

protected:
	void paintEvent(QPaintEvent* event) override
	{
		if (m_marker.isEmpty()) {
			return;
		}
		QPainter painter(this);
		painter.setCompositionMode(QPainter::RasterOp_SourceXorDestination);
		QColor face = WhidbeyStyle::colors().face;
		auto invert = [&painter, face](int x, int y, int width, int height) {
			if (width > 0 && height > 0) {
				painter.fillRect(QRect(x, y, width, height), face);
			}
		};
		const QRect& rc = m_marker;
		int left = rc.left();
		int top = rc.top();
		int right = rc.left() + rc.width();
		int bottom = rc.top() + rc.height();
		if (m_vertical) {
			invert(left, top, 1, rc.height());
			invert(left + 1, top + 1, 1, rc.height() - 2);
			invert(left + 2, top + 2, rc.width() - 4, 2);
			invert(right - 1, top, 1, rc.height());
			invert(right - 2, top + 1, 1, rc.height() - 2);
		}
		else {
			invert(left, top, rc.width(), 1);
			invert(left + 1, top + 1, rc.width() - 2, 1);
			invert(left + 2, top + 2, 2, rc.height() - 4);
			invert(left, bottom - 1, rc.width(), 1);
			invert(left + 1, bottom - 2, rc.width() - 2, 1);
		}
	}

private:
	QRect m_marker;
	bool m_vertical = false;
};

class ControlDragFilter : public QObject
{
public:
	std::function<void(QPoint, Qt::KeyboardModifiers)> onMove;
	std::function<void(bool)> onModifiers;
	std::function<void(bool)> onFinish;

	bool eventFilter(QObject* watched, QEvent* event) override
	{
		bool mouse = event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonRelease ||
					 event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick;
		if (mouse && !qobject_cast<QWidget*>(watched)) {
			return false;
		}
		switch (event->type()) {
		case QEvent::MouseMove: {
			QMouseEvent* move = static_cast<QMouseEvent*>(event);
			onMove(move->globalPosition().toPoint(), move->modifiers());
			if (!(move->buttons() & Qt::LeftButton)) {
				onFinish(true);
			}
			return true;
		}
		case QEvent::MouseButtonRelease:
			if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
				onFinish(true);
			}
			return true;
		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonDblClick:
			if (static_cast<QMouseEvent*>(event)->button() == Qt::RightButton) {
				onFinish(false);
			}
			return true;
		case QEvent::KeyPress:
			if (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
				onFinish(false);
				return true;
			}
			if (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Control) {
				onModifiers(false);
			}
			return false;
		case QEvent::KeyRelease:
			if (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Control) {
				onModifiers(true);
			}
			return false;
		default:
			return false;
		}
	}
};

QRect CMainFrame::controlRect(QToolBar* toolBar, const ToolBarControl& control) const
{
	QRect rect;
	for (QAction* action : control.actions) {
		if (QWidget* widget = toolBar->widgetForAction(action); widget && widget->isVisible()) {
			rect = rect.united(widget->geometry());
		}
	}
	return rect;
}

bool CMainFrame::isCustomizeBar(QWidget* widget) const
{
	if (QToolBar* toolBar = qobject_cast<QToolBar*>(widget)) {
		return m_toolBarItems.contains(toolBar);
	}
	if (QMenu* menu = qobject_cast<QMenu*>(widget)) {
		return menu->property("customizable").toBool();
	}
	return widget == menuBar();
}

QWidget* CMainFrame::dropTarget(QPoint globalPos) const
{
	for (QWidget* widget = QApplication::widgetAt(globalPos); widget; widget = widget->parentWidget()) {
		if (isCustomizeBar(widget)) {
			return widget->isVisible() ? widget : nullptr;
		}
		if (widget->isWindow()) {
			break;
		}
	}
	return nullptr;
}

std::vector<ToolBarControl>& CMainFrame::barControls(QWidget* bar)
{
	if (QToolBar* toolBar = qobject_cast<QToolBar*>(bar)) {
		return m_toolBarItems[toolBar];
	}
	if (QMenu* menu = qobject_cast<QMenu*>(bar)) {
		ensureMenuControls(menu);
		return m_menuItems[menu];
	}
	ensureMenuBarControls();
	return m_menuBarItems;
}

QRect CMainFrame::barControlRect(QWidget* bar, const ToolBarControl& control) const
{
	if (QToolBar* toolBar = qobject_cast<QToolBar*>(bar)) {
		return controlRect(toolBar, control);
	}
	if (QMenu* menu = qobject_cast<QMenu*>(bar)) {
		return menu->actionGeometry(control.actions.front());
	}
	return menuBar()->actionGeometry(control.actions.front());
}

bool CMainFrame::barVertical(QWidget* bar) const
{
	if (QToolBar* toolBar = qobject_cast<QToolBar*>(bar)) {
		return toolBar->orientation() == Qt::Vertical;
	}
	return qobject_cast<QMenu*>(bar) != nullptr;
}

void CMainFrame::findDropIndex(
	QWidget* bar, QPoint point, const ToolBarControl* dragged, QRect& marker, int& index, bool& after, bool& vertical
)
{
	const std::vector<ToolBarControl>& controls = barControls(bar);
	vertical = barVertical(bar);
	int count = static_cast<int>(controls.size());
	int offset = -1;
	int found = -1;
	for (int i = 0; i < count; i++) {
		QRect rc = barControlRect(bar, controls[i]);
		if (rc.isEmpty()) {
			continue;
		}
		int left = rc.left();
		int top = rc.top();
		int right = rc.left() + rc.width();
		int bottom = rc.top() + rc.height();
		bool nextGroup = i >= count - 1 || controls[i + 1].beginGroup;
		if (vertical) {
			int distance = std::abs(rc.center().y() - point.y());
			if (offset == -1 || offset > distance) {
				offset = distance;
				found = i;
				after = rc.center().y() < point.y();
				marker = !after      ? QRect(QPoint(left, top), QPoint(right - 1, top + 5))
						 : nextGroup ? QRect(QPoint(left, bottom - 6), QPoint(right - 1, bottom - 1))
									 : QRect(QPoint(left, bottom), QPoint(right - 1, bottom + 5));
			}
		}
		else {
			int distance = std::abs(rc.center().x() - point.x());
			if (offset == -1 || offset > distance) {
				offset = distance;
				found = i;
				after = rc.center().x() < point.x();
				marker = !after      ? QRect(QPoint(left, top), QPoint(left + 5, bottom - 1))
						 : nextGroup ? QRect(QPoint(right - 6, top), QPoint(right - 1, bottom - 1))
									 : QRect(QPoint(right, top), QPoint(right + 5, bottom - 1));
			}
		}
	}
	if (found == -1) {
		QRect rc = bar->rect();
		marker = QRect(QPoint(rc.left() + 2, rc.top() + 2), QPoint(rc.left() + 7, rc.bottom() - 2));
		after = true;
		index = -1;
		vertical = false;
	}
	else {
		index = found;
	}
	if (found != -1 && dragged && &controls[found] == dragged) {
		marker = QRect();
	}
}

void CMainFrame::showDropMarker(QWidget* bar, QRect marker, bool vertical)
{
	if (m_dropMarkerBar && (m_dropMarkerBar != bar || marker.isEmpty())) {
		if (QWidget* overlay =
				m_dropMarkerBar->findChild<QWidget*>(QStringLiteral("DropMarker"), Qt::FindDirectChildrenOnly)) {
			overlay->hide();
		}
		m_dropMarkerBar = nullptr;
	}
	if (!bar || marker.isEmpty()) {
		return;
	}
	DropMarker* overlay =
		static_cast<DropMarker*>(bar->findChild<QWidget*>(QStringLiteral("DropMarker"), Qt::FindDirectChildrenOnly));
	if (!overlay) {
		overlay = new DropMarker(bar);
	}
	overlay->setGeometry(bar->rect());
	overlay->setMarker(marker, vertical);
	overlay->raise();
	overlay->show();
	m_dropMarkerBar = bar;
}

void CMainFrame::openDragPopup(QWidget* bar, QPoint globalPos)
{
	QAction* action = nullptr;
	QPoint at;
	if (bar == menuBar()) {
		action = menuBar()->actionAt(menuBar()->mapFromGlobal(globalPos));
		if (action) {
			at = menuBar()->mapToGlobal(menuBar()->actionGeometry(action).bottomLeft());
		}
	}
	else if (QMenu* menu = qobject_cast<QMenu*>(bar)) {
		action = menu->actionAt(menu->mapFromGlobal(globalPos));
		if (action) {
			at = menu->mapToGlobal(menu->actionGeometry(action).topRight());
		}
	}
	if (!action || !action->menu() || action->menu()->isVisible()) {
		return;
	}
	closeDragPopups(bar);
	action->menu()->popup(at);
}

void CMainFrame::closeDragPopups(QWidget* keep)
{
	for (QMenu* open : findChildren<QMenu*>()) {
		if (!open->isVisible() || !open->property("customizable").toBool()) {
			continue;
		}
		bool ancestor = false;
		for (QWidget* chain = keep; chain; chain = chain->parentWidget()) {
			if (chain == open) {
				ancestor = true;
				break;
			}
		}
		if (!ancestor) {
			open->hide();
		}
	}
}

void CMainFrame::applyBarControl(QWidget* bar, ToolBarControl& control)
{
	if (QToolBar* toolBar = qobject_cast<QToolBar*>(bar)) {
		applyToolBarControl(toolBar, control);
	}
	else if (QMenu* menu = qobject_cast<QMenu*>(bar)) {
		applyMenuControl(menu, control);
	}
	else {
		QAction* action = control.actions.front();
		action->setText(controlCaption(control));
		if (QMenu* popup = action->menu()) {
			popup->setEnabled(true);
			action->setEnabled(true);
		}
	}
}

QAction* CMainFrame::newBarAction(QWidget* bar, const ToolBarControl& control)
{
	if (qobject_cast<QToolBar*>(bar)) {
		return toolBarAction(control.id);
	}
	if (QMenu* menu = qobject_cast<QMenu*>(bar)) {
		return menuCommandAction(menu, control.id, controlCaption(control));
	}
	return menuCommandAction(menuBar(), control.id, controlCaption(control));
}

void CMainFrame::rebuildBar(QWidget* bar, std::vector<ToolBarControl> controls)
{
	if (QToolBar* toolBar = qobject_cast<QToolBar*>(bar)) {
		rebuildToolBar(toolBar, controls);
		return;
	}
	QList<QAction*> current = bar->actions();
	for (QAction* action : current) {
		if (qobject_cast<QWidgetAction*>(action) && !action->property("expandButton").toBool()) {
			continue;
		}
		bar->removeAction(action);
		if (action->isSeparator() || qobject_cast<QWidgetAction*>(action)) {
			delete action;
		}
	}
	std::vector<ToolBarControl>& items =
		qobject_cast<QMenu*>(bar) ? m_menuItems[static_cast<QMenu*>(bar)] : m_menuBarItems;
	items.clear();
	for (ToolBarControl control : controls) {
		control.separator = nullptr;
		if (control.beginGroup && !items.empty() && qobject_cast<QMenu*>(bar)) {
			control.separator = static_cast<QMenu*>(bar)->addSeparator();
		}
		else {
			control.beginGroup = false;
		}
		bar->addAction(control.actions.front());
		items.push_back(control);
	}
	for (ToolBarControl& control : items) {
		applyBarControl(bar, control);
	}
	if (QMenu* menu = qobject_cast<QMenu*>(bar)) {
		m_customizedMenus.insert(menu);
	}
	else {
		m_menuBarCustomized = true;
	}
}

void CMainFrame::selectBarControl(QWidget* bar, const ToolBarControl& control)
{
	if (QToolBar* toolBar = qobject_cast<QToolBar*>(bar)) {
		QAction* identity = control.actions.front();
		QTimer::singleShot(0, this, [this, toolBar, identity]() {
			for (ToolBarControl& candidate : m_toolBarItems[toolBar]) {
				if (candidate.actions.front() == identity) {
					selectControl(toolBar, candidate);
				}
			}
		});
		return;
	}
	QAction* action = control.actions.front();
	bar->setProperty("customizeAction", QVariant::fromValue<QObject*>(action));
	if (QMenu* menu = qobject_cast<QMenu*>(bar)) {
		WhidbeyStyle::setCustomizeSelection(menu, menu->actionGeometry(action));
	}
	else {
		WhidbeyStyle::setCustomizeSelection(
			menuBar(), menuBar()->actionGeometry(action).translated(menuBar()->pos()).adjusted(0, 1, 0, 0)
		);
		menuBar()->setProperty("customizeAction", QVariant::fromValue<QObject*>(action));
	}
}

void CMainFrame::ensureMenuBarControls()
{
	std::vector<ToolBarControl> previous = std::move(m_menuBarItems);
	m_menuBarItems.clear();
	for (QAction* action : menuBar()->actions()) {
		if (action->isSeparator()) {
			continue;
		}
		ToolBarControl control;
		auto old = std::find_if(previous.begin(), previous.end(), [action](const ToolBarControl& entry) {
			return entry.actions.front() == action;
		});
		if (old != previous.end()) {
			control = *old;
		}
		else {
			control.id = action->data().toUInt();
			control.actions = {action};
			control.caption = action->text();
		}
		m_menuBarItems.push_back(control);
	}
}

void CMainFrame::rebuildToolBar(QToolBar* toolBar, std::vector<ToolBarControl> controls)
{
	for (ToolBarControl& control : m_toolBarItems[toolBar]) {
		setControlBeginGroup(toolBar, control, false);
		for (QAction* action : control.actions) {
			toolBar->removeAction(action);
		}
	}
	m_toolBarItems[toolBar].clear();
	for (const ToolBarControl& control : controls) {
		ToolBarControl added = control;
		added.beginGroup = false;
		added.separator = nullptr;
		for (QAction* action : added.actions) {
			toolBar->addAction(action);
		}
		m_toolBarItems[toolBar].push_back(added);
	}
	for (size_t i = 0; i < controls.size(); i++) {
		ToolBarControl& control = m_toolBarItems[toolBar][i];
		if (controls[i].beginGroup) {
			setControlBeginGroup(toolBar, control, true);
		}
		applyToolBarControl(toolBar, control);
	}
	toolBar->update();
}

CMainFrame::DragOutcome CMainFrame::runDragLoop(
	QWidget* grabber, QPoint start, bool copyOnly, const std::function<const ToolBarControl*(QWidget*)>& dragged
)
{
	QCursor deleteCursor = Resources::cursor(dragDeleteCursorId);
	QCursor moveCursor = Resources::cursor(dragMoveCursorId);
	QCursor copyCursor = Resources::cursor(dragCopyCursorId);
	DragOutcome outcome;
	outcome.last = start;
	QEventLoop loop;
	ControlDragFilter filter;
	auto freshenCursor = [&]() {
		const QCursor& cursor = outcome.target ? (outcome.move && !copyOnly ? moveCursor : copyCursor) : deleteCursor;
		if (QApplication::overrideCursor()) {
			QApplication::changeOverrideCursor(cursor);
		}
		else {
			QApplication::setOverrideCursor(cursor);
		}
	};
	filter.onMove = [&](QPoint position, Qt::KeyboardModifiers modifiers) {
		outcome.moved = true;
		outcome.move = !(modifiers & Qt::ControlModifier);
		outcome.last = position;
		outcome.target = dropTarget(position);
		if (QWidget* bar = outcome.target) {
			openDragPopup(bar, position);
			QRect marker;
			int index = -1;
			bool after = false;
			bool vertical = false;
			findDropIndex(bar, bar->mapFromGlobal(position), dragged(bar), marker, index, after, vertical);
			showDropMarker(bar, marker, vertical);
		}
		else {
			showDropMarker(nullptr, QRect(), false);
		}
		freshenCursor();
	};
	filter.onModifiers = [&](bool moving) {
		outcome.move = moving;
		if (outcome.moved) {
			freshenCursor();
		}
	};
	filter.onFinish = [&](bool drop) {
		outcome.dropped = drop;
		loop.quit();
	};
	grabber->grabMouse();
	qApp->installEventFilter(&filter);
	loop.exec();
	qApp->removeEventFilter(&filter);
	grabber->releaseMouse();
	showDropMarker(nullptr, QRect(), false);
	if (QApplication::overrideCursor()) {
		QApplication::restoreOverrideCursor();
	}
	return outcome;
}

void CMainFrame::dragCommand(
	unsigned id,
	const QString& caption,
	bool popup,
	unsigned menuResource,
	int menuIndex,
	QWidget* grabber,
	QPoint start
)
{
	DragOutcome outcome = runDragLoop(grabber, start, true, [](QWidget*) -> const ToolBarControl* { return nullptr; });
	if (!outcome.dropped || !outcome.moved || !outcome.target) {
		closeDragPopups(nullptr);
		return;
	}
	QWidget* target = outcome.target;
	QRect marker;
	int index = -1;
	bool after = false;
	bool vertical = false;
	findDropIndex(target, target->mapFromGlobal(outcome.last), nullptr, marker, index, after, vertical);
	std::vector<ToolBarControl> controls = barControls(target);
	if (after) {
		index++;
	}
	ToolBarControl control;
	control.id = id;
	control.caption = caption;
	if (!after && index >= 0 && index < static_cast<int>(controls.size())) {
		control.beginGroup = controls[index].beginGroup;
		controls[index].beginGroup = false;
	}
	QAction* action = popup ? createPopupMenu(caption, menuResource, menuIndex, QString())->menuAction()
							: newBarAction(target, control);
	control.actions = {action};
	index = std::clamp(index, 0, static_cast<int>(controls.size()));
	controls.insert(controls.begin() + index, control);
	rebuildBar(target, controls);
	reparentPopups(target);
	closeDragPopups(qobject_cast<QMenu*>(target));
	for (ToolBarControl& added : barControls(target)) {
		if (added.actions.front() == action) {
			selectBarControl(target, added);
		}
	}
}

bool CMainFrame::dragControl(QWidget* button, QPoint start)
{
	QToolBar* toolBar = nullptr;
	ToolBarControl* source = toolBarControl(button, &toolBar);
	if (!source) {
		return false;
	}
	return dragBarControl(toolBar, source->actions.front(), button, start);
}

bool CMainFrame::dragBarControl(QWidget* bar, QAction* identity, QWidget* grabber, QPoint start)
{
	auto locate = [this, identity](QWidget*& found) -> int {
		auto search = [&](QWidget* candidate, const std::vector<ToolBarControl>& controls) {
			for (size_t i = 0; i < controls.size(); i++) {
				if (controls[i].actions.front() == identity) {
					found = candidate;
					return static_cast<int>(i);
				}
			}
			return -1;
		};
		for (auto& [candidate, controls] : m_toolBarItems) {
			if (int index = search(candidate, controls); index >= 0) {
				return index;
			}
		}
		for (auto& [candidate, controls] : m_menuItems) {
			if (int index = search(candidate, controls); index >= 0) {
				return index;
			}
		}
		return search(menuBar(), m_menuBarItems);
	};

	barControls(bar);
	QWidget* sourceBar = nullptr;
	int sourceIndex = locate(sourceBar);
	if (sourceIndex < 0) {
		return false;
	}
	selectBarControl(sourceBar, barControls(sourceBar)[sourceIndex]);

	DragOutcome outcome = runDragLoop(grabber, start, false, [this, locate](QWidget* target) -> const ToolBarControl* {
		QWidget* found = nullptr;
		int index = locate(found);
		return found == target && index >= 0 ? &barControls(target)[index] : nullptr;
	});
	bool move = outcome.move;
	QWidget* target = outcome.target;
	QPoint last = outcome.last;
	if (!outcome.dropped || !outcome.moved) {
		return outcome.moved;
	}

	sourceIndex = locate(sourceBar);
	if (sourceIndex < 0) {
		return true;
	}
	std::vector<ToolBarControl> sourceControls = barControls(sourceBar);
	ToolBarControl dragged = sourceControls[sourceIndex];
	std::vector<ToolBarControl> targetControls = target ? barControls(target) : std::vector<ToolBarControl>();
	bool removeSource = !target || move;
	bool sameType = target &&
					(qobject_cast<QToolBar*>(target) != nullptr) == (qobject_cast<QToolBar*>(sourceBar) != nullptr) &&
					(target == menuBar()) == (sourceBar == menuBar());
	QAction* cloneAction = nullptr;
	QAction* deleteAction = nullptr;

	if (target) {
		QRect marker;
		int index = -1;
		bool after = false;
		bool vertical = false;
		QPoint point = target->mapFromGlobal(last);
		findDropIndex(target, point, nullptr, marker, index, after, vertical);
		if (index != -1 && target == sourceBar && index == sourceIndex) {
			QPoint from = target->mapFromGlobal(start);
			if (std::abs(from.x() - point.x()) > 4 || std::abs(from.y() - point.y()) > 4) {
				std::vector<ToolBarControl> controls = barControls(target);
				controls[index].beginGroup = after && index != 0;
				rebuildBar(target, controls);
				selectBarControl(target, barControls(target)[index]);
			}
			return true;
		}
		ToolBarControl clone = dragged;
		clone.separator = nullptr;
		if (QMenu* popup = dragged.actions.front()->menu()) {
			for (QWidget* chain = target; chain; chain = chain->parentWidget()) {
				if (chain == popup) {
					return true;
				}
			}
			if (!move) {
				clone.actions = {clonePopupMenu(popup)->menuAction()};
			}
		}
		else if (!move || !sameType) {
			if (dragged.actions.size() != 1 || qobject_cast<QWidgetAction*>(dragged.actions.front())) {
				return true;
			}
			clone.actions = {newBarAction(target, dragged)};
			if (move) {
				deleteAction = dragged.actions.front();
			}
		}
		if (after) {
			index++;
		}
		bool beginGroup = false;
		if (!after && index >= 0 && index < static_cast<int>(targetControls.size())) {
			beginGroup = targetControls[index].beginGroup;
			targetControls[index].beginGroup = false;
		}
		clone.beginGroup = beginGroup;
		cloneAction = clone.actions.front();
		index = std::clamp(index, 0, static_cast<int>(targetControls.size()));
		targetControls.insert(targetControls.begin() + index, clone);
		if (target == sourceBar) {
			sourceControls = targetControls;
			if (index <= sourceIndex) {
				sourceIndex++;
			}
		}
	}

	if (removeSource) {
		int count = static_cast<int>(sourceControls.size());
		if (sourceControls[sourceIndex].beginGroup && sourceIndex < count - 1) {
			sourceControls[sourceIndex + 1].beginGroup = true;
		}
		if (sourceIndex == 0 && count > 1) {
			sourceControls[1].beginGroup = false;
		}
		ToolBarControl removed = sourceControls[sourceIndex];
		sourceControls.erase(sourceControls.begin() + sourceIndex);
		if (target == sourceBar) {
			targetControls = sourceControls;
		}
		if (!target) {
			deleteAction = removed.actions.front();
		}
	}

	if (target != sourceBar) {
		rebuildBar(sourceBar, sourceControls);
	}
	if (target) {
		rebuildBar(target, targetControls);
		reparentPopups(target);
	}
	if (deleteAction && !qobject_cast<QWidgetAction*>(deleteAction)) {
		sourceBar->removeAction(deleteAction);
		if (qobject_cast<QToolBar*>(sourceBar)) {
			for (auto it = m_toolBarActions.begin(); it != m_toolBarActions.end(); ++it) {
				if (it->second == deleteAction) {
					m_toolBarActions.erase(it);
					break;
				}
			}
		}
		if (!deleteAction->menu() && !isDefaultToolBarAction(deleteAction)) {
			deleteAction->deleteLater();
		}
	}
	closeDragPopups(qobject_cast<QMenu*>(target));
	if (target) {
		for (ToolBarControl& control : barControls(target)) {
			if (control.actions.front() == cloneAction) {
				selectBarControl(target, control);
			}
		}
	}
	else {
		WhidbeyStyle::setCustomizeSelection(nullptr);
	}
	return true;
}

void CMainFrame::ensureMenuControls(QMenu* menu)
{
	std::vector<ToolBarControl>& controls = m_menuItems[menu];
	std::vector<ToolBarControl> previous = std::move(controls);
	controls.clear();
	QAction* separator = nullptr;
	for (QAction* action : menu->actions()) {
		if (action->isSeparator()) {
			separator = action;
			continue;
		}
		if (qobject_cast<QWidgetAction*>(action) || action->property("windowList").toBool() ||
			action->property("toolBarList").toBool()) {
			continue;
		}
		ToolBarControl control;
		auto old = std::find_if(previous.begin(), previous.end(), [action](const ToolBarControl& entry) {
			return entry.actions.front() == action;
		});
		if (old != previous.end()) {
			control = *old;
		}
		else {
			control.id = action->data().toUInt();
			control.actions = {action};
			control.caption = action->text().section('\t', 0, 0);
		}
		control.beginGroup = separator != nullptr && !controls.empty();
		control.separator = control.beginGroup ? separator : nullptr;
		controls.push_back(control);
		separator = nullptr;
	}
}

void CMainFrame::applyMenuControl(QMenu* menu, ToolBarControl& control)
{
	m_customizedMenus.insert(menu);
	QAction* action = control.actions.front();
	QString text = action->text();
	qsizetype tab = text.indexOf(QLatin1Char('\t'));
	QString shortcut = tab < 0 ? acceleratorText(control.id) : text.mid(tab + 1);
	action->setText(controlCaption(control) + (shortcut.isEmpty() ? QString() : QLatin1Char('\t') + shortcut));
	QIcon icon = control.customIcon.isNull() ? ImageManager::instance().icon(control.id) : control.customIcon;
	action->setIcon(control.customStyle == Qt::ToolButtonTextOnly ? QIcon() : icon);
	if (WhidbeyStyle::customizeMode()) {
		WhidbeyStyle::setCustomizeSelection(menu, menu->actionGeometry(action));
	}
}

ToolBarControl* CMainFrame::selectedBarControl(QWidget* bar)
{
	QObject* selected = bar->property("customizeAction").value<QObject*>();
	for (ToolBarControl& control : barControls(bar)) {
		if (control.actions.front() == selected) {
			return &control;
		}
	}
	return nullptr;
}

void CMainFrame::deleteBarControl(QWidget* bar, QAction* identity)
{
	WhidbeyStyle::setCustomizeSelection(nullptr);
	std::vector<ToolBarControl> controls = barControls(bar);
	auto found = std::find_if(controls.begin(), controls.end(), [identity](const ToolBarControl& control) {
		return control.actions.front() == identity;
	});
	if (found == controls.end()) {
		return;
	}
	controls.erase(found);
	rebuildBar(bar, controls);
	if (!identity->menu() && !isDefaultToolBarAction(identity)) {
		identity->deleteLater();
	}
}

bool CMainFrame::isDefaultToolBarAction(QAction* action) const
{
	for (const auto& [toolBar, defaults] : m_toolBarDefaults) {
		for (const ToolBarControl& control : defaults) {
			if (std::find(control.actions.begin(), control.actions.end(), action) != control.actions.end()) {
				return true;
			}
		}
	}
	return false;
}

void CMainFrame::setBarControlBeginGroup(QWidget* bar, QAction* identity, bool beginGroup)
{
	std::vector<ToolBarControl> controls = barControls(bar);
	for (ToolBarControl& control : controls) {
		if (control.actions.front() == identity) {
			control.beginGroup = beginGroup;
		}
	}
	rebuildBar(bar, controls);
	for (ToolBarControl& control : barControls(bar)) {
		if (control.actions.front() == identity) {
			selectBarControl(bar, control);
		}
	}
}

void CMainFrame::setupMenu(QMenu* menu)
{
	menu->setProperty("customizable", true);
	connect(menu, &QMenu::aboutToShow, this, [this, menu]() { updateMenu(menu); });
	connect(menu, &QMenu::aboutToHide, this, [this, menu]() {
		m_hiddenMenu = menu;
		m_hiddenMenuTimer.start();
	});
	menu->installEventFilter(this);
	menu->setWindowFlag(Qt::NoDropShadowWindowHint, WhidbeyStyle::customizeMode());
}

QMenu* CMainFrame::menuByKey(const QString& key) const
{
	for (QMenu* menu : findChildren<QMenu*>()) {
		if (menu->property("menuKey").toString() == key) {
			return menu;
		}
	}
	return nullptr;
}

QMenu* CMainFrame::createPopupMenu(const QString& caption, unsigned resource, int index, QString key)
{
	QString base = caption;
	std::span<const Resources::MenuEntry> entries = Resources::menu(resource);
	if (index >= 0 && index < static_cast<int>(entries.size())) {
		base = QString::fromLatin1(entries[index].text);
	}
	if (key.isEmpty()) {
		for (int n = 1; key.isEmpty() || menuByKey(key); n++) {
			key = base + QLatin1Char('#') + QString::number(n);
		}
	}
	QMenu* menu = new QMenu(caption, this);
	menu->setProperty("menuKey", key);
	if (index >= 0 && index < static_cast<int>(entries.size())) {
		MenuIterator it = entries.begin() + index + 1;
		buildMenu(menu, it, entries.end());
		menu->setWindowFlag(Qt::NoDropShadowWindowHint, WhidbeyStyle::customizeMode());
	}
	else {
		setupMenu(menu);
	}
	m_createdMenus[key] = {caption, resource, index};
	m_customizedMenus.insert(menu);
	return menu;
}

QMenu* CMainFrame::clonePopupMenu(QMenu* source)
{
	QMenu* menu = createPopupMenu(source->title(), 0, -1, QString());
	ensureMenuControls(source);
	std::vector<ToolBarControl> controls;
	for (const ToolBarControl& control : m_menuItems[source]) {
		ToolBarControl copy = control;
		copy.separator = nullptr;
		QAction* original = control.actions.front();
		copy.actions = {
			original->menu() ? clonePopupMenu(original->menu())->menuAction()
							 : menuCommandAction(menu, control.id, controlCaption(control))
		};
		controls.push_back(copy);
	}
	rebuildBar(menu, controls);
	reparentPopups(menu);
	return menu;
}

void CMainFrame::reparentPopups(QWidget* bar)
{
	for (ToolBarControl& control : barControls(bar)) {
		QMenu* popup = control.actions.front()->menu();
		if (popup && popup->parentWidget() != bar) {
			popup->setParent(bar, popup->windowFlags());
		}
	}
}

void CMainFrame::restoreMenuBar(const QVariantList& entries)
{
	std::vector<QAction*> pool;
	for (QAction* action : menuBar()->actions()) {
		menuBar()->removeAction(action);
		pool.push_back(action);
	}
	std::vector<ToolBarControl> controls;
	for (const QVariant& value : entries) {
		QVariantMap entry = value.toMap();
		unsigned id = entry.value(QStringLiteral("Id")).toUInt();
		QString key = entry.value(QStringLiteral("Menu")).toString();
		QAction* action = nullptr;
		if (!key.isEmpty()) {
			if (QMenu* popup = menuByKey(key)) {
				action = popup->menuAction();
			}
		}
		else {
			auto found = std::find_if(pool.begin(), pool.end(), [id](QAction* candidate) {
				return !candidate->menu() && candidate->data().toUInt() == id;
			});
			action = found != pool.end()
						 ? *found
						 : menuCommandAction(menuBar(), id, entry.value(QStringLiteral("BaseCaption")).toString());
		}
		if (!action) {
			continue;
		}
		ToolBarControl control;
		control.id = id;
		control.actions = {action};
		control.caption = entry.value(QStringLiteral("BaseCaption")).toString();
		control.customCaption = entry.value(QStringLiteral("Caption")).toString();
		controls.push_back(control);
	}
	for (QAction* action : pool) {
		bool used = std::any_of(controls.begin(), controls.end(), [action](const ToolBarControl& control) {
			return control.actions.front() == action;
		});
		if (!used && !action->menu()) {
			delete action;
		}
	}
	rebuildBar(menuBar(), controls);
	reparentPopups(menuBar());
}

void CMainFrame::restoreMenuControls(QMenu* menu, const QVariantList& entries)
{
	std::vector<QAction*> pool;
	for (QAction* action : menu->actions()) {
		if (qobject_cast<QWidgetAction*>(action) && !action->property("expandButton").toBool()) {
			continue;
		}
		menu->removeAction(action);
		if (action->isSeparator() || qobject_cast<QWidgetAction*>(action)) {
			delete action;
		}
		else {
			pool.push_back(action);
		}
	}

	std::vector<ToolBarControl>& controls = m_menuItems[menu];
	controls.clear();
	for (const QVariant& value : entries) {
		QVariantMap entry = value.toMap();
		unsigned id = entry.value(QStringLiteral("Id")).toUInt();
		QString key = entry.value(QStringLiteral("Menu")).toString();
		auto found = std::find_if(pool.begin(), pool.end(), [id, &key](QAction* action) {
			return key.isEmpty() ? !action->menu() && action->data().toUInt() == id
								 : action->menu() && action->menu()->property("menuKey").toString() == key;
		});
		QAction* action = nullptr;
		if (found != pool.end()) {
			action = *found;
			pool.erase(found);
		}
		else if (!key.isEmpty()) {
			QMenu* popup = menuByKey(key);
			if (!popup) {
				continue;
			}
			action = popup->menuAction();
		}
		else if (!Resources::string(id).isEmpty() || !ImageManager::instance().icon(id).isNull()) {
			action = menuCommandAction(menu, id, entry.value(QStringLiteral("BaseCaption")).toString());
		}
		else {
			continue;
		}

		ToolBarControl control;
		control.id = id;
		control.actions = {action};
		control.caption = entry.value(QStringLiteral("BaseCaption")).toString();
		control.customCaption = entry.value(QStringLiteral("Caption")).toString();
		if (entry.contains(QStringLiteral("Style"))) {
			control.customStyle = static_cast<Qt::ToolButtonStyle>(entry.value(QStringLiteral("Style")).toInt());
		}
		QPixmap icon;
		if (icon.loadFromData(entry.value(QStringLiteral("Icon")).toByteArray(), "PNG")) {
			control.customIcon = QIcon(icon);
			control.customIconAlpha = entry.value(QStringLiteral("IconAlpha")).toBool();
		}
		if (entry.value(QStringLiteral("BeginGroup")).toBool() && !controls.empty()) {
			control.beginGroup = true;
			control.separator = menu->addSeparator();
		}
		menu->addAction(action);
		controls.push_back(control);
		applyMenuControl(menu, controls.back());
	}

	for (QAction* action : pool) {
		if (!action->menu()) {
			delete action;
		}
	}
	m_customizedMenus.insert(menu);
	reparentPopups(menu);
	WhidbeyStyle::setCustomizeSelection(nullptr);
}

void CMainFrame::showCustomizeContextMenu(const QPoint& pos)
{
	QToolBar* toolBar = nullptr;
	QWidget* selection = WhidbeyStyle::customizeSelection();
	QWidget* popupMenu = qobject_cast<QMenu*>(selection) || (selection && selection == menuBar()) ? selection : nullptr;
	ToolBarControl* selected = popupMenu ? selectedBarControl(popupMenu) : toolBarControl(selection, &toolBar);
	if (!selected) {
		return;
	}
	unsigned selectedId = selected->id;
	QAction* identity = selected->actions.front();
	const std::vector<ToolBarControl>& siblings = popupMenu ? barControls(popupMenu) : m_toolBarItems[toolBar];
	bool firstControl = !siblings.empty() && siblings.front().actions.front() == identity;
	selected = popupMenu ? selectedBarControl(popupMenu) : selected;
	auto current = [this, selectedId, popupMenu, identity]() -> ToolBarControl* {
		QToolBar* bar = nullptr;
		ToolBarControl* control =
			popupMenu ? selectedBarControl(popupMenu) : toolBarControl(WhidbeyStyle::customizeSelection(), &bar);
		return control && control->id == selectedId && control->actions.front() == identity ? control : nullptr;
	};
	auto apply = [this, toolBar, popupMenu](ToolBarControl& control) {
		if (popupMenu) {
			applyBarControl(popupMenu, control);
		}
		else {
			applyToolBarControl(toolBar, control);
		}
	};

	QToolButton* button = popupMenu ? nullptr : controlButton(toolBar, *selected);
	bool drawImage = qobject_cast<QMenu*>(popupMenu) || button != nullptr;
	bool hasIcon = drawImage && !ImageManager::instance().icon(selected->id).isNull();

	QMenu menu(this);
	std::span<const Resources::MenuEntry> entries = Resources::menu(customizeMenuId);
	bool separator = false;
	for (auto it = entries.begin() + 1; it != entries.end() && it->kind != Resources::MENU_END; ++it) {
		if (it->kind == Resources::MENU_SEPARATOR) {
			separator = true;
			continue;
		}
		if (separator) {
			menu.addSeparator();
			separator = false;
		}
		unsigned id = it->id;
		QString text = QString::fromLatin1(it->text);
		if (id == customizeNameId) {
			QWidgetAction* action = new QWidgetAction(&menu);
			MenuEditItem* item = new MenuEditItem(text, controlCaption(*selected), &menu);
			QLineEdit* edit = item->edit();
			action->setDefaultWidget(item);
			connect(edit, &QLineEdit::returnPressed, this, [apply, current, edit, &menu]() {
				if (ToolBarControl* control = current()) {
					control->customCaption = edit->text();
					apply(*control);
				}
				menu.close();
			});
			menu.addAction(action);
			continue;
		}
		if (id == customizeChangeImageId) {
			QMenu* images = menu.addMenu(text);
			images->setEnabled(hasIcon);
			QImage source =
				QImage(Resources::bitmapPath(customizeIconsBitmapId)).convertToFormat(QImage::Format_ARGB32);
			std::vector<QIcon> icons;
			for (int x = 0; x + customIconSize <= source.width(); x += customIconSize) {
				QImage image = source.copy(x, 0, customIconSize, customIconSize);
				for (int py = 0; py < image.height(); py++) {
					for (int px = 0; px < image.width(); px++) {
						if (image.pixel(px, py) == customIconMask) {
							image.setPixel(px, py, qRgba(0, 0, 0, 0));
						}
					}
				}
				icons.emplace_back(QPixmap::fromImage(image));
			}
			QWidgetAction* grid = new QWidgetAction(images);
			MenuIconGrid* widget = new MenuIconGrid(std::move(icons), images);
			widget->onSelected = [apply, current](const QIcon& icon) {
				if (ToolBarControl* control = current()) {
					control->customIcon = icon;
					control->customIconAlpha = false;
					apply(*control);
				}
			};
			grid->setDefaultWidget(widget);
			images->addAction(grid);
			continue;
		}

		QAction* action = menu.addAction(text);
		switch (id) {
		case customizeResetId:
			action->setEnabled(
				!selected->customIcon.isNull() || !selected->customCaption.isEmpty() || selected->customStyle
			);
			connect(action, &QAction::triggered, this, [apply, current]() {
				if (ToolBarControl* control = current()) {
					control->customIcon = QIcon();
					control->customCaption.clear();
					control->customStyle.reset();
					apply(*control);
				}
			});
			break;
		case customizeDeleteId:
			connect(action, &QAction::triggered, this, [this, toolBar, popupMenu, current]() {
				if (ToolBarControl* control = current()) {
					if (popupMenu) {
						deleteBarControl(popupMenu, control->actions.front());
					}
					else {
						deleteToolBarControl(toolBar, control);
					}
				}
			});
			break;
		case customizeCopyImageId:
			action->setEnabled(hasIcon);
			connect(action, &QAction::triggered, this, [this, toolBar, popupMenu, current]() {
				if (ToolBarControl* control = current()) {
					QToolButton* button = popupMenu ? nullptr : controlButton(toolBar, *control);
					QSize size = button ? button->iconSize() : QSize(16, 16);
					QImage image(size, QImage::Format_RGB32);
					image.fill(palette().color(QPalette::Button));
					QPainter painter(&image);
					QIcon icon =
						control->customIcon.isNull() ? ImageManager::instance().icon(control->id) : control->customIcon;
					icon.paint(&painter, image.rect());
					painter.end();
					QApplication::clipboard()->setImage(image);
				}
			});
			break;
		case customizePasteImageId:
			action->setEnabled(
				drawImage && QApplication::clipboard()->mimeData() && QApplication::clipboard()->mimeData()->hasImage()
			);
			connect(action, &QAction::triggered, this, [this, apply, current]() {
				ToolBarControl* control = current();
				QImage source = QApplication::clipboard()->image();
				bool alpha = source.hasAlphaChannel();
				QImage image = source.convertToFormat(QImage::Format_ARGB32);
				if (!control || image.isNull()) {
					return;
				}
				QRgb face = palette().color(QPalette::Button).rgb();
				for (int y = 0; y < image.height() && !alpha; y++) {
					for (int x = 0; x < image.width(); x++) {
						if ((image.pixel(x, y) & 0xffffff) == (face & 0xffffff)) {
							image.setPixel(x, y, qRgba(0, 0, 0, 0));
						}
					}
				}
				control->customIcon = QIcon(QPixmap::fromImage(image));
				control->customIconAlpha = alpha;
				apply(*control);
			});
			break;
		case customizeResetImageId:
			action->setEnabled(!selected->customIcon.isNull());
			connect(action, &QAction::triggered, this, [apply, current]() {
				if (ToolBarControl* control = current()) {
					control->customIcon = QIcon();
					apply(*control);
				}
			});
			break;
		case customizeEditImageId:
			action->setEnabled(hasIcon);
			connect(action, &QAction::triggered, this, [this, apply, current]() {
				ToolBarControl* control = current();
				if (!control) {
					return;
				}
				QIcon icon =
					control->customIcon.isNull() ? ImageManager::instance().icon(control->id) : control->customIcon;
				QImage image = icon.pixmap(icon.actualSize(QSize(editImageSize, editImageSize))).toImage();
				bool alpha = control->customIcon.isNull() ? ImageManager::instance().isAlpha(control->id)
														  : control->customIconAlpha;
				ImageEditorDialog dialog(image, alpha, m_customizeSheet ? m_customizeSheet : this);
				if (dialog.DoModal() == IDOK && dialog.hasImage()) {
					if (ToolBarControl* edited = current()) {
						edited->customIcon = QIcon(QPixmap::fromImage(dialog.image()));
						edited->customIconAlpha = dialog.alpha();
						apply(*edited);
					}
				}
			});
			break;
		case customizeDefaultStyleId:
		case customizeTextOnlyId:
		case customizeImageAndTextId: {
			Qt::ToolButtonStyle style = id == customizeTextOnlyId       ? Qt::ToolButtonTextOnly
										: id == customizeImageAndTextId ? Qt::ToolButtonTextBesideIcon
																		: Qt::ToolButtonIconOnly;
			action->setCheckable(true);
			action->setEnabled(hasIcon);
			action->setChecked(
				hasIcon ? selected->customStyle.value_or(Qt::ToolButtonIconOnly) == style
						: id == customizeDefaultStyleId
			);
			connect(action, &QAction::triggered, this, [apply, current, style]() {
				if (ToolBarControl* control = current()) {
					control->customStyle = style;
					apply(*control);
				}
			});
			break;
		}
		case customizeBeginGroupId: {
			action->setCheckable(true);
			action->setChecked(selected->beginGroup);
			action->setEnabled(!firstControl);
			connect(action, &QAction::triggered, this, [this, toolBar, popupMenu, current]() {
				if (ToolBarControl* control = current()) {
					if (popupMenu) {
						setBarControlBeginGroup(popupMenu, control->actions.front(), !control->beginGroup);
					}
					else {
						setControlBeginGroup(toolBar, *control, !control->beginGroup);
					}
				}
			});
			break;
		}
		}
	}
	if (m_customizeSheet) {
		CustomizePopupKeys keys(&menu, m_customizeSheet);
		qApp->installEventFilter(&keys);
		menu.exec(pos);
		qApp->removeEventFilter(&keys);
	}
	else {
		menu.exec(pos);
	}
}

QString CMainFrame::acceleratorText(unsigned id) const
{
	QString text;
	int best = 0;
	for (const Resources::Accelerator& accelerator : Resources::accelerators(mainResourceId)) {
		if (accelerator.id != id) {
			continue;
		}
		QKeySequence sequence = acceleratorSequence(accelerator);
		if (sequence.isEmpty()) {
			continue;
		}
		int priority = (accelerator.flags & Resources::ACCEL_CONTROL) ? 3
					   : (accelerator.flags & Resources::ACCEL_ALT)   ? 2
					   : (accelerator.flags & Resources::ACCEL_SHIFT) ? 1
																	  : 4;
		if (text.isEmpty() || priority > best) {
			text = sequence.toString(QKeySequence::NativeText);
			best = priority;
		}
	}
	return text;
}

bool CMainFrame::eventFilter(QObject* watched, QEvent* event)
{
	if (QMenu* menu = qobject_cast<QMenu*>(watched);
		menu && WhidbeyStyle::customizeMode() && menu->property("customizable").toBool() &&
		(event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease ||
		 event->type() == QEvent::MouseButtonDblClick)) {
		QAction* action = menu->actionAt(static_cast<QMouseEvent*>(event)->position().toPoint());
		if (action && !action->isSeparator()) {
			if (event->type() == QEvent::MouseButtonPress) {
				menu->setProperty("customizeAction", QVariant::fromValue<QObject*>(action));
				WhidbeyStyle::setCustomizeSelection(menu, menu->actionGeometry(action));
				QPoint pos = static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
				if (static_cast<QMouseEvent*>(event)->button() == Qt::RightButton) {
					QTimer::singleShot(0, this, [this, pos]() { showCustomizeContextMenu(pos); });
				}
				else if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
					if (!dragBarControl(menu, action, menu, pos) && action->menu() && menu->isVisible()) {
						action->menu()->popup(menu->mapToGlobal(menu->actionGeometry(action).topRight()));
					}
				}
			}
			return true;
		}
	}
	if (watched == menuBar() && WhidbeyStyle::customizeMode() && event->type() == QEvent::MouseButtonPress) {
		if (QAction* action = menuBar()->actionAt(static_cast<QMouseEvent*>(event)->position().toPoint())) {
			WhidbeyStyle::setCustomizeSelection(
				menuBar(), menuBar()->actionGeometry(action).translated(menuBar()->pos()).adjusted(0, 1, 0, 0)
			);
			menuBar()->setProperty("customizeAction", QVariant::fromValue<QObject*>(action));
			QPoint pos = static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
			if (static_cast<QMouseEvent*>(event)->button() == Qt::RightButton) {
				QTimer::singleShot(0, this, [this, pos]() { showCustomizeContextMenu(pos); });
				return true;
			}
			if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
				if (action->menu() && action->menu()->isVisible()) {
					action->menu()->hide();
					return true;
				}
				if (action->menu() && action->menu() == m_hiddenMenu && m_hiddenMenuTimer.isValid() &&
					m_hiddenMenuTimer.elapsed() < menuReplayInterval) {
					return true;
				}
				ensureMenuBarControls();
				if (!dragBarControl(menuBar(), action, menuBar(), pos) && action->menu()) {
					menuBar()->setActiveAction(action);
				}
				return true;
			}
		}
	}
	QToolBar* customizeBar = nullptr;
	QWidget* customizeOwner = nullptr;
	ToolBarControl* customizeControl = nullptr;
	if (WhidbeyStyle::customizeMode() && qobject_cast<QWidget*>(watched)) {
		customizeControl = controlForWidget(static_cast<QWidget*>(watched), &customizeBar, &customizeOwner);
	}
	if (customizeControl) {
		{
			switch (event->type()) {
			case QEvent::MouseButtonPress:
				selectControl(customizeBar, *customizeControl);
				if (static_cast<QMouseEvent*>(event)->button() == Qt::RightButton) {
					QPoint pos = static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
					QTimer::singleShot(0, this, [this, pos]() { showCustomizeContextMenu(pos); });
				}
				else if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
					QToolButton* popupButton = qobject_cast<QToolButton*>(customizeOwner);
					if (!dragControl(customizeOwner, static_cast<QMouseEvent*>(event)->globalPosition().toPoint()) &&
						popupButton && popupButton->defaultAction() && popupButton->defaultAction()->menu()) {
						QTimer::singleShot(0, popupButton, [popupButton]() { popupButton->showMenu(); });
					}
				}
				return true;
			case QEvent::MouseButtonRelease:
			case QEvent::MouseButtonDblClick:
			case QEvent::ContextMenu:
			case QEvent::ToolTip:
				return true;
			default:
				break;
			}
		}
	}
	if (event->type() == QEvent::ContextMenu && !WhidbeyStyle::customizeMode()) {
		QWidget* widget = qobject_cast<QWidget*>(watched);
		QToolBar* toolBar = qobject_cast<QToolBar*>(widget);
		if (!toolBar && qobject_cast<QToolButton*>(widget)) {
			toolBar = qobject_cast<QToolBar*>(widget->parentWidget());
		}
		if (widget == menuBar() || m_toolBarItems.contains(toolBar)) {
			showToolBarsContextMenu(static_cast<QContextMenuEvent*>(event)->globalPos());
			return true;
		}
	}
	if (event->type() == QEvent::MouseButtonPress && !m_gameLayout &&
		static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton &&
		(static_cast<QMouseEvent*>(event)->modifiers() & Qt::AltModifier)) {
		if (QToolButton* button = qobject_cast<QToolButton*>(watched);
			button && m_toolBarItems.contains(qobject_cast<QToolBar*>(button->parentWidget()))) {
			QToolBar* owner = nullptr;
			if (!toolBarControl(button, &owner)) {
				return false;
			}
			setCustomizeMode(true, true);
			dragControl(button, static_cast<QMouseEvent*>(event)->globalPosition().toPoint());
			setCustomizeMode(false, true);
			return true;
		}
	}
	if (event->type() == QEvent::ToolTip) {
		if (QToolButton* button = qobject_cast<QToolButton*>(watched);
			button && m_toolBarItems.contains(qobject_cast<QToolBar*>(button->parentWidget()))) {
			if (!m_options.toolBarScreenTips) {
				QToolTip::hideText();
				return true;
			}
			QString tip = button->toolTip();
			unsigned id = button->defaultAction() ? button->defaultAction()->data().toUInt() : 0;
			QString shortcut = m_options.toolBarAccelTips && id ? acceleratorText(id) : QString();
			if (!tip.isEmpty() && !shortcut.isEmpty()) {
				tip += QStringLiteral(" (%1)").arg(shortcut);
			}
			QToolTip::showText(static_cast<QHelpEvent*>(event)->globalPos(), tip, button);
			return true;
		}
	}
	if (QToolButton* extension = qobject_cast<QToolButton*>(watched);
		extension && extension->inherits("QToolBarExtension") &&
		(event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick) &&
		static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
		if (QToolBar* toolBar = qobject_cast<QToolBar*>(extension->parentWidget());
			toolBar && m_toolBarItems.contains(toolBar)) {
			showToolBarOptions(toolBar);
			return true;
		}
	}
	if (QToolBar* toolBar = qobject_cast<QToolBar*>(watched); toolBar && m_toolBarItems.contains(toolBar)) {
		if (event->type() == QEvent::LayoutRequest) {
			toolBar->update();
		}
		if (event->type() == QEvent::ChildPolished) {
			if (QToolButton* button = qobject_cast<QToolButton*>(static_cast<QChildEvent*>(event)->child())) {
				button->installEventFilter(this);
			}
		}
		if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick) {
			QMouseEvent* mouse = static_cast<QMouseEvent*>(event);
			QRect expand = WhidbeyStyle::toolBarExpandRect(toolBar);
			if (mouse->button() == Qt::LeftButton && expand.contains(mouse->position().toPoint())) {
				showToolBarOptions(toolBar);
				return true;
			}
		}
	}
	if (QMenu* menu = qobject_cast<QMenu*>(watched); menu && !menu->property("doubleGripper").toBool()) {
		if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Down) {
			QList<QAction*> actions = menu->actions();
			bool collapsed = std::any_of(actions.begin(), actions.end(), [](QAction* action) {
				return action->property("expandButton").toBool() && action->isVisible();
			});
			QAction* last = nullptr;
			for (QAction* action : actions) {
				if (action->isVisible() && !action->isSeparator() && !action->property("expandButton").toBool()) {
					last = action;
				}
			}
			if (collapsed &&
				((static_cast<QKeyEvent*>(event)->modifiers() & Qt::ControlModifier) || menu->activeAction() == last)) {
				expandMenu(menu);
			}
		}
		else if (event->type() == QEvent::Hide && qobject_cast<QMenuBar*>(menu->parentWidget())) {
			QTimer::singleShot(0, this, [this]() {
				if (!QApplication::activePopupWidget()) {
					m_menusExpanded = false;
				}
			});
		}
	}
	if (QMenu* menu = qobject_cast<QMenu*>(watched); menu && menu->property("doubleGripper").toBool()) {
		if (event->type() == QEvent::MouseButtonRelease) {
			QMouseEvent* mouse = static_cast<QMouseEvent*>(event);
			if (toggleCustomizeItem(menu, menu->actionAt(mouse->position().toPoint()))) {
				return true;
			}
		}
		else if (event->type() == QEvent::KeyPress) {
			int key = static_cast<QKeyEvent*>(event)->key();
			if ((key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Space) &&
				toggleCustomizeItem(menu, menu->activeAction())) {
				return true;
			}
		}
	}
	if (watched == m_commandLineLabel && event->type() == QEvent::MouseButtonPress && m_commandLine->isEnabled()) {
		m_commandLine->setFocus();
		m_commandLine->showPopup();
		return true;
	}

	return QMainWindow::eventFilter(watched, event);
}

void CMainFrame::closeEvent(QCloseEvent* event)
{
	if (!m_gameLayout) {
		saveLayout();
	}

	if (!theApp.saveAllModified()) {
		event->ignore();
		return;
	}

	theApp.closeAllDocuments();
	onDestroy();
	event->accept();
}

void CMainFrame::onDestroy()
{
	if (m_destroyed) {
		return;
	}
	m_destroyed = true;

	Profile::WriteProfileInt("Settings", "fullscreen", m_fullScreen);

	if (m_fullScreen) {
		restoreResolution();
	}
	else {
		savePlacement();
	}

	QCoreApplication::quit();
}

CommandRoute CMainFrame::commandRoute()
{
	CommandRoute route;
	if (m_activeView) {
		route.push_back(m_activeView);
		route.push_back(m_activeView->document());
	}
	route.push_back(this);
	route.push_back(&theApp);
	return route;
}

bool CMainFrame::sendCommand(unsigned id)
{
	return routeCommand(commandRoute(), id);
}

void CMainFrame::postCommand(unsigned id)
{
	QMetaObject::invokeMethod(this, [this, id]() { sendCommand(id); }, Qt::QueuedConnection);
}

void CMainFrame::updateAction(QAction* action, unsigned id, bool setText)
{
	CommandState state;
	routeCommandUpdate(commandRoute(), id, state);
	action->setEnabled(state.enabled);
	action->setCheckable(state.checked.has_value());
	action->setChecked(state.checked.value_or(false));
	if (setText && state.text) {
		action->setText(*state.text);
	}
}

void CMainFrame::updateRecentFileMenu(QMenu* menu)
{
	QList<QAction*> actions = menu->actions();
	qsizetype position = -1;
	for (qsizetype i = 0; i < actions.size(); i++) {
		bool ok;
		unsigned id = actions[i]->data().toUInt(&ok);
		if (ok && isRecentFileCommand(id)) {
			if (position < 0) {
				position = i;
			}
			menu->removeAction(actions[i]);
			delete actions[i];
		}
	}
	if (position < 0) {
		return;
	}

	actions = menu->actions();
	QAction* before = position < actions.size() ? actions[position] : nullptr;
	std::vector<QString> names = theApp.recentFileDisplayNames();
	if (names.empty()) {
		QAction* action = new QAction("Recent File", menu);
		action->setData(static_cast<unsigned>(ID_FILE_MRU_FILE1));
		action->setEnabled(false);
		menu->insertAction(before, action);
		return;
	}

	for (unsigned i = 0; i < names.size(); i++) {
		QString name = names[i];
		name = escapeMnemonics(name);
		QAction* action = new QAction(QStringLiteral("&%1 %2").arg(i + 1).arg(name), menu);
		unsigned id = ID_FILE_MRU_FILE1 + i;
		action->setData(id);
		action->setStatusTip(Resources::string(ID_FILE_MRU_FILE1).section('\n', 0, 0));
		connect(action, &QAction::triggered, this, [this, id]() { sendCommand(id); });
		menu->insertAction(before, action);
	}
}

void CMainFrame::updateWindowListMenu(QMenu* menu)
{
	QAction* placeholder = nullptr;
	for (QAction* action : menu->actions()) {
		if (action->property("windowList").toBool()) {
			menu->removeAction(action);
			delete action;
		}
		else if (action->data().toUInt() == ID_WINDOW_LIST) {
			placeholder = action;
		}
	}
	if (!placeholder) {
		return;
	}

	placeholder->setVisible(WhidbeyStyle::customizeMode());
	if (WhidbeyStyle::customizeMode()) {
		return;
	}
	QList<QAction*> actions = menu->actions();
	qsizetype position = actions.indexOf(placeholder) + 1;
	QAction* before = position < actions.size() ? actions[position] : nullptr;

	int index = 1;
	for (int i = 0; i < m_workspace->count() && index <= windowListItemCount; i++) {
		QWidget* frame = m_workspace->widget(i);
		QString title = frame->windowTitle();
		title = escapeMnemonics(title);
		QAction* action = new QAction(QStringLiteral("&%1 %2").arg(index).arg(title), menu);
		action->setProperty("windowList", true);
		action->setCheckable(true);
		action->setChecked(frame == m_workspace->currentWidget());
		action->setStatusTip(Resources::string(windowListPromptStringId));
		connect(action, &QAction::triggered, this, [this, frame]() {
			if (m_workspace->indexOf(frame) >= 0) {
				m_workspace->setCurrentWidget(frame);
			}
		});
		menu->insertAction(before, action);
		index++;
	}
}

void CMainFrame::updateToolBarListMenu(QMenu* menu)
{
	QAction* placeholder = nullptr;
	for (QAction* action : menu->actions()) {
		if (action->property("toolBarList").toBool()) {
			menu->removeAction(action);
			delete action;
		}
		else if (action->data().toUInt() == ID_VIEW_TOOLBAR) {
			placeholder = action;
		}
	}
	if (!placeholder) {
		return;
	}

	placeholder->setVisible(WhidbeyStyle::customizeMode());
	if (WhidbeyStyle::customizeMode()) {
		return;
	}
	QList<QAction*> actions = menu->actions();
	qsizetype position = actions.indexOf(placeholder) + 1;
	QAction* before = position < actions.size() ? actions[position] : nullptr;
	for (QAction* action : toolBarListActions(menu)) {
		menu->insertAction(before, action);
	}
}

std::vector<QAction*> CMainFrame::toolBarListActions(QObject* parent)
{
	std::vector<QAction*> actions;
	for (QToolBar* toolBar : m_toolBars) {
		QAction* action = new QAction(escapeMnemonics(toolBar->windowTitle()), parent);
		action->setProperty("toolBarList", true);
		action->setCheckable(true);
		action->setChecked(toolBar->isVisible());
		connect(action, &QAction::triggered, toolBar, [toolBar]() { toolBar->setVisible(!toolBar->isVisible()); });
		actions.push_back(action);
	}
	return actions;
}

void CMainFrame::showToolBarsContextMenu(const QPoint& globalPos)
{
	if (WhidbeyStyle::customizeMode()) {
		return;
	}

	QMenu menu(this);
	for (QAction* action : toolBarListActions(&menu)) {
		menu.addAction(action);
	}
	menu.addSeparator();
	QAction* customize = menu.addAction(Resources::string(xtpCustomizeStringId).section('\n', 1, 1));
	connect(customize, &QAction::triggered, this, [this]() { sendCommand(ID_TOOLS_CUSTOMIZE); });
	updateAction(customize, ID_TOOLS_CUSTOMIZE, false);
	menu.exec(globalPos);
}

void CMainFrame::contextMenuEvent(QContextMenuEvent* event)
{
	if (!childAt(event->pos())) {
		showToolBarsContextMenu(event->globalPos());
	}
}

void CMainFrame::updateMenu(QMenu* menu)
{
	updateRecentFileMenu(menu);
	updateWindowListMenu(menu);
	updateToolBarListMenu(menu);

	for (QAction* action : menu->actions()) {
		bool ok;
		unsigned id = action->data().toUInt(&ok);
		if (!ok || id == 0 || id == ID_WINDOW_LIST || action->isSeparator() || action->menu() ||
			isRecentFileCommand(id)) {
			continue;
		}

		updateAction(action, id, true);
		if (WhidbeyStyle::customizeMode()) {
			action->setEnabled(true);
		}
	}
	applyPersonalizedMenu(menu);
}

bool CMainFrame::isCommandHidden(unsigned id) const
{
	return std::find(std::begin(hiddenCommands), std::end(hiddenCommands), id) != std::end(hiddenCommands) &&
		   !m_options.usedCommands.contains(id);
}

bool CMainFrame::isActionHidden(QAction* action) const
{
	if (QMenu* popup = action->menu()) {
		bool any = false;
		for (QAction* child : popup->actions()) {
			if (child->isSeparator() || child->property("expandButton").toBool()) {
				continue;
			}
			any = true;
			if (!isActionHidden(child)) {
				return false;
			}
		}
		return any;
	}
	bool ok;
	unsigned id = action->data().toUInt(&ok);
	return ok && id != 0 && isCommandHidden(id);
}

void CMainFrame::markCommandUsed(unsigned id)
{
	if (isCommandHidden(id)) {
		m_options.usedCommands.insert(id);
	}
}

void CMainFrame::applyPersonalizedMenu(QMenu* menu)
{
	QList<QAction*> actions = menu->actions();
	QAction* expand = nullptr;
	for (QAction* action : actions) {
		if (action->property("expandButton").toBool()) {
			expand = action;
		}
	}

	bool collapsedExists = false;
	std::vector<std::pair<QAction*, bool>> items;
	for (QAction* action : actions) {
		if (action->isSeparator() || action == expand || qobject_cast<QWidgetAction*>(action)) {
			continue;
		}
		bool hidden = !m_options.alwaysShowFullMenus && !WhidbeyStyle::customizeMode() && isActionHidden(action);
		items.emplace_back(action, hidden);
		if (!hidden) {
			collapsedExists = true;
		}
	}

	bool collapsed = !m_options.alwaysShowFullMenus && !m_menusExpanded && collapsedExists;
	bool anyCollapsed = false;
	for (auto& [action, hidden] : items) {
		action->setProperty("expandedItem", hidden);
		action->setVisible(!(hidden && collapsed));
		anyCollapsed = anyCollapsed || (hidden && collapsed);
	}

	for (qsizetype i = 0; i < actions.size(); i++) {
		if (!actions[i]->isSeparator()) {
			continue;
		}
		bool visible = false;
		for (qsizetype j = i + 1; j < actions.size(); j++) {
			if (!actions[j]->isSeparator()) {
				visible = actions[j]->isVisible() && actions[j] != expand;
				break;
			}
		}
		actions[i]->setVisible(visible);
	}

	if (anyCollapsed && !expand) {
		QWidgetAction* action = MenuExpandButton::addTo(menu);
		action->setProperty("expandButton", true);
		MenuExpandButton* button = static_cast<MenuExpandButton*>(action->defaultWidget());
		button->onExpand = [this, menu]() { expandMenu(menu); };
		button->expandOnHover = [this]() { return m_options.showFullAfterDelay; };
		expand = action;
	}
	if (expand) {
		expand->setVisible(anyCollapsed);
		static_cast<QWidgetAction*>(expand)->defaultWidget()->setVisible(anyCollapsed);
	}

	if (anyCollapsed && m_options.showFullAfterDelay && qobject_cast<QMenuBar*>(menu->parentWidget())) {
		QTimer::singleShot(expandDelay, menu, [this, menu]() {
			QRect item = menuBar()->actionGeometry(menu->menuAction());
			if (menu->isVisible() && item.contains(menuBar()->mapFromGlobal(QCursor::pos()))) {
				expandMenu(menu);
			}
		});
	}
}

void CMainFrame::expandMenu(QMenu* menu)
{
	QRect previous = menu->geometry();
	m_menusExpanded = true;
	applyPersonalizedMenu(menu);
	menu->resize(menu->sizeHint());
	WhidbeyStyle::animateMenuExpansion(menu, previous);
}

void CMainFrame::updateCommandUI()
{
	if (!m_gameLayout) {
		bool customize = WhidbeyStyle::customizeMode();
		for (auto& [id, action] : m_toolBarActions) {
			updateAction(action, id, false);
			if (customize) {
				action->setEnabled(true);
			}
		}

		for (QLabel* label : m_toolBarLabels) {
			CommandState state;
			routeCommandUpdate(commandRoute(), label->property("commandId").toUInt(), state);
			label->setEnabled(state.enabled || customize);
			if (state.text) {
				label->setText(*state.text);
			}
		}

		CommandState commandLine;
		routeCommandUpdate(commandRoute(), ID_COMMAND_LINE, commandLine);
		m_commandLine->setEnabled(commandLine.enabled || customize);
		m_commandLineLabel->setEnabled(commandLine.enabled || customize);

		CommandState fill;
		routeCommandUpdate(commandRoute(), ID_TOOL_FILL, fill);
		m_fillColorButton->setEnabled(fill.enabled || customize);

		for (int i = 1; i < 5; i++) {
			CommandState state;
			routeCommandUpdate(commandRoute(), indicators[i], state);
			if (state.text) {
				m_indicators[i]->setText(*state.text);
			}
		}
	}

	for (int i = 0; i < m_workspace->count(); i++) {
		static_cast<CChildFrame*>(m_workspace->widget(i))->activeView()->updateCommandUI();
	}
}

CChildFrame* CMainFrame::activeChildFrame() const
{
	return static_cast<CChildFrame*>(m_workspace->currentWidget());
}

void CMainFrame::addChildFrame(CChildFrame* frame)
{
	QSignalBlocker blocker(m_workspace);
	m_workspace->addTab(frame, QString());
}

void CMainFrame::activateChildFrame(CChildFrame* frame)
{
	m_workspace->setCurrentWidget(frame);
	childFrameActivated(frame);
	frame->activeView()->setFocus();
}

void CMainFrame::childFrameTitleChanged(CChildFrame* frame)
{
	int index = m_workspace->indexOf(frame);
	if (index >= 0) {
		QString title = frame->windowTitle();
		m_workspace->setTabText(index, escapeMnemonics(title));
		m_workspace->setTabToolTip(index, title);
	}
	updateTitle();
}

void CMainFrame::childFrameActivated(CChildFrame* frame)
{
	View* activate = frame ? frame->activeView() : nullptr;
	View* deactivate = m_activeView;
	if (activate != deactivate) {
		m_activeView = activate;
		if (deactivate) {
			deactivate->activateFrame(false);
			deactivate->activate(false, deactivate);
		}
		if (activate) {
			activate->activateFrame(true);
			activate->activate(true, activate);
		}
		updateCommandUI();
	}
	updateTitle();
}

void CMainFrame::childFrameDestroyed(CChildFrame* frame)
{
	if (m_activeView && frame->activeView() == m_activeView) {
		View* view = m_activeView;
		m_activeView = nullptr;
		view->activate(false, nullptr);
	}
}

void CMainFrame::childFrameViewChanged(CChildFrame* frame)
{
	if (frame == activeChildFrame()) {
		m_activeView = frame->activeView();
		updateCommandUI();
		updateTitle();
	}
}

void CMainFrame::updateTitle()
{
	QString title = Resources::string(appTitleStringId);
	if (m_activeView && m_activeView->document()) {
		title += QStringLiteral(" - [%1]").arg(m_activeView->document()->title());
	}
	setWindowTitle(title);
}
