/**************************************************************************/
/*  godot_cef_ipc_contract.h                                              */
/**************************************************************************/

#pragma once

// Shared between the engine (browser process) and godot_cef_helper (render process).
// Must not include engine headers. Engine TUs include godot_cef_include.h first so CEF
// error codes are renamed before this file pulls in the scheme registrar.

#ifndef GODOT_CEF_SDK_INCLUDED
#include "include/cef_scheme.h"
#endif

namespace GodotCefIpc {

// Maximum typed IPC payload, in bytes.
constexpr size_t MAX_IPC_DATA_BYTES = 8 * 1024 * 1024;

constexpr const char *ROUTE_IPC_GODOT_TO_RENDERER = "ipcGodotToRenderer";
constexpr const char *ROUTE_IPC_RENDERER_TO_GODOT = "ipcRendererToGodot";
constexpr const char *ROUTE_IPC_BINARY_GODOT_TO_RENDERER = "ipcBinaryGodotToRenderer";
constexpr const char *ROUTE_IPC_BINARY_RENDERER_TO_GODOT = "ipcBinaryRendererToGodot";
constexpr const char *ROUTE_IPC_DATA_GODOT_TO_RENDERER = "ipcDataGodotToRenderer";
constexpr const char *ROUTE_IPC_DATA_RENDERER_TO_GODOT = "ipcDataRendererToGodot";
constexpr const char *ROUTE_TRIGGER_IME = "triggerIme";
constexpr const char *ROUTE_IME_CARET_POSITION = "imeCaretPosition";

constexpr const char *EXTRA_INFO_PRELOAD_SCRIPT = "godotCefPreloadScript";

// Typed values sent with `sendIpcData` travel as CefValue trees. Godot values that have
// no JS counterpart are sent as `{__godot_type: <Variant::Type>, __godot_value: <str()>}`.
constexpr const char *TYPED_VALUE_TYPE_KEY = "__godot_type";
constexpr const char *TYPED_VALUE_VALUE_KEY = "__godot_value";
// CefBinaryValue cannot be empty, so an empty ArrayBuffer / PackedByteArray travels as this tag.
constexpr const char *TYPED_VALUE_EMPTY_BYTES = "bytes";

constexpr const char *SCHEME_RES = "res";
constexpr const char *SCHEME_USER = "user";

// Must be called with identical arguments in every process.
inline void register_custom_schemes(CefRawPtr<CefSchemeRegistrar> p_registrar) {
	const int options = CEF_SCHEME_OPTION_STANDARD | CEF_SCHEME_OPTION_LOCAL | CEF_SCHEME_OPTION_SECURE |
			CEF_SCHEME_OPTION_CORS_ENABLED | CEF_SCHEME_OPTION_FETCH_ENABLED | CEF_SCHEME_OPTION_CSP_BYPASSING;
	p_registrar->AddCustomScheme(SCHEME_RES, options);
	p_registrar->AddCustomScheme(SCHEME_USER, options);
}

} // namespace GodotCefIpc
