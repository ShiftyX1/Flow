/**************************************************************************/
/*  godot_cef_settings.h                                                  */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"

class GodotCefSettings {
public:
	enum PermissionPolicy {
		PERMISSION_POLICY_DENY_ALL = 0,
		PERMISSION_POLICY_ALLOW_ALL = 1,
		PERMISSION_POLICY_SIGNAL = 2,
	};

	static const String SETTING_DATA_PATH;
	static const String SETTING_ALLOW_INSECURE_CONTENT;
	static const String SETTING_IGNORE_CERTIFICATE_ERRORS;
	static const String SETTING_DISABLE_WEB_SECURITY;
	static const String SETTING_DEFAULT_PERMISSION_POLICY;
	static const String SETTING_ENABLE_AUDIO_CAPTURE;
	static const String SETTING_REMOTE_DEVTOOLS_PORT;
	static const String SETTING_MAX_FRAME_RATE;
	static const String SETTING_CACHE_SIZE_MB;
	static const String SETTING_USER_AGENT;
	static const String SETTING_PROXY_SERVER;
	static const String SETTING_PROXY_BYPASS_LIST;
	static const String SETTING_ENABLE_ADBLOCK;
	static const String SETTING_ADBLOCK_RULES_PATH;
	static const String SETTING_CUSTOM_SWITCHES;

	static const String DEFAULT_DATA_PATH;
	static constexpr bool DEFAULT_ALLOW_INSECURE_CONTENT = false;
	static constexpr bool DEFAULT_IGNORE_CERTIFICATE_ERRORS = false;
	static constexpr bool DEFAULT_DISABLE_WEB_SECURITY = false;
	static constexpr int DEFAULT_PERMISSION_POLICY = PERMISSION_POLICY_DENY_ALL;
	static constexpr bool DEFAULT_ENABLE_AUDIO_CAPTURE = false;
	static constexpr int DEFAULT_REMOTE_DEVTOOLS_PORT = 9229;
	static constexpr int DEFAULT_MAX_FRAME_RATE = 0;
	static constexpr int DEFAULT_CACHE_SIZE_MB = 0;
	static const String DEFAULT_USER_AGENT;
	static const String DEFAULT_PROXY_SERVER;
	static const String DEFAULT_PROXY_BYPASS_LIST;
	static constexpr bool DEFAULT_ENABLE_ADBLOCK = false;
	static const String DEFAULT_ADBLOCK_RULES_PATH;
	static const String DEFAULT_CUSTOM_SWITCHES;

	static void register_project_settings();
	static String get_data_path();
	static int get_remote_devtools_port();
	static bool is_audio_capture_enabled();
};
