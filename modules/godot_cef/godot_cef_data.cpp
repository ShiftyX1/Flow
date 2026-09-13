/**************************************************************************/
/*  godot_cef_data.cpp                                                    */
/**************************************************************************/

#include "godot_cef_data.h"

#include "core/object/class_db.h"

Ref<DragDataInfo> DragDataInfo::create() {
	Ref<DragDataInfo> info;
	info.instantiate();
	return info;
}

void DragDataInfo::set_is_link(bool p_is_link) {
	link = p_is_link;
}

bool DragDataInfo::get_is_link() const {
	return link;
}

void DragDataInfo::set_is_file(bool p_is_file) {
	file = p_is_file;
}

bool DragDataInfo::get_is_file() const {
	return file;
}

void DragDataInfo::set_is_fragment(bool p_is_fragment) {
	fragment = p_is_fragment;
}

bool DragDataInfo::get_is_fragment() const {
	return fragment;
}

void DragDataInfo::set_link_url(const String &p_link_url) {
	link_url = p_link_url;
}

String DragDataInfo::get_link_url() const {
	return link_url;
}

void DragDataInfo::set_link_title(const String &p_link_title) {
	link_title = p_link_title;
}

String DragDataInfo::get_link_title() const {
	return link_title;
}

void DragDataInfo::set_fragment_text(const String &p_fragment_text) {
	fragment_text = p_fragment_text;
}

String DragDataInfo::get_fragment_text() const {
	return fragment_text;
}

void DragDataInfo::set_fragment_html(const String &p_fragment_html) {
	fragment_html = p_fragment_html;
}

String DragDataInfo::get_fragment_html() const {
	return fragment_html;
}

void DragDataInfo::set_file_names(const Array &p_file_names) {
	file_names = p_file_names;
}

Array DragDataInfo::get_file_names() const {
	return file_names;
}

void DragDataInfo::_bind_methods() {
	ClassDB::bind_static_method("DragDataInfo", D_METHOD("create"), &DragDataInfo::create);
	ClassDB::bind_method(D_METHOD("set_is_link", "is_link"), &DragDataInfo::set_is_link);
	ClassDB::bind_method(D_METHOD("get_is_link"), &DragDataInfo::get_is_link);
	ClassDB::bind_method(D_METHOD("set_is_file", "is_file"), &DragDataInfo::set_is_file);
	ClassDB::bind_method(D_METHOD("get_is_file"), &DragDataInfo::get_is_file);
	ClassDB::bind_method(D_METHOD("set_is_fragment", "is_fragment"), &DragDataInfo::set_is_fragment);
	ClassDB::bind_method(D_METHOD("get_is_fragment"), &DragDataInfo::get_is_fragment);
	ClassDB::bind_method(D_METHOD("set_link_url", "link_url"), &DragDataInfo::set_link_url);
	ClassDB::bind_method(D_METHOD("get_link_url"), &DragDataInfo::get_link_url);
	ClassDB::bind_method(D_METHOD("set_link_title", "link_title"), &DragDataInfo::set_link_title);
	ClassDB::bind_method(D_METHOD("get_link_title"), &DragDataInfo::get_link_title);
	ClassDB::bind_method(D_METHOD("set_fragment_text", "fragment_text"), &DragDataInfo::set_fragment_text);
	ClassDB::bind_method(D_METHOD("get_fragment_text"), &DragDataInfo::get_fragment_text);
	ClassDB::bind_method(D_METHOD("set_fragment_html", "fragment_html"), &DragDataInfo::set_fragment_html);
	ClassDB::bind_method(D_METHOD("get_fragment_html"), &DragDataInfo::get_fragment_html);
	ClassDB::bind_method(D_METHOD("set_file_names", "file_names"), &DragDataInfo::set_file_names);
	ClassDB::bind_method(D_METHOD("get_file_names"), &DragDataInfo::get_file_names);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_link"), "set_is_link", "get_is_link");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_file"), "set_is_file", "get_is_file");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_fragment"), "set_is_fragment", "get_is_fragment");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "link_url"), "set_link_url", "get_link_url");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "link_title"), "set_link_title", "get_link_title");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "fragment_text"), "set_fragment_text", "get_fragment_text");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "fragment_html"), "set_fragment_html", "get_fragment_html");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "file_names", PROPERTY_HINT_ARRAY_TYPE, "String"), "set_file_names", "get_file_names");
}

