/**************************************************************************/
/*  godot_cef_settings.cpp                                                */
/**************************************************************************/

#include "godot_cef_settings.h"

#include "core/config/project_settings.h"

const String GodotCefSettings::SETTING_DATA_PATH = "godot_cef/storage/data_path";
const String GodotCefSettings::SETTING_ALLOW_INSECURE_CONTENT = "godot_cef/security/allow_insecure_content";
const String GodotCefSettings::SETTING_IGNORE_CERTIFICATE_ERRORS = "godot_cef/security/ignore_certificate_errors";
const String GodotCefSettings::SETTING_DISABLE_WEB_SECURITY = "godot_cef/security/disable_web_security";
const String GodotCefSettings::SETTING_DEFAULT_PERMISSION_POLICY = "godot_cef/security/default_permission_policy";
const String GodotCefSettings::SETTING_ENABLE_AUDIO_CAPTURE = "godot_cef/audio/enable_audio_capture";
const String GodotCefSettings::SETTING_REMOTE_DEVTOOLS_PORT = "godot_cef/debug/remote_devtools_port";
const String GodotCefSettings::SETTING_MAX_FRAME_RATE = "godot_cef/performance/max_frame_rate";
const String GodotCefSettings::SETTING_CACHE_SIZE_MB = "godot_cef/storage/cache_size_mb";
const String GodotCefSettings::SETTING_USER_AGENT = "godot_cef/network/user_agent";
const String GodotCefSettings::SETTING_PROXY_SERVER = "godot_cef/network/proxy_server";
const String GodotCefSettings::SETTING_PROXY_BYPASS_LIST = "godot_cef/network/proxy_bypass_list";
const String GodotCefSettings::SETTING_ENABLE_ADBLOCK = "godot_cef/network/enable_adblock";
const String GodotCefSettings::SETTING_ADBLOCK_RULES_PATH = "godot_cef/network/adblock_rules_path";
const String GodotCefSettings::SETTING_CUSTOM_SWITCHES = "godot_cef/advanced/custom_command_line_switches";

const String GodotCefSettings::DEFAULT_DATA_PATH = "user://cef-data";
const String GodotCefSettings::DEFAULT_USER_AGENT = "";
const String GodotCefSettings::DEFAULT_PROXY_SERVER = "";
const String GodotCefSettings::DEFAULT_PROXY_BYPASS_LIST = "";
const String GodotCefSettings::DEFAULT_ADBLOCK_RULES_PATH = "";
const String GodotCefSettings::DEFAULT_CUSTOM_SWITCHES = "";

void GodotCefSettings::register_project_settings() {
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, SETTING_DATA_PATH, PROPERTY_HINT_DIR), DEFAULT_DATA_PATH);
	GLOBAL_DEF_BASIC(SETTING_ALLOW_INSECURE_CONTENT, DEFAULT_ALLOW_INSECURE_CONTENT);
	GLOBAL_DEF_BASIC(SETTING_IGNORE_CERTIFICATE_ERRORS, DEFAULT_IGNORE_CERTIFICATE_ERRORS);
	GLOBAL_DEF_BASIC(SETTING_DISABLE_WEB_SECURITY, DEFAULT_DISABLE_WEB_SECURITY);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, SETTING_DEFAULT_PERMISSION_POLICY, PROPERTY_HINT_ENUM, "DenyAll:0,AllowAll:1,Signal:2"), DEFAULT_PERMISSION_POLICY);
	GLOBAL_DEF_BASIC(SETTING_ENABLE_AUDIO_CAPTURE, DEFAULT_ENABLE_AUDIO_CAPTURE);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, SETTING_REMOTE_DEVTOOLS_PORT, PROPERTY_HINT_RANGE, "1,65535"), DEFAULT_REMOTE_DEVTOOLS_PORT);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, SETTING_MAX_FRAME_RATE, PROPERTY_HINT_RANGE, "0,240,or_greater"), DEFAULT_MAX_FRAME_RATE);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, SETTING_CACHE_SIZE_MB, PROPERTY_HINT_RANGE, "0,10240,or_greater"), DEFAULT_CACHE_SIZE_MB);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, SETTING_USER_AGENT, PROPERTY_HINT_PLACEHOLDER_TEXT, "Custom user agent string (empty = CEF default)"), DEFAULT_USER_AGENT);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, SETTING_PROXY_SERVER, PROPERTY_HINT_PLACEHOLDER_TEXT, "socks5://127.0.0.1:1080 or http://proxy:8080"), DEFAULT_PROXY_SERVER);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, SETTING_PROXY_BYPASS_LIST, PROPERTY_HINT_PLACEHOLDER_TEXT, "localhost,127.0.0.1"), DEFAULT_PROXY_BYPASS_LIST);
	GLOBAL_DEF_BASIC(SETTING_ENABLE_ADBLOCK, DEFAULT_ENABLE_ADBLOCK);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, SETTING_ADBLOCK_RULES_PATH, PROPERTY_HINT_PLACEHOLDER_TEXT, "Path to EasyList/ABP rules file"), DEFAULT_ADBLOCK_RULES_PATH);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, SETTING_CUSTOM_SWITCHES, PROPERTY_HINT_MULTILINE_TEXT), DEFAULT_CUSTOM_SWITCHES);
}

String GodotCefSettings::get_data_path() {
	ProjectSettings *settings = ProjectSettings::get_singleton();
	const String configured = GLOBAL_GET(SETTING_DATA_PATH);
	return settings ? settings->globalize_path(configured) : configured;
}

int GodotCefSettings::get_remote_devtools_port() {
	return GLOBAL_GET(SETTING_REMOTE_DEVTOOLS_PORT);
}

bool GodotCefSettings::is_audio_capture_enabled() {
	return GLOBAL_GET(SETTING_ENABLE_AUDIO_CAPTURE);
}
