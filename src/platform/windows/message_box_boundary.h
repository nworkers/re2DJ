#pragma once

namespace re2dj::platform::windows
{

// Receives one already formatted line per captured dialog.
using MessageBoxBoundarySink = void (*)(const char* message);

// Redirects USER32's MessageBoxA and MessageBoxW entry points to a recorder
// that writes the caption and text to the sink and answers with `result`
// instead of showing a modal dialog.
//
// This is a diagnostic boundary. A guest that reports a startup failure through
// a dialog otherwise stops the run at that dialog, and a detached product run
// has nobody to dismiss it, so the failure the dialog names never reaches the
// log. Redirecting the entry point rather than an import slot is what reaches a
// packed guest, whose imports are resolved by walking the USER32 export table
// rather than through GetProcAddress.
//
// Returns true when at least one entry point was redirected. Calling it again
// after a successful install is a no-op that returns true.
bool InstallMessageBoxBoundary(MessageBoxBoundarySink sink, int result);

}  // namespace re2dj::platform::windows
