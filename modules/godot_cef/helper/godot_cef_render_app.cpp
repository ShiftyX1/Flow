/**************************************************************************/
/*  godot_cef_render_app.cpp                                              */
/**************************************************************************/

#include "godot_cef_render_app.h"

#include "../common/godot_cef_ipc_contract.h"
#include "ime_helper_js.h"

#include "include/cef_v8.h"

#include <climits>
#include <cstdio>
#include <functional>
#include <vector>

namespace {

constexpr const char *LISTENER_CALLBACKS_KEY = "__godotCefListenerCallbacks";

bool send_to_browser(const char *p_route, const std::function<void(CefRefPtr<CefListValue>)> &p_fill) {
	CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
	if (!context) {
		return false;
	}
	CefRefPtr<CefFrame> frame = context->GetFrame();
	if (!frame) {
		return false;
	}
	CefRefPtr<CefProcessMessage> message = CefProcessMessage::Create(p_route);
	p_fill(message->GetArgumentList());
	frame->SendProcessMessage(PID_BROWSER, message);
	return true;
}

// JS value -> CefValue, used by `sendIpcData`. Returns nullptr and sets p_error on failure.
CefRefPtr<CefValue> v8_to_cef_value(CefRefPtr<CefV8Value> p_value, size_t &r_bytes, std::string &r_error, int p_depth = 0) {
	CefRefPtr<CefValue> out = CefValue::Create();
	if (p_depth > 64) {
		r_error = "IPC data is nested too deeply";
		return nullptr;
	}
	if (!p_value || p_value->IsUndefined() || p_value->IsNull()) {
		out->SetNull();
	} else if (p_value->IsBool()) {
		out->SetBool(p_value->GetBoolValue());
	} else if (p_value->IsInt()) {
		out->SetInt(p_value->GetIntValue());
	} else if (p_value->IsUInt()) {
		const uint32_t v = p_value->GetUIntValue();
		if (v <= INT_MAX) {
			out->SetInt(int(v));
		} else {
			out->SetDouble(double(v));
		}
	} else if (p_value->IsDouble()) {
		out->SetDouble(p_value->GetDoubleValue());
	} else if (p_value->IsString()) {
		const std::string s = p_value->GetStringValue().ToString();
		r_bytes += s.size();
		out->SetString(s);
	} else if (p_value->IsArrayBuffer()) {
		const size_t length = p_value->GetArrayBufferByteLength();
		r_bytes += length;
		if (r_bytes > GodotCefIpc::MAX_IPC_DATA_BYTES) {
			r_error = "ArrayBuffer exceeds the maximum IPC data size";
			return nullptr;
		}
		if (length == 0) {
			// CefBinaryValue cannot be empty.
			CefRefPtr<CefDictionaryValue> tagged = CefDictionaryValue::Create();
			tagged->SetString(GodotCefIpc::TYPED_VALUE_TYPE_KEY, GodotCefIpc::TYPED_VALUE_EMPTY_BYTES);
			tagged->SetString(GodotCefIpc::TYPED_VALUE_VALUE_KEY, "");
			out->SetDictionary(tagged);
		} else {
			out->SetBinary(CefBinaryValue::Create(p_value->GetArrayBufferData(), length));
		}
	} else if (p_value->IsArray()) {
		CefRefPtr<CefListValue> list = CefListValue::Create();
		const int length = p_value->GetArrayLength();
		list->SetSize(size_t(length));
		for (int i = 0; i < length; i++) {
			CefRefPtr<CefValue> element = v8_to_cef_value(p_value->GetValue(i), r_bytes, r_error, p_depth + 1);
			if (!element) {
				return nullptr;
			}
			list->SetValue(size_t(i), element);
		}
		out->SetList(list);
	} else if (p_value->IsFunction()) {
		r_error = "Functions cannot be sent over IPC";
		return nullptr;
	} else if (p_value->IsObject()) {
		CefRefPtr<CefDictionaryValue> dict = CefDictionaryValue::Create();
		std::vector<CefString> keys;
		p_value->GetKeys(keys);
		for (const CefString &key : keys) {
			CefRefPtr<CefValue> element = v8_to_cef_value(p_value->GetValue(key), r_bytes, r_error, p_depth + 1);
			if (!element) {
				return nullptr;
			}
			dict->SetValue(key, element);
		}
		out->SetDictionary(dict);
	} else {
		r_error = "Unsupported JS value for IPC";
		return nullptr;
	}
	if (r_bytes > GodotCefIpc::MAX_IPC_DATA_BYTES) {
		r_error = "IPC data payload exceeds the maximum size";
		return nullptr;
	}
	return out;
}

CefRefPtr<CefV8Value> cef_value_to_v8(CefRefPtr<CefValue> p_value) {
	if (!p_value) {
		return CefV8Value::CreateNull();
	}
	switch (p_value->GetType()) {
		case VTYPE_BOOL:
			return CefV8Value::CreateBool(p_value->GetBool());
		case VTYPE_INT:
			return CefV8Value::CreateInt(p_value->GetInt());
		case VTYPE_DOUBLE:
			return CefV8Value::CreateDouble(p_value->GetDouble());
		case VTYPE_STRING:
			return CefV8Value::CreateString(p_value->GetString());
		case VTYPE_BINARY: {
			CefRefPtr<CefBinaryValue> binary = p_value->GetBinary();
			if (!binary) {
				return CefV8Value::CreateArrayBufferWithCopy(nullptr, 0);
			}
			std::vector<uint8_t> bytes(binary->GetSize());
			if (!bytes.empty()) {
				binary->GetData(bytes.data(), bytes.size(), 0);
			}
			return CefV8Value::CreateArrayBufferWithCopy(bytes.data(), bytes.size());
		}
		case VTYPE_LIST: {
			CefRefPtr<CefListValue> list = p_value->GetList();
			CefRefPtr<CefV8Value> array = CefV8Value::CreateArray(int(list->GetSize()));
			for (size_t i = 0; i < list->GetSize(); i++) {
				array->SetValue(int(i), cef_value_to_v8(list->GetValue(i)));
			}
			return array;
		}
		case VTYPE_DICTIONARY: {
			CefRefPtr<CefDictionaryValue> dict = p_value->GetDictionary();
			CefRefPtr<CefV8Value> object = CefV8Value::CreateObject(nullptr, nullptr);
			CefDictionaryValue::KeyList keys;
			dict->GetKeys(keys);
			for (const CefString &key : keys) {
				object->SetValue(key, cef_value_to_v8(dict->GetValue(key)), V8_PROPERTY_ATTRIBUTE_NONE);
			}
			return object;
		}
		default:
			return CefV8Value::CreateNull();
	}
}

CefRefPtr<CefV8Value> listener_callbacks(CefRefPtr<CefV8Value> p_object) {
	if (!p_object || !p_object->IsObject()) {
		return nullptr;
	}
	CefRefPtr<CefV8Value> callbacks = p_object->GetValue(LISTENER_CALLBACKS_KEY);
	return (callbacks && callbacks->IsArray()) ? callbacks : nullptr;
}

std::vector<CefRefPtr<CefV8Value>> collect_callbacks(CefRefPtr<CefV8Value> p_callbacks) {
	std::vector<CefRefPtr<CefV8Value>> out;
	if (!p_callbacks) {
		return out;
	}
	for (int i = 0; i < p_callbacks->GetArrayLength(); i++) {
		CefRefPtr<CefV8Value> callback = p_callbacks->GetValue(i);
		if (callback && callback->IsValid() && callback->IsFunction()) {
			out.push_back(callback);
		}
	}
	return out;
}

void store_callbacks(CefRefPtr<CefV8Value> p_object, const std::vector<CefRefPtr<CefV8Value>> &p_callbacks) {
	CefRefPtr<CefV8Value> array = CefV8Value::CreateArray(int(p_callbacks.size()));
	for (size_t i = 0; i < p_callbacks.size(); i++) {
		array->SetValue(int(i), p_callbacks[i]);
	}
	p_object->SetValue(LISTENER_CALLBACKS_KEY, array, cef_v8_propertyattribute_t(V8_PROPERTY_ATTRIBUTE_DONTENUM | V8_PROPERTY_ATTRIBUTE_DONTDELETE));
}

class GodotCefV8Handler : public CefV8Handler {
public:
	enum Kind {
		SEND_MESSAGE,
		SEND_BINARY,
		SEND_DATA,
		IME_CARET,
		LISTENER_ADD,
		LISTENER_REMOVE,
		LISTENER_HAS,
	};

private:
	Kind kind;