void DragOperation::_bind_methods() {
	BIND_ENUM_CONSTANT(NONE);
	BIND_ENUM_CONSTANT(COPY);
	BIND_ENUM_CONSTANT(LINK);
	BIND_ENUM_CONSTANT(MOVE);
	BIND_ENUM_CONSTANT(EVERY);
}

void DownloadRequestInfo::set_id(int64_t p_id) {
	id = p_id;
}

int64_t DownloadRequestInfo::get_id() const {
	return id;
}

void DownloadRequestInfo::set_url(const String &p_url) {
	url = p_url;
}

String DownloadRequestInfo::get_url() const {
	return url;
}

void DownloadRequestInfo::set_original_url(const String &p_original_url) {
	original_url = p_original_url;
}

String DownloadRequestInfo::get_original_url() const {
	return original_url;
}

void DownloadRequestInfo::set_suggested_file_name(const String &p_name) {
	suggested_file_name = p_name;
}

String DownloadRequestInfo::get_suggested_file_name() const {
	return suggested_file_name;
}

void DownloadRequestInfo::set_mime_type(const String &p_mime_type) {
	mime_type = p_mime_type;
}

String DownloadRequestInfo::get_mime_type() const {
	return mime_type;
}

void DownloadRequestInfo::set_total_bytes(int64_t p_total_bytes) {
	total_bytes = p_total_bytes;
}

int64_t DownloadRequestInfo::get_total_bytes() const {
	return total_bytes;
}

void DownloadRequestInfo::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_id", "id"), &DownloadRequestInfo::set_id);
	ClassDB::bind_method(D_METHOD("get_id"), &DownloadRequestInfo::get_id);
	ClassDB::bind_method(D_METHOD("set_url", "url"), &DownloadRequestInfo::set_url);
	ClassDB::bind_method(D_METHOD("get_url"), &DownloadRequestInfo::get_url);
	ClassDB::bind_method(D_METHOD("set_original_url", "original_url"), &DownloadRequestInfo::set_original_url);
	ClassDB::bind_method(D_METHOD("get_original_url"), &DownloadRequestInfo::get_original_url);
	ClassDB::bind_method(D_METHOD("set_suggested_file_name", "suggested_file_name"), &DownloadRequestInfo::set_suggested_file_name);
	ClassDB::bind_method(D_METHOD("get_suggested_file_name"), &DownloadRequestInfo::get_suggested_file_name);
	ClassDB::bind_method(D_METHOD("set_mime_type", "mime_type"), &DownloadRequestInfo::set_mime_type);
	ClassDB::bind_method(D_METHOD("get_mime_type"), &DownloadRequestInfo::get_mime_type);
	ClassDB::bind_method(D_METHOD("set_total_bytes", "total_bytes"), &DownloadRequestInfo::set_total_bytes);
	ClassDB::bind_method(D_METHOD("get_total_bytes"), &DownloadRequestInfo::get_total_bytes);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "id"), "set_id", "get_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "url"), "set_url", "get_url");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "original_url"), "set_original_url", "get_original_url");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "suggested_file_name"), "set_suggested_file_name", "get_suggested_file_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "mime_type"), "set_mime_type", "get_mime_type");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "total_bytes"), "set_total_bytes", "get_total_bytes");
}

void DownloadUpdateInfo::set_id(int64_t p_id) {
	id = p_id;
}

int64_t DownloadUpdateInfo::get_id() const {
	return id;
}

void DownloadUpdateInfo::set_url(const String &p_url) {
	url = p_url;
}

String DownloadUpdateInfo::get_url() const {
	return url;
}

void DownloadUpdateInfo::set_full_path(const String &p_full_path) {
	full_path = p_full_path;
}

String DownloadUpdateInfo::get_full_path() const {
	return full_path;
}

void DownloadUpdateInfo::set_received_bytes(int64_t p_received_bytes) {
	received_bytes = p_received_bytes;
}

int64_t DownloadUpdateInfo::get_received_bytes() const {
	return received_bytes;
}

void DownloadUpdateInfo::set_total_bytes(int64_t p_total_bytes) {
	total_bytes = p_total_bytes;
}

int64_t DownloadUpdateInfo::get_total_bytes() const {
	return total_bytes;
}

