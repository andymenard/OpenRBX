#include "AndroidAssets.h"

#include "util/FileSystem.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QStandardPaths>
#include <QUrl>
#include <cstdlib>
#include <utility>
#include <vector>

static const char assetRoot[] = "assets:/openrbx";

static QByteArray readStamp(const QString& path)
{
	QFile file(path);
	return file.open(QIODevice::ReadOnly) ? file.readAll().trimmed() : QByteArray();
}

// Lists files below a folder with paths relative to it. Paths are built from entry names, so this
// also works for Android document tree URLs, whose child paths are not textual subpaths.
static void listFiles(const QString& folder, const QString& prefix, std::vector<std::pair<QString, QString>>& files)
{
	const QFileInfoList entries =
		QDir(folder).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
	for (const QFileInfo& entry : entries) {
		QString relative = prefix + entry.fileName();
		if (entry.isDir()) {
			listFiles(entry.filePath(), relative + '/', files);
		}
		else {
			files.emplace_back(entry.filePath(), relative);
		}
	}
}

static QString findChild(const QString& folder, const QString& name)
{
	const QFileInfoList entries = QDir(folder).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
	for (const QFileInfo& entry : entries) {
		if (entry.fileName().compare(name, Qt::CaseInsensitive) == 0) {
			return entry.filePath();
		}
	}
	return QString();
}

// The chosen folder, or its "content" subfolder, must contain the client's textures.
static QString locateContent(const QString& folder)
{
	if (!findChild(folder, "textures").isEmpty()) {
		return folder;
	}
	QString content = findChild(folder, "content");
	return !content.isEmpty() && !findChild(content, "textures").isEmpty() ? content : QString();
}

static bool copyContent(const QString& source, const QString& target, QString& error)
{
	std::vector<std::pair<QString, QString>> files;
	listFiles(source, QString(), files);
	QProgressDialog progress("Copying game content...", QString(), 0, static_cast<int>(files.size()));
	progress.setWindowModality(Qt::ApplicationModal);
	progress.setMinimumDuration(0);
	int done = 0;
	for (const auto& [path, relative] : files) {
		progress.setValue(done++);
		// Files shipped in the APK (original replacements for missing models) take precedence.
		QString bundled = QString(assetRoot) + "/content/" + relative;
		QFile in(QFile::exists(bundled) ? bundled : path);
		QString destination = QDir(target).filePath(relative);
		QDir().mkpath(QFileInfo(destination).absolutePath());
		QFile out(destination);
		if (!in.open(QIODevice::ReadOnly) || !out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			error = relative + ": " + (in.isOpen() ? out.errorString() : in.errorString());
			return false;
		}
		while (!in.atEnd()) {
			QByteArray chunk = in.read(256 * 1024);
			if (in.error() != QFileDevice::NoError || out.write(chunk) != chunk.size()) {
				error = relative + ": " + (in.error() != QFileDevice::NoError ? in.errorString() : out.errorString());
				return false;
			}
		}
	}
	progress.setValue(done);
	// Also restore bundled files the chosen folder does not have.
	QDirIterator bundled(QString(assetRoot) + "/content", QDir::Files, QDirIterator::Subdirectories);
	while (bundled.hasNext()) {
		QString source = bundled.next();
		QString destination = QDir(target).filePath(QDir(QString(assetRoot) + "/content").relativeFilePath(source));
		QDir().mkpath(QFileInfo(destination).absolutePath());
		QFile::remove(destination);
		QFile::copy(source, destination);
		QFile::setPermissions(destination, QFile::ReadOwner | QFile::WriteOwner);
	}
	return true;
}

namespace AndroidAssets {

bool hasContent()
{
	QDir textures(QString::fromStdString(dataDirectory()) + "/content/textures");
	return textures.exists() && !textures.isEmpty();
}

bool requestContent()
{
	QString message = "OpenRBX needs the content folder from a 2007-era client (fonts, sky, sounds and "
					  "textures). It is not included with the app.\n\n"
					  "Copy one to this device, for example data/clients/2007E/content from Novetus, then "
					  "choose that folder (or the folder that contains it).";
	while (true) {
		QMessageBox box(QMessageBox::Information, "Game content needed", message);
		QPushButton* choose = box.addButton("Choose folder", QMessageBox::AcceptRole);
		box.addButton("Quit", QMessageBox::RejectRole);
		box.exec();
		if (box.clickedButton() != choose) {
			return false;
		}

		QUrl selected = QFileDialog::getExistingDirectoryUrl(nullptr, "Choose the content folder");
		if (selected.isEmpty()) {
			continue;
		}
		QString folder = selected.isLocalFile() ? selected.toLocalFile() : selected.toString();
		QString content = locateContent(folder);
		if (content.isEmpty()) {
			message = "That folder has no \"textures\" folder, so it is not a client content folder.\n\n"
					  "Choose the \"content\" folder itself, or the client folder that contains it.";
			continue;
		}
		QString error;
		if (copyContent(content, QString::fromStdString(dataDirectory()) + "/content", error)) {
			return true;
		}
		message = "Copying the content failed:\n" + error + "\n\nChoose the folder again, or quit.";
	}
}

std::string dataDirectory()
{
	return RBX::FileSystem::getUserAppDataDirectory(true, nullptr);
}

void install()
{
	// Qt's Android loader points HOME at the app's files directory; keep a fallback for launchers
	// that do not, since the data paths are derived from it.
	const char* home = std::getenv("HOME");
	if (!home || !*home) {
		QString files = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
		QDir().mkpath(files);
		setenv("HOME", QFile::encodeName(files).constData(), 1);
	}

	QString target = QString::fromStdString(dataDirectory());
	if (target.isEmpty()) {
		return;
	}

	QByteArray packaged = readStamp(QString(assetRoot) + "/stamp.txt");
	QString installedStampPath = QDir(target).filePath("stamp.txt");
	if (!packaged.isEmpty() && packaged == readStamp(installedStampPath)) {
		return;
	}

	QDir root(assetRoot);
	QDirIterator it(assetRoot, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		QString source = it.next();
		QString relative = root.relativeFilePath(source);
		if (relative == "stamp.txt") {
			continue;
		}
		QString destination = QDir(target).filePath(relative);
		QDir().mkpath(QFileInfo(destination).absolutePath());
		QFile::remove(destination);
		QFile::copy(source, destination);
		QFile::setPermissions(destination, QFile::ReadOwner | QFile::WriteOwner);
	}

	QFile stamp(installedStampPath);
	if (stamp.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		stamp.write(packaged);
	}
}

} // namespace AndroidAssets