	bool _listener_op(CefRefPtr<CefV8Value> p_object, CefRefPtr<CefV8Value> p_callback) {
		CefRefPtr<CefV8Value> callbacks = listener_callbacks(p_object);
		if (!callbacks || !p_callback || !p_callback->IsFunction()) {
			return false;
		}
		std::vector<CefRefPtr<CefV8Value>> list = collect_callbacks(callbacks);
		int found = -1;
		for (size_t i = 0; i < list.size(); i++) {
			if (list[i]->IsSame(p_callback)) {
				found = int(i);
				break;
			}
		}
		switch (kind) {
			case LISTENER_ADD:
				if (found < 0) {
					list.push_back(p_callback);
				}
				store_callbacks(p_object, list);
				return true;
			case LISTENER_REMOVE:
				if (found >= 0) {
					list.erase(list.begin() + found);
					store_callbacks(p_object, list);
				}
				return found >= 0;
			default:
				return found >= 0;
		}
	}

public:
	explicit GodotCefV8Handler(Kind p_kind) :
			kind(p_kind) {}

	bool Execute(const CefString &p_name, CefRefPtr<CefV8Value> p_object, const CefV8ValueList &p_arguments, CefRefPtr<CefV8Value> &r_retval, CefString &r_exception) override {
		bool ok = false;
		CefRefPtr<CefV8Value> arg = p_arguments.empty() ? nullptr : p_arguments[0];

		switch (kind) {
			case SEND_MESSAGE: {
				if (arg && arg->IsString()) {
					const CefString message = arg->GetStringValue();
					ok = send_to_browser(GodotCefIpc::ROUTE_IPC_RENDERER_TO_GODOT, [&](CefRefPtr<CefListValue> args) {
						args->SetString(0, message);
					});
				}
			} break;
			case SEND_BINARY: {
				if (arg && arg->IsArrayBuffer() && arg->GetArrayBufferByteLength() > 0) {
					CefRefPtr<CefBinaryValue> binary = CefBinaryValue::Create(arg->GetArrayBufferData(), arg->GetArrayBufferByteLength());
					ok = send_to_browser(GodotCefIpc::ROUTE_IPC_BINARY_RENDERER_TO_GODOT, [&](CefRefPtr<CefListValue> args) {
						args->SetBinary(0, binary);
					});
				}
			} break;
			case SEND_DATA: {
				size_t bytes = 0;
				std::string error;
				CefRefPtr<CefValue> value = v8_to_cef_value(arg, bytes, error);
				if (!value) {
					r_exception = error;
					r_retval = CefV8Value::CreateBool(false);
					return true;
				}
				ok = send_to_browser(GodotCefIpc::ROUTE_IPC_DATA_RENDERER_TO_GODOT, [&](CefRefPtr<CefListValue> args) {
					args->SetValue(0, value);
				});
			} break;
			case IME_CARET: {
				if (p_arguments.size() >= 3) {
					const int x = p_arguments[0]->GetIntValue();
					const int y = p_arguments[1]->GetIntValue();
					const int height = p_arguments[2]->GetIntValue();
					ok = send_to_browser(GodotCefIpc::ROUTE_IME_CARET_POSITION, [&](CefRefPtr<CefListValue> args) {
						args->SetInt(0, x);
						args->SetInt(1, y);
						args->SetInt(2, height);
					});
				}
			} break;
			case LISTENER_ADD:
			case LISTENER_REMOVE:
			case LISTENER_HAS: {
				ok = _listener_op(p_object, arg);
			} break;
		}

		r_retval = CefV8Value::CreateBool(ok);
		return true;
	}