void DownloadUpdateInfo::set_current_speed(int64_t p_current_speed) {
	current_speed = p_current_speed;
}

int64_t DownloadUpdateInfo::get_current_speed() const {
	return current_speed;
}

void DownloadUpdateInfo::set_percent_complete(int p_percent_complete) {
	percent_complete = p_percent_complete;
}

int DownloadUpdateInfo::get_percent_complete() const {
	return percent_complete;
}

void DownloadUpdateInfo::set_is_in_progress(bool p_in_progress) {
	in_progress = p_in_progress;
}

bool DownloadUpdateInfo::get_is_in_progress() const {
	return in_progress;
}

void DownloadUpdateInfo::set_complete(bool p_complete) {
	complete = p_complete;
}

bool DownloadUpdateInfo::get_complete() const {
	return complete;
}

void DownloadUpdateInfo::set_is_complete(bool p_complete) {
	complete = p_complete;
}

bool DownloadUpdateInfo::get_is_complete() const {
	return complete;
}

void DownloadUpdateInfo::set_canceled(bool p_canceled) {
	canceled = p_canceled;
}

bool DownloadUpdateInfo::get_canceled() const {
	return canceled;
}

void DownloadUpdateInfo::set_is_canceled(bool p_canceled) {
	canceled = p_canceled;
}

bool DownloadUpdateInfo::get_is_canceled() const {
	return canceled;
}

void DownloadUpdateInfo::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_id", "id"), &DownloadUpdateInfo::set_id);
	ClassDB::bind_method(D_METHOD("get_id"), &DownloadUpdateInfo::get_id);
	ClassDB::bind_method(D_METHOD("set_url", "url"), &DownloadUpdateInfo::set_url);
	ClassDB::bind_method(D_METHOD("get_url"), &DownloadUpdateInfo::get_url);
	ClassDB::bind_method(D_METHOD("set_full_path", "full_path"), &DownloadUpdateInfo::set_full_path);
	ClassDB::bind_method(D_METHOD("get_full_path"), &DownloadUpdateInfo::get_full_path);
	ClassDB::bind_method(D_METHOD("set_received_bytes", "received_bytes"), &DownloadUpdateInfo::set_received_bytes);
	ClassDB::bind_method(D_METHOD("get_received_bytes"), &DownloadUpdateInfo::get_received_bytes);
	ClassDB::bind_method(D_METHOD("set_total_bytes", "total_bytes"), &DownloadUpdateInfo::set_total_bytes);
	ClassDB::bind_method(D_METHOD("get_total_bytes"), &DownloadUpdateInfo::get_total_bytes);
	ClassDB::bind_method(D_METHOD("set_current_speed", "current_speed"), &DownloadUpdateInfo::set_current_speed);
	ClassDB::bind_method(D_METHOD("get_current_speed"), &DownloadUpdateInfo::get_current_speed);
	ClassDB::bind_method(D_METHOD("set_percent_complete", "percent_complete"), &DownloadUpdateInfo::set_percent_complete);
	ClassDB::bind_method(D_METHOD("get_percent_complete"), &DownloadUpdateInfo::get_percent_complete);
	ClassDB::bind_method(D_METHOD("set_is_in_progress", "is_in_progress"), &DownloadUpdateInfo::set_is_in_progress);
	ClassDB::bind_method(D_METHOD("get_is_in_progress"), &DownloadUpdateInfo::get_is_in_progress);
	ClassDB::bind_method(D_METHOD("set_complete", "complete"), &DownloadUpdateInfo::set_complete);
	ClassDB::bind_method(D_METHOD("get_complete"), &DownloadUpdateInfo::get_complete);
	ClassDB::bind_method(D_METHOD("set_is_complete", "is_complete"), &DownloadUpdateInfo::set_is_complete);
	ClassDB::bind_method(D_METHOD("get_is_complete"), &DownloadUpdateInfo::get_is_complete);
	ClassDB::bind_method(D_METHOD("set_canceled", "canceled"), &DownloadUpdateInfo::set_canceled);
	ClassDB::bind_method(D_METHOD("get_canceled"), &DownloadUpdateInfo::get_canceled);
	ClassDB::bind_method(D_METHOD("set_is_canceled", "is_canceled"), &DownloadUpdateInfo::set_is_canceled);
	ClassDB::bind_method(D_METHOD("get_is_canceled"), &DownloadUpdateInfo::get_is_canceled);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "id"), "set_id", "get_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "url"), "set_url", "get_url");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "full_path"), "set_full_path", "get_full_path");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "received_bytes"), "set_received_bytes", "get_received_bytes");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "total_bytes"), "set_total_bytes", "get_total_bytes");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "current_speed"), "set_current_speed", "get_current_speed");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "percent_complete"), "set_percent_complete", "get_percent_complete");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_in_progress"), "set_is_in_progress", "get_is_in_progress");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_complete"), "set_is_complete", "get_is_complete");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_canceled"), "set_is_canceled", "get_is_canceled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "complete", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_complete", "get_complete");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "canceled", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_canceled", "get_canceled");
}

