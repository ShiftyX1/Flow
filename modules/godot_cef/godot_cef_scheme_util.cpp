/**************************************************************************/
/*  godot_cef_scheme_util.cpp                                             */
/**************************************************************************/

#include "godot_cef_scheme_util.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"

namespace GodotCefSchemeUtil {

String url_to_path(const String &p_url) {
	String scheme;
	if (p_url.begins_with("res://")) {
		scheme = "res://";
	} else if (p_url.begins_with("user://")) {
		scheme = "user://";
	} else {
		return String();
	}

	String rest = p_url.substr(scheme.length());
	const int query = rest.find_char('?');
	if (query >= 0) {
		rest = rest.substr(0, query);
	}
	const int fragment = rest.find_char('#');
	if (fragment >= 0) {
		rest = rest.substr(0, fragment);
	}
	rest = rest.uri_decode();
	const bool trailing_slash = rest.is_empty() || rest.ends_with("/") || rest.ends_with("\\");

	Vector<String> segments;
	for (const String &segment : rest.replace_char('\\', '/').split("/", false)) {
		if (segment == ".") {
			continue;
		}
		if (segment == "..") {
			if (segments.is_empty()) {
				return String();
			}
			segments.remove_at(segments.size() - 1);
			continue;
		}
		if (segment.contains_char(':')) {
			return String();
		}
		segments.push_back(segment);
	}
	if (segments.is_empty()) {
		return scheme + "index.html";
	}
	String path = scheme + String("/").join(segments);
	const String last = segments[segments.size() - 1];
	if (trailing_slash || !last.contains_char('.')) {
		path = path.path_join("index.html");
	}
	return path;
}

String resolve_case_insensitive_root(const String &p_path) {
	if (p_path.is_empty() || FileAccess::exists(p_path)) {
		return p_path;
	}
	const int scheme_end = p_path.find("://");
	if (scheme_end < 0) {
		return p_path;
	}
	const String root = p_path.substr(0, scheme_end + 3);
	const String rest = p_path.substr(root.length());
	const int slash = rest.find_char('/');
	const String first = slash >= 0 ? rest.substr(0, slash) : rest;
	const String tail = slash >= 0 ? rest.substr(slash) : String();

	Ref<DirAccess> dir = DirAccess::open(root);
	if (dir.is_null()) {
		return p_path;
	}
	dir->list_dir_begin();
	for (String name = dir->get_next(); !name.is_empty(); name = dir->get_next()) {
		if (name != first && name.nocasecmp_to(first) == 0) {
			dir->list_dir_end();
			return root + name + tail;
		}
	}
	dir->list_dir_end();
	return p_path;
}

String get_mime_type(const String &p_path) {
	const String ext = p_path.get_extension().to_lower();
	struct MimeEntry {
		const char *ext;
		const char *mime;
	};
	static const MimeEntry entries[] = {
		{ "html", "text/html" },
		{ "htm", "text/html" },
		{ "css", "text/css" },
		{ "js", "text/javascript" },
		{ "mjs", "text/javascript" },
		{ "json", "application/json" },
		{ "map", "application/json" },
		{ "txt", "text/plain" },
		{ "xml", "application/xml" },
		{ "svg", "image/svg+xml" },
		{ "png", "image/png" },
		{ "jpg", "image/jpeg" },
		{ "jpeg", "image/jpeg" },
		{ "gif", "image/gif" },
		{ "webp", "image/webp" },
		{ "avif", "image/avif" },
		{ "ico", "image/x-icon" },
		{ "bmp", "image/bmp" },
		{ "woff", "font/woff" },
		{ "woff2", "font/woff2" },
		{ "ttf", "font/ttf" },
		{ "otf", "font/otf" },
		{ "wasm", "application/wasm" },
		{ "mp3", "audio/mpeg" },
		{ "ogg", "audio/ogg" },
		{ "oga", "audio/ogg" },
		{ "wav", "audio/wav" },
		{ "flac", "audio/flac" },
		{ "mp4", "video/mp4" },
		{ "m4v", "video/mp4" },
		{ "webm", "video/webm" },
		{ "ogv", "video/ogg" },
		{ "pdf", "application/pdf" },
		{ "zip", "application/zip" },
	};
	for (const MimeEntry &entry : entries) {
		if (ext == entry.ext) {
			return entry.mime;
		}
	}
	return "application/octet-stream";
}

bool parse_range(const String &p_header, int64_t p_size, int64_t &r_start, int64_t &r_end) {
	const String header = p_header.strip_edges();
	if (!header.begins_with("bytes=") || p_size <= 0) {
		return false;
	}
	const String spec = header.substr(6).strip_edges();
	if (spec.contains_char(',')) {
		// Multipart ranges are not supported; the full body is served instead.
		return false;
	}
	const int dash = spec.find_char('-');
	if (dash < 0) {
		return false;
	}
	const String first = spec.substr(0, dash).strip_edges();
	const String last = spec.substr(dash + 1).strip_edges();
	if (first.is_empty()) {
		if (last.is_empty() || !last.is_valid_int()) {
			return false;
		}
		const int64_t suffix = last.to_int();
		if (suffix <= 0) {
			return false;
		}
		r_start = MAX<int64_t>(0, p_size - suffix);
		r_end = p_size - 1;
		return true;
	}
	if (!first.is_valid_int()) {
		return false;
	}
	r_start = first.to_int();
	if (r_start < 0 || r_start >= p_size) {
		return false;
	}
	if (last.is_empty()) {
		r_end = p_size - 1;
	} else {
		if (!last.is_valid_int()) {
			return false;
		}
		r_end = MIN(last.to_int(), p_size - 1);
		if (r_end < r_start) {
			return false;
		}
	}
	return true;
}

} // namespace GodotCefSchemeUtil
