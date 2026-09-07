#pragma once

namespace re2dj::platform::windows
{

// Appends one line to the graphics trace file the launcher named through the
// exported g_re2dj_graphics_trace_path buffer, and mirrors the same line to an
// attached debugger. The debugger mirror is what an attached diagnostic run
// collects as output_debug events; the file is the only evidence a detached
// product run leaves behind, so every graphics HLE boundary writes through
// here rather than calling OutputDebugStringA directly.
//
// Safe to call before the launcher fills the path: the debugger mirror still
// happens and the file write is skipped.
void WriteGraphicsTraceLine(const char* message);

// printf-style form of WriteGraphicsTraceLine. Lines longer than the internal
// buffer are truncated rather than split.
void WriteGraphicsTraceFormat(const char* format, ...);

// A per-method budget for a vtable slot that has no implementation yet. The
// question such a slot answers is which methods the guest reaches and in what
// order, so each one records its first few calls individually rather than a
// summary, and the budget keeps a per-frame method from filling the file.
//
// One instance belongs to one vtable slot, declared as a function-local static
// so the count is per method rather than per interface.
struct GraphicsCallLedger
{
    const char* method = nullptr;
    long remaining = 0;
};

// Records "<interface>::<method>:not-implemented" while the ledger has budget.
void ReportUnimplementedGraphicsCall(const char* interface_name,
                                     GraphicsCallLedger* ledger);

// Whether the diagnostics that run inside the per-draw path are enabled.
//
// Those diagnostics scan whole texture surfaces and format long records, so
// they cost real frame time even though each one is budgeted. The product path
// leaves them off; the launcher turns them on through its own option when a
// draw-level investigation needs them. This is deliberately separate from the
// graphics trace path, which the launcher always fills and which therefore
// cannot act as a switch.
bool AreGraphicsDrawDiagnosticsEnabled();

}  // namespace re2dj::platform::windows