void CookieInfo::set_name(const String &p_name) {
	name = p_name;
}

String CookieInfo::get_name() const {
	return name;
}

void CookieInfo::set_value(const String &p_value) {
	value = p_value;
}

String CookieInfo::get_value() const {
	return value;
}

void CookieInfo::set_domain(const String &p_domain) {
	domain = p_domain;
}

String CookieInfo::get_domain() const {
	return domain;
}

void CookieInfo::set_path(const String &p_path) {
	path = p_path;
}

String CookieInfo::get_path() const {
	return path;
}

void CookieInfo::set_secure(bool p_secure) {
	secure = p_secure;
}

bool CookieInfo::get_secure() const {
	return secure;
}

void CookieInfo::set_http_only(bool p_http_only) {
	http_only = p_http_only;
}

bool CookieInfo::get_http_only() const {
	return http_only;
}

void CookieInfo::set_httponly(bool p_http_only) {
	http_only = p_http_only;
}

bool CookieInfo::get_httponly() const {
	return http_only;
}

void CookieInfo::set_same_site(int p_same_site) {
	same_site = p_same_site;
}

int CookieInfo::get_same_site() const {
	return same_site;
}

void CookieInfo::set_has_expires(bool p_has_expires) {
	has_expires = p_has_expires;
}

bool CookieInfo::get_has_expires() const {
	return has_expires;
}

void CookieInfo::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_name", "name"), &CookieInfo::set_name);
	ClassDB::bind_method(D_METHOD("get_name"), &CookieInfo::get_name);
	ClassDB::bind_method(D_METHOD("set_value", "value"), &CookieInfo::set_value);
	ClassDB::bind_method(D_METHOD("get_value"), &CookieInfo::get_value);
	ClassDB::bind_method(D_METHOD("set_domain", "domain"), &CookieInfo::set_domain);
	ClassDB::bind_method(D_METHOD("get_domain"), &CookieInfo::get_domain);
	ClassDB::bind_method(D_METHOD("set_path", "path"), &CookieInfo::set_path);
	ClassDB::bind_method(D_METHOD("get_path"), &CookieInfo::get_path);
	ClassDB::bind_method(D_METHOD("set_secure", "secure"), &CookieInfo::set_secure);
	ClassDB::bind_method(D_METHOD("get_secure"), &CookieInfo::get_secure);
	ClassDB::bind_method(D_METHOD("set_http_only", "http_only"), &CookieInfo::set_http_only);
	ClassDB::bind_method(D_METHOD("get_http_only"), &CookieInfo::get_http_only);
	ClassDB::bind_method(D_METHOD("set_httponly", "httponly"), &CookieInfo::set_httponly);
	ClassDB::bind_method(D_METHOD("get_httponly"), &CookieInfo::get_httponly);
	ClassDB::bind_method(D_METHOD("set_same_site", "same_site"), &CookieInfo::set_same_site);
	ClassDB::bind_method(D_METHOD("get_same_site"), &CookieInfo::get_same_site);
	ClassDB::bind_method(D_METHOD("set_has_expires", "has_expires"), &CookieInfo::set_has_expires);
	ClassDB::bind_method(D_METHOD("get_has_expires"), &CookieInfo::get_has_expires);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "set_name", "get_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "value"), "set_value", "get_value");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "domain"), "set_domain", "get_domain");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "path"), "set_path", "get_path");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "secure"), "set_secure", "get_secure");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "httponly"), "set_httponly", "get_httponly");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "same_site"), "set_same_site", "get_same_site");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "has_expires"), "set_has_expires", "get_has_expires");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "http_only", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_http_only", "get_http_only");
}
