/**************************************************************************/
/*  godot_cef_ipc.h                                                       */
/**************************************************************************/

#pragma once

#include "godot_cef_include.h"

#include "core/variant/variant.h"

namespace GodotCefIpcData {

// Converts a Variant for transport to the render process.
// Types without a JavaScript equivalent travel as {"__godot_type": <Variant::Type>, "__godot_value": var_to_str()}.
CefRefPtr<CefValue> variant_to_cef_value(const Variant &p_value);
Variant cef_value_to_variant(const CefRefPtr<CefValue> &p_value);

String to_godot_string(const CefString &p_string);
CefString to_cef_string(const String &p_string);

} // namespace GodotCefIpcData