	IMPLEMENT_REFCOUNTING(GodotCefV8Handler);
};

void register_function(CefRefPtr<CefV8Value> p_target, const char *p_name, GodotCefV8Handler::Kind p_kind) {
	p_target->SetValue(p_name, CefV8Value::CreateFunction(p_name, new GodotCefV8Handler(p_kind)), V8_PROPERTY_ATTRIBUTE_NONE);
}

CefRefPtr<CefV8Value> create_listener_object() {
	CefRefPtr<CefV8Value> object = CefV8Value::CreateObject(nullptr, nullptr);
	store_callbacks(object, {});
	register_function(object, "addListener", GodotCefV8Handler::LISTENER_ADD);
	register_function(object, "removeListener", GodotCefV8Handler::LISTENER_REMOVE);
	register_function(object, "hasListener", GodotCefV8Handler::LISTENER_HAS);
	return object;
}

// Delivers a Godot -> JS message to the legacy `window.<callback>` and to every listener of `window.<listener_api>`.
void deliver_to_js(CefRefPtr<CefFrame> p_frame, const char *p_callback, const char *p_listener_api, const std::function<CefRefPtr<CefV8Value>()> &p_make_value) {
	CefRefPtr<CefV8Context> context = p_frame->GetV8Context();
	if (!context || !context->Enter()) {
		return;
	}
	CefRefPtr<CefV8Value> global = context->GetGlobal();
	CefRefPtr<CefV8Value> value = p_make_value();
	if (global && value) {
		CefV8ValueList args = { value };
		CefRefPtr<CefV8Value> callback = global->GetValue(p_callback);
		if (callback && callback->IsFunction()) {
			callback->ExecuteFunction(global, args);
		}
		for (CefRefPtr<CefV8Value> listener : collect_callbacks(listener_callbacks(global->GetValue(p_listener_api)))) {
			listener->ExecuteFunction(global, args);
		}
	}
	context->Exit();
}

void send_ime_trigger(CefRefPtr<CefFrame> p_frame, bool p_active) {
	CefRefPtr<CefProcessMessage> message = CefProcessMessage::Create(GodotCefIpc::ROUTE_TRIGGER_IME);
	message->GetArgumentList()->SetBool(0, p_active);
	p_frame->SendProcessMessage(PID_BROWSER, message);
}

} // namespace

