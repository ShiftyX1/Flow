/**************************************************************************/
/*  godot_cef_data.h                                                      */
/**************************************************************************/

#pragma once

#include "core/object/ref_counted.h"
#include "core/variant/array.h"
#include "core/variant/type_info.h"

class DragDataInfo : public RefCounted {
	GDCLASS(DragDataInfo, RefCounted);

	bool link = false;
	bool file = false;
	bool fragment = false;
	String link_url;
	String link_title;
	String fragment_text;
	String fragment_html;
	Array file_names;

protected:
	static void _bind_methods();

public:
	static Ref<DragDataInfo> create();

	void set_is_link(bool p_is_link);
	bool get_is_link() const;
	void set_is_file(bool p_is_file);
	bool get_is_file() const;
	void set_is_fragment(bool p_is_fragment);
	bool get_is_fragment() const;
	void set_link_url(const String &p_link_url);
	String get_link_url() const;
	void set_link_title(const String &p_link_title);
	String get_link_title() const;
	void set_fragment_text(const String &p_fragment_text);
	String get_fragment_text() const;
	void set_fragment_html(const String &p_fragment_html);
	String get_fragment_html() const;
	void set_file_names(const Array &p_file_names);
	Array get_file_names() const;
};

class DragOperation : public RefCounted {
	GDCLASS(DragOperation, RefCounted);

protected:
	static void _bind_methods();

public:
	enum Operation {
		NONE = 0,
		COPY = 1,
		LINK = 2,
		MOVE = 16,
		EVERY = 2147483647,
	};
};

VARIANT_ENUM_CAST(DragOperation::Operation);

class DownloadRequestInfo : public RefCounted {
	GDCLASS(DownloadRequestInfo, RefCounted);

	int64_t id = 0;
	String url;
	String original_url;
	String suggested_file_name;
	String mime_type;
	int64_t total_bytes = -1;

protected:
	static void _bind_methods();

public:
	void set_id(int64_t p_id);
	int64_t get_id() const;
	void set_url(const String &p_url);
	String get_url() const;
	void set_original_url(const String &p_original_url);
	String get_original_url() const;
	void set_suggested_file_name(const String &p_name);
	String get_suggested_file_name() const;
	void set_mime_type(const String &p_mime_type);
	String get_mime_type() const;
	void set_total_bytes(int64_t p_total_bytes);
	int64_t get_total_bytes() const;
};

class DownloadUpdateInfo : public RefCounted {
	GDCLASS(DownloadUpdateInfo, RefCounted);

	int64_t id = 0;
	String url;
	String full_path;
	int64_t received_bytes = 0;
	int64_t total_bytes = -1;
	int64_t current_speed = 0;
	int percent_complete = -1;
	bool in_progress = false;
	bool complete = false;
	bool canceled = false;

protected:
	static void _bind_methods();

public:
	void set_id(int64_t p_id);
	int64_t get_id() const;
	void set_url(const String &p_url);
	String get_url() const;
	void set_full_path(const String &p_full_path);
	String get_full_path() const;
	void set_received_bytes(int64_t p_received_bytes);
	int64_t get_received_bytes() const;
	void set_total_bytes(int64_t p_total_bytes);
	int64_t get_total_bytes() const;
	void set_current_speed(int64_t p_current_speed);
	int64_t get_current_speed() const;
	void set_percent_complete(int p_percent_complete);
	int get_percent_complete() const;
	void set_is_in_progress(bool p_in_progress);
	bool get_is_in_progress() const;
	void set_complete(bool p_complete);
	bool get_complete() const;
	void set_is_complete(bool p_complete);
	bool get_is_complete() const;
	void set_canceled(bool p_canceled);
	bool get_canceled() const;
	void set_is_canceled(bool p_canceled);
	bool get_is_canceled() const;
};

class CookieInfo : public RefCounted {
	GDCLASS(CookieInfo, RefCounted);

	String name;
	String value;
	String domain;
	String path;
	bool secure = false;
	bool http_only = false;
	int same_site = 0;
	bool has_expires = false;

protected:
	static void _bind_methods();

public:
	void set_name(const String &p_name);
	String get_name() const;
	void set_value(const String &p_value);
	String get_value() const;
	void set_domain(const String &p_domain);
	String get_domain() const;
	void set_path(const String &p_path);
	String get_path() const;
	void set_secure(bool p_secure);
	bool get_secure() const;
	void set_http_only(bool p_http_only);
	bool get_http_only() const;
	void set_httponly(bool p_http_only);
	bool get_httponly() const;
	void set_same_site(int p_same_site);
	int get_same_site() const;
	void set_has_expires(bool p_has_expires);
	bool get_has_expires() const;
};
