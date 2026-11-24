#include "AndroidAssets.h"

#include "util/FileSystem.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <cstdlib>

static const char assetRoot[] = "assets:/openrbx";

static QByteArray readStamp(const QString& path)
{
	QFile file(path);
	return file.open(QIODevice::ReadOnly) ? file.readAll().trimmed() : QByteArray();
}

namespace AndroidAssets {

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
