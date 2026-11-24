#ifndef ANDROIDASSETS_H
#define ANDROIDASSETS_H

#include <string>

namespace AndroidAssets {

// Copies the runtime files and content packaged in the APK to the app's data directory, once per
// installed build. Needs a running QGuiApplication, which provides the assets:/ file engine.
void install();

// True when game content beyond the files packaged in the APK is installed.
bool hasContent();

// Asks for a game content folder (for example a 2007 client's "content" folder) and copies it to
// the data directory. Returns false when the user chooses to quit instead.
bool requestContent();

// Where the runtime files that sit beside the executable on desktop platforms are found.
std::string dataDirectory();

} // namespace AndroidAssets

#endif // ANDROIDASSETS_H
