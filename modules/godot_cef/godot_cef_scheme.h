/**************************************************************************/
/*  godot_cef_scheme.h                                                    */
/**************************************************************************/

#pragma once

// Registers the res:// and user:// scheme handler factories. Called from
// CefBrowserProcessHandler::OnContextInitialized.
void godot_cef_register_scheme_handlers();
