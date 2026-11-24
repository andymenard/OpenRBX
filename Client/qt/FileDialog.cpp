#include "FileDialog.h"

#include "Dialog.h"
#include "MainFrm.h"
#include "Roblox.h"

#include <QFileInfo>
#include <QStringList>
#include <QWidget>

#ifdef __ANDROID__
#include "SystemMessageBox.h"

#include <QCryptographicHash>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <bzlib.h>
#endif

#ifdef _WIN32
#include "NativeWindow.h"

#define NOMINMAX
#include <commdlg.h>
#include <vector>
#include <windows.h>
#else
#include <QDir>
#include <QFileDialog>
#endif

CFileDialog::CFileDialog(
	bool openFileDialog,
	const QString& defExt,
	const QString& fileName,
	unsigned flags,
	const QString& filter,
	QWidget* parent
)
	: m_openFileDialog(openFileDialog), m_defExt(defExt), m_fileName(fileName), m_flags(flags), m_filter(filter),
	  m_parent(parent)
{
}

QString CFileDialog::GetFileExt() const
{
	return QFileInfo(m_pathName).suffix();
}

#ifdef _WIN32

static UINT_PTR CALLBACK fileDialogHook(HWND window, UINT message, WPARAM, LPARAM lParam)
{
	if (message == WM_INITDIALOG) {
		return TRUE;
	}
	if (message == WM_NOTIFY && reinterpret_cast<OFNOTIFYW*>(lParam)->hdr.code == CDN_INITDONE) {
		centerWindowOverOwner(GetParent(window));
	}
	return 0;
}

static HWND safeOwner(QWidget* parent)
{
	HWND window = parent ? reinterpret_cast<HWND>(parent->window()->winId())
						 : reinterpret_cast<HWND>(theApp.mainFrame()->winId());
	while (window && (GetWindowLongW(window, GWL_STYLE) & WS_CHILD)) {
		window = GetParent(window);
	}
	if (!parent && window) {
		window = GetLastActivePopup(window);
	}
	return window;
}

int CFileDialog::DoModal()
{
	std::wstring filter = m_filter.toStdWString();
	for (wchar_t& character : filter) {
		if (character == L'|') {
			character = L'\0';
		}
	}
	std::wstring defExt = m_defExt.toStdWString();
	std::wstring title = m_title.toStdWString();
	std::vector<wchar_t> file(MAX_PATH, L'\0');
	std::vector<wchar_t> fileTitle(64, L'\0');
	std::wstring initial = m_fileName.toStdWString();
	wcsncpy_s(file.data(), file.size(), initial.c_str(), _TRUNCATE);

	OPENFILENAMEW ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = safeOwner(m_parent);
	ofn.lpstrFile = file.data();
	ofn.nMaxFile = static_cast<DWORD>(file.size());
	ofn.lpstrDefExt = m_defExt.isEmpty() ? nullptr : defExt.c_str();
	ofn.lpstrFileTitle = fileTitle.data();
	ofn.nMaxFileTitle = static_cast<DWORD>(fileTitle.size());
	ofn.Flags = m_flags | OFN_ENABLEHOOK | OFN_EXPLORER;
	ofn.lpfnHook = fileDialogHook;
	ofn.lpstrFilter = m_filter.isEmpty() ? nullptr : filter.c_str();
	ofn.nFilterIndex = static_cast<DWORD>(m_filterIndex);
	ofn.lpstrTitle = m_title.isEmpty() ? nullptr : title.c_str();

	BOOL result = m_openFileDialog ? GetOpenFileNameW(&ofn) : GetSaveFileNameW(&ofn);
	if (!result) {
		return IDCANCEL;
	}
	m_pathName = QString::fromWCharArray(file.data());
	return IDOK;
}

#else

#ifdef __ANDROID__
// Copies an Android document into a local file, expanding bzip2 (Novetus .rbxl.bz2).
// Returns an empty string on success, otherwise a message for the user.
static QString importAndroidDocument(QFile& source, const QString& folder, QString& name, QString& path)
{
	QByteArray chunk = source.read(64 * 1024);
	if (source.error() != QFileDevice::NoError) {
		return source.errorString();
	}
	const bool compressed = chunk.startsWith("BZh");
	if (compressed && name.endsWith(QStringLiteral(".bz2"), Qt::CaseInsensitive)) {
		name.chop(4);
	}
	if (!QDir().mkpath(folder)) {
		return QStringLiteral("Cannot create a local copy of the selected file.");
	}
	QSaveFile destination(QDir(folder).filePath(name));
	if (!destination.open(QIODevice::WriteOnly)) {
		return QStringLiteral("Cannot create a local copy of the selected file.");
	}

	bz_stream stream = {};
	bool streamOpen = false;
	QByteArray head;
	QByteArray output(256 * 1024, Qt::Uninitialized);
	while (!chunk.isEmpty()) {
		if (!compressed) {
			if (head.size() < 64) {
				head += chunk.left(64 - head.size());
			}
			if (destination.write(chunk) != chunk.size()) {
				return destination.errorString();
			}
		}
		else {
			stream.next_in = chunk.data();
			stream.avail_in = static_cast<unsigned>(chunk.size());
			while (stream.avail_in > 0) {
				if (!streamOpen) {
					// Concatenated bzip2 streams (as written by pbzip2) are one file.
					if (BZ2_bzDecompressInit(&stream, 0, 0) != BZ_OK) {
						return QStringLiteral("Cannot start bzip2 decompression.");
					}
					streamOpen = true;
				}
				stream.next_out = output.data();
				stream.avail_out = static_cast<unsigned>(output.size());
				int result = BZ2_bzDecompress(&stream);
				qsizetype produced = output.size() - stream.avail_out;
				if (head.size() < 64) {
					head += output.left(std::min<qsizetype>(produced, 64 - head.size()));
				}
				if (destination.write(output.constData(), produced) != produced) {
					BZ2_bzDecompressEnd(&stream);
					return destination.errorString();
				}
				if (result == BZ_STREAM_END) {
					BZ2_bzDecompressEnd(&stream);
					streamOpen = false;
				}
				else if (result != BZ_OK) {
					BZ2_bzDecompressEnd(&stream);
					return QStringLiteral("The compressed file is damaged (bzip2 error %1).").arg(result);
				}
			}
		}
		chunk = source.read(64 * 1024);
		if (source.error() != QFileDevice::NoError) {
			if (streamOpen) {
				BZ2_bzDecompressEnd(&stream);
			}
			return source.errorString();
		}
	}
	if (streamOpen) {
		BZ2_bzDecompressEnd(&stream);
		return QStringLiteral("The compressed file ends unexpectedly.");
	}

	// Places and models are XML or "<roblox!" binary; reject descriptions and other text.
	QByteArray start = head.startsWith("\xEF\xBB\xBF") ? head.mid(3) : head;
	if (!start.trimmed().startsWith('<')) {
		destination.cancelWriting();
		return name.endsWith(QStringLiteral("_desc.txt"), Qt::CaseInsensitive)
				   ? QStringLiteral("This is a map description. Choose the .rbxl or .rbxl.bz2 file next to it.")
				   : QStringLiteral("This is not a place or model file.");
	}
	if (!destination.commit()) {
		return destination.errorString();
	}
	path = destination.fileName();
	return QString();
}
#endif

