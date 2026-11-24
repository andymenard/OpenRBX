#ifndef ANDROIDASSETS_H
#define ANDROIDASSETS_H

#include <string>

namespace AndroidAssets {

// Copies the runtime files and content packaged in the APK to the app's data directory, once per
// installed build. Needs a running QGuiApplication, which provides the assets:/ file engine.
void install();

// Where the runtime files that sit beside the executable on desktop platforms are found.
std::string dataDirectory();

} // namespace AndroidAssets

#endif // ANDROIDASSETS_H
