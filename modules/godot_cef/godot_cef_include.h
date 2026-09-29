/**************************************************************************/
/*  godot_cef_include.h                                                   */
/**************************************************************************/

#pragma once

// Every engine-side translation unit must include CEF through this header.
// Godot's global `Error` enum and CEF's `cef_errorcode_t` both declare
// ERR_FILE_NOT_FOUND and ERR_OUT_OF_MEMORY, so the CEF ones are renamed.

#include "core/error/error_list.h"

#ifndef GODOT_CEF_SDK_INCLUDED
#define GODOT_CEF_SDK_INCLUDED

#pragma push_macro("ERR_FILE_NOT_FOUND")
#pragma push_macro("ERR_OUT_OF_MEMORY")
#undef ERR_FILE_NOT_FOUND
#undef ERR_OUT_OF_MEMORY
#define ERR_FILE_NOT_FOUND CEF_ERR_FILE_NOT_FOUND
#define ERR_OUT_OF_MEMORY CEF_ERR_OUT_OF_MEMORY

#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_command_line.h"
#include "include/cef_cookie.h"
#include "include/cef_drag_data.h"
#include "include/cef_parser.h"
#include "include/cef_request_context.h"
#include "include/cef_resource_handler.h"
#include "include/cef_scheme.h"
#include "include/cef_task.h"
#include "include/cef_values.h"
#include "include/wrapper/cef_helpers.h"

#pragma pop_macro("ERR_OUT_OF_MEMORY")
#pragma pop_macro("ERR_FILE_NOT_FOUND")

#endif // GODOT_CEF_SDK_INCLUDED
