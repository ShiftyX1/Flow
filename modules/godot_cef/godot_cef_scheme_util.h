/**************************************************************************/
/*  godot_cef_scheme_util.h                                               */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"

// Helpers for serving res:// and user:// to CEF. Engine-only, no CEF types.
namespace GodotCefSchemeUtil {

// Converts a canonical CEF URL ("res://host/path?query#fragment") to a Godot path.
// Directories (trailing slash or no file extension) map to index.html.
// Returns an empty string for other schemes or paths that try to escape the root.
String url_to_path(const String &p_url);

// CEF lowercases the host of standard-scheme URLs, so "res://UI/index.html" arrives as
// "res://ui/index.html". Restores the case of the first path segment if needed.
String resolve_case_insensitive_root(const String &p_path);

String get_mime_type(const String &p_path);

// Parses a single "bytes=" range. On success r_start..r_end is inclusive and within size.
bool parse_range(const String &p_header, int64_t p_size, int64_t &r_start, int64_t &r_end);

} // namespace GodotCefSchemeUtil