void GodotCefRenderApp::OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> p_registrar) {
	GodotCefIpc::register_custom_schemes(p_registrar);
}

void GodotCefRenderApp::OnBrowserCreated(CefRefPtr<CefBrowser> p_browser, CefRefPtr<CefDictionaryValue> p_extra_info) {
	const int id = p_browser->GetIdentifier();
	preload_scripts.erase(id);
	if (p_extra_info && p_extra_info->HasKey(GodotCefIpc::EXTRA_INFO_PRELOAD_SCRIPT)) {
		const std::string script = p_extra_info->GetString(GodotCefIpc::EXTRA_INFO_PRELOAD_SCRIPT).ToString();
		if (!script.empty()) {
			preload_scripts[id] = script;
		}
	}
}

void GodotCefRenderApp::OnBrowserDestroyed(CefRefPtr<CefBrowser> p_browser) {
	preload_scripts.erase(p_browser->GetIdentifier());
}

void GodotCefRenderApp::OnContextCreated(CefRefPtr<CefBrowser> p_browser, CefRefPtr<CefFrame> p_frame, CefRefPtr<CefV8Context> p_context) {
	CefRefPtr<CefV8Value> global = p_context->GetGlobal();
	register_function(global, "sendIpcMessage", GodotCefV8Handler::SEND_MESSAGE);
	register_function(global, "sendIpcBinaryMessage", GodotCefV8Handler::SEND_BINARY);
	register_function(global, "sendIpcData", GodotCefV8Handler::SEND_DATA);
	register_function(global, "__sendImeCaretPosition", GodotCefV8Handler::IME_CARET);
	for (const char *name : { "ipcMessage", "ipcBinaryMessage", "ipcDataMessage" }) {
		global->SetValue(name, create_listener_object(), V8_PROPERTY_ATTRIBUTE_NONE);
	}

	p_frame->ExecuteJavaScript(GODOT_CEF_IME_HELPER_JS, "godot-cef://ime-helper.js", 0);

	if (!p_frame->IsMain()) {
		return;
	}
	auto it = preload_scripts.find(p_browser->GetIdentifier());
	if (it == preload_scripts.end()) {
		return;
	}
	CefRefPtr<CefV8Value> retval;
	CefRefPtr<CefV8Exception> exception;
	if (!p_context->Eval(it->second, "godot-cef://user-preload.js", 1, retval, exception) && exception) {
		fprintf(stderr, "[GodotCef] Preload script failed at line %d: %s\n", exception->GetLineNumber(), exception->GetMessage().ToString().c_str());
	}
}

void GodotCefRenderApp::OnFocusedNodeChanged(CefRefPtr<CefBrowser> p_browser, CefRefPtr<CefFrame> p_frame, CefRefPtr<CefDOMNode> p_node) {
	if (!p_frame) {
		return;
	}
	const bool editable = p_node && p_node->IsEditable();
	send_ime_trigger(p_frame, editable);
	p_frame->ExecuteJavaScript(editable ? "if(window.__activateImeTracking)window.__activateImeTracking();" : "if(window.__deactivateImeTracking)window.__deactivateImeTracking();", "", 0);
}

bool GodotCefRenderApp::OnProcessMessageReceived(CefRefPtr<CefBrowser> p_browser, CefRefPtr<CefFrame> p_frame, CefProcessId p_source_process, CefRefPtr<CefProcessMessage> p_message) {
	if (!p_frame) {
		return false;
	}
	const std::string route = p_message->GetName().ToString();
	CefRefPtr<CefListValue> args = p_message->GetArgumentList();

	if (route == GodotCefIpc::ROUTE_IPC_GODOT_TO_RENDERER) {
		const CefString text = args->GetString(0);
		deliver_to_js(p_frame, "onIpcMessage", "ipcMessage", [&]() { return CefV8Value::CreateString(text); });
		return true;
	}
	if (route == GodotCefIpc::ROUTE_IPC_BINARY_GODOT_TO_RENDERER) {
		CefRefPtr<CefValue> value = CefValue::Create();
		if (CefRefPtr<CefBinaryValue> binary = args->GetBinary(0)) {
			value->SetBinary(binary);
		}
		deliver_to_js(p_frame, "onIpcBinaryMessage", "ipcBinaryMessage", [&]() { return cef_value_to_v8(value); });
		return true;
	}
	if (route == GodotCefIpc::ROUTE_IPC_DATA_GODOT_TO_RENDERER) {
		CefRefPtr<CefValue> value = args->GetValue(0);
		deliver_to_js(p_frame, "onIpcDataMessage", "ipcDataMessage", [&]() { return cef_value_to_v8(value); });
		return true;
	}
	return false;
}
