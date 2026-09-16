#ifndef RE2DJ_GRAPHICS_PRESENT_OVERLAY_H_
#define RE2DJ_GRAPHICS_PRESENT_OVERLAY_H_

namespace re2dj::graphics
{

// Something drawn on top of a presented frame without becoming part of it.
//
// The guest renders into a fixed logical target that the backend scales onto
// the window. An overlay draws after that composite, at the window's own pixel
// size, so it stays sharp at any scale and never leaks into the guest image the
// next frame builds on.
//
// The backend knows only this interface, so what the overlay is - and which UI
// library draws it - stays out of the rendering layer.
class PresentOverlay
{
public:
    virtual ~PresentOverlay() = default;

    // Called once per present with the window's default framebuffer bound, the
    // viewport covering the whole window, and the backend's OpenGL context
    // current, immediately before the buffers are swapped. An implementation
    // must leave the OpenGL state it found, because the backend's next frame
    // assumes its own state is still in place.
    virtual void DrawOverlay(int pixel_width, int pixel_height) = 0;
};

}  // namespace re2dj::graphics

#endif  // RE2DJ_GRAPHICS_PRESENT_OVERLAY_H_