int CFileDialog::DoModal()
{
	QStringList parts = m_filter.split(QLatin1Char('|'));
	QStringList filters;
	for (qsizetype i = 0; i + 1 < parts.size() && !parts[i].isEmpty(); i += 2) {
		QString patterns = QString(parts[i + 1]).replace(QLatin1Char(';'), QLatin1Char(' '));
		filters.append(
			parts[i].contains(QLatin1Char('(')) ? parts[i] : QStringLiteral("%1 (%2)").arg(parts[i], patterns)
		);
	}
	QString selectedFilter =
		m_filterIndex > 0 && m_filterIndex <= filters.size() ? filters[m_filterIndex - 1] : QString();
	QWidget* parent = m_parent ? m_parent->window() : theApp.mainFrame();
	QString title =
		m_title.isEmpty() ? (m_openFileDialog ? QStringLiteral("Open") : QStringLiteral("Save As")) : m_title;
	QFileDialog::Options options;
	if (!(m_flags & OverwritePrompt)) {
		options |= QFileDialog::DontConfirmOverwrite;
	}
	QString path;
#ifdef __ANDROID__
	if (m_openFileDialog) {
		// Android providers return content URLs. .rbxl and .bz2 have no registered MIME
		// type there (both are application/octet-stream); filtering hides the _desc.txt
		// files Novetus keeps next to each map, whose truncated names look the same.
		QFileDialog dialog(parent, title);
		dialog.setOptions(options);
		dialog.setFileMode(QFileDialog::ExistingFile);
		dialog.setMimeTypeFilters(
			{QStringLiteral("application/octet-stream"),
			 QStringLiteral("application/x-bzip2"),
			 QStringLiteral("application/x-bzip"),
			 QStringLiteral("application/xml"),
			 QStringLiteral("text/xml")}
		);
		QUrl selected;
		if (dialog.exec() == QDialog::Accepted && !dialog.selectedUrls().isEmpty()) {
			selected = dialog.selectedUrls().constFirst();
		}
		path = selected.isLocalFile() ? selected.toLocalFile() : selected.toString();
	}
	else
#endif
	{
		path = m_openFileDialog
				   ? QFileDialog::getOpenFileName(
						 parent, title, m_fileName, filters.join(QStringLiteral(";;")), &selectedFilter, options
					 )
				   : QFileDialog::getSaveFileName(
						 parent, title, m_fileName, filters.join(QStringLiteral(";;")), &selectedFilter, options
					 );
	}
	if (path.isEmpty()) {
		return IDCANCEL;
	}
#ifdef __ANDROID__
	if (m_openFileDialog && QUrl(path).scheme() == QStringLiteral("content")) {
		// The legacy readers use std::ifstream, which cannot open Android content URLs.
		// Keep a persistent local copy so reopening from Recent Files also works.
		QFile source(path);
		QString name = QFileInfo(QFileInfo(path).fileName()).fileName();
		if (name.isEmpty() || name == QStringLiteral(".") || name == QStringLiteral("..")) {
			name = QStringLiteral("document");
		}
		QString id = QString::fromLatin1(QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha256).toHex());
		QString folder =
			QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/imports/") + id;
		QString error =
			source.open(QIODevice::ReadOnly) ? importAndroidDocument(source, folder, name, path) : source.errorString();
		if (!error.isEmpty()) {
			SystemMessageBox::warning(
				parent, title, QStringLiteral("Unable to import the selected file.\n%1").arg(error)
			);
			return IDCANCEL;
		}
	}
#endif
	if (!m_openFileDialog && !m_defExt.isEmpty() && QFileInfo(path).suffix().isEmpty()) {
		path += QLatin1Char('.') + m_defExt;
	}
	m_pathName = QDir::toNativeSeparators(path);
	return IDOK;
}

#endif
