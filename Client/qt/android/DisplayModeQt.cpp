#include "DisplayMode.h"

// Android apps cannot change the display mode. Full screen keeps the device's native mode, so the
// change always succeeds and the caller only has to show the window full screen.

bool changeDisplayMode(QWidget* window, QSize size)
{
	return true;
}

void restoreDisplayMode(QWidget* window)
{
}
