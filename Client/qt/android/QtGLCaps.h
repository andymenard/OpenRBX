#ifndef QTGLCAPS_H
#define QTGLCAPS_H

// Qt's OpenGL ES headers and G3D's desktop OpenGL headers cannot be included together, so the
// G3D side of window setup lives in its own file.
void initGLCaps();

#endif // QTGLCAPS_H
