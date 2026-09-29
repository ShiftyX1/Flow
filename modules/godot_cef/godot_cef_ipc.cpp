/**************************************************************************/
/*  godot_cef_ipc.cpp                                                     */
/**************************************************************************/

#include "godot_cef_ipc.h"

#include "common/godot_cef_ipc_contract.h"

#include "core/variant/variant_parser.h"
#include "core/variant/variant_utility.h"

namespace GodotCefIpcData {

static constexpr int MAX_DEPTH = 64;
static constexpr int64_t MAX_SAFE_JS_INTEGER = (int64_t(1) << 53) - 1;

String to_godot_string(const CefString &p_string) {
	const std::string utf8 = p_string.ToString();
	return String::utf8(utf8.data(), int(utf8.size()));
}

CefString to_cef_string(const String &p_string) {
	const CharString utf8 = p_string.utf8();
	return CefString(std::string(utf8.get_data(), utf8.length()));
}

static CefRefPtr<CefValue> _make_tagged(const String &p_type, const String &p_value) {
	CefRefPtr<CefDictionaryValue> dict = CefDictionaryValue::Create();
	dict->SetString(GodotCefIpc::TYPED_VALUE_TYPE_KEY, to_cef_string(p_type));
	dict->SetString(GodotCefIpc::TYPED_VALUE_VALUE_KEY, to_cef_string(p_value));
	CefRefPtr<CefValue> value = CefValue::Create();
	value->SetDictionary(dict);
	return value;
}

static CefRefPtr<CefValue> _to_cef(const Variant &p_value, int p_depth) {
	CefRefPtr<CefValue> out = CefValue::Create();
	if (p_depth > MAX_DEPTH) {
		out->SetNull();
		return out;
	}

	switch (p_value.get_type()) {
		case Variant::NIL: {
			out->SetNull();
		} break;
		case Variant::BOOL: {
			out->SetBool(bool(p_value));
		} break;
		case Variant::INT: {
			const int64_t v = p_value;
			if (v >= INT32_MIN && v <= INT32_MAX) {
				out->SetInt(int(v));
			} else if (v >= -MAX_SAFE_JS_INTEGER && v <= MAX_SAFE_JS_INTEGER) {
				out->SetDouble(double(v));
			} else {
				return _make_tagged(itos(Variant::INT), itos(v));
			}
		} break;
		case Variant::FLOAT: {
			out->SetDouble(double(p_value));
		} break;
		case Variant::STRING:
		case Variant::STRING_NAME:
		case Variant::NODE_PATH: {
			out->SetString(to_cef_string(p_value));
		} break;
		case Variant::PACKED_BYTE_ARRAY: {
			const PackedByteArray bytes = p_value;
			if (bytes.is_empty()) {
				return _make_tagged(GodotCefIpc::TYPED_VALUE_EMPTY_BYTES, String());
			}
			out->SetBinary(CefBinaryValue::Create(bytes.ptr(), size_t(bytes.size())));
		} break;
		case Variant::ARRAY:
		case Variant::PACKED_INT32_ARRAY:
		case Variant::PACKED_INT64_ARRAY:
		case Variant::PACKED_FLOAT32_ARRAY:
		case Variant::PACKED_FLOAT64_ARRAY:
		case Variant::PACKED_STRING_ARRAY: {
			const Array array = p_value;
			CefRefPtr<CefListValue> list = CefListValue::Create();
			list->SetSize(size_t(array.size()));
			for (int i = 0; i < array.size(); i++) {
				list->SetValue(size_t(i), _to_cef(array[i], p_depth + 1));
			}
			out->SetList(list);
		} break;
		case Variant::DICTIONARY: {
			const Dictionary dict = p_value;
			CefRefPtr<CefDictionaryValue> cef_dict = CefDictionaryValue::Create();
			for (const KeyValue<Variant, Variant> &kv : dict) {
				cef_dict->SetValue(to_cef_string(kv.key.operator String()), _to_cef(kv.value, p_depth + 1));
			}
			out->SetDictionary(cef_dict);
		} break;
		case Variant::OBJECT:
		case Variant::CALLABLE:
		case Variant::SIGNAL:
		case Variant::RID: {
			// Not transferable across processes.
			out->SetNull();
		} break;
		default: {
			return _make_tagged(itos(p_value.get_type()), VariantUtilityFunctions::var_to_str(p_value));
		} break;
	}
	return out;
}

CefRefPtr<CefValue> variant_to_cef_value(const Variant &p_value) {
	return _to_cef(p_value, 0);
}

static bool _is_restorable_type(Variant::Type p_type) {
	switch (p_type) {
		case Variant::NIL:
		case Variant::OBJECT:
		case Variant::CALLABLE:
		case Variant::SIGNAL:
		case Variant::RID:
		case Variant::DICTIONARY:
		case Variant::ARRAY:
		case Variant::VARIANT_MAX:
			return false;
		default:
			return true;
	}
}

// Restores a tagged value. The text comes from web content, so it is only parsed when it is a
// plain constructor of the declared type (no nested objects can be instantiated that way).
static bool _restore_tagged(const CefRefPtr<CefDictionaryValue> &p_dict, Variant &r_value) {
	if (p_dict->GetSize() != 2 ||
			p_dict->GetType(GodotCefIpc::TYPED_VALUE_TYPE_KEY) != VTYPE_STRING ||
			p_dict->GetType(GodotCefIpc::TYPED_VALUE_VALUE_KEY) != VTYPE_STRING) {
		return false;
	}
	const String type = to_godot_string(p_dict->GetString(GodotCefIpc::TYPED_VALUE_TYPE_KEY));
	const String text = to_godot_string(p_dict->GetString(GodotCefIpc::TYPED_VALUE_VALUE_KEY));

	if (type == GodotCefIpc::TYPED_VALUE_EMPTY_BYTES) {
		r_value = PackedByteArray();
		return true;
	}
	if (!type.is_valid_int()) {
		return false;
	}
	const int64_t type_index = type.to_int();
	if (type_index <= Variant::NIL || type_index >= Variant::VARIANT_MAX) {
		return false;
	}
	const Variant::Type variant_type = Variant::Type(type_index);
	if (variant_type == Variant::INT) {
		if (!text.is_valid_int()) {
			return false;
		}
		r_value = text.to_int();
		return true;
	}
	if (!_is_restorable_type(variant_type)) {
		return false;
	}
	if (variant_type == Variant::STRING || variant_type == Variant::STRING_NAME || variant_type == Variant::NODE_PATH) {
		r_value = text;
		return true;
	}
	const String expected_prefix = Variant::get_type_name(variant_type) + "(";
	if (!text.begins_with(expected_prefix) || text.contains("Object(") || text.contains("Resource(")) {
		return false;
	}

	VariantParser::StreamString stream;
	stream.s = text;
	String error;
	int line = 0;
	Variant parsed;
	if (VariantParser::parse(&stream, parsed, error, line) != OK || parsed.get_type() != variant_type) {
		return false;
	}
	r_value = parsed;
	return true;
}

static Variant _to_variant(const CefRefPtr<CefValue> &p_value, int p_depth) {
	if (!p_value || p_depth > MAX_DEPTH) {
		return Variant();
	}
	switch (p_value->GetType()) {
		case VTYPE_BOOL:
			return bool(p_value->GetBool());
		case VTYPE_INT:
			return int64_t(p_value->GetInt());
		case VTYPE_DOUBLE:
			return p_value->GetDouble();
		case VTYPE_STRING:
			return to_godot_string(p_value->GetString());
		case VTYPE_BINARY: {
			CefRefPtr<CefBinaryValue> binary = p_value->GetBinary();
			PackedByteArray bytes;
			if (binary && binary->GetSize() > 0) {
				bytes.resize(int64_t(binary->GetSize()));
				binary->GetData(bytes.ptrw(), binary->GetSize(), 0);
			}
			return bytes;
		}
		case VTYPE_LIST: {
			CefRefPtr<CefListValue> list = p_value->GetList();
			Array array;
			array.resize(int(list->GetSize()));
			for (size_t i = 0; i < list->GetSize(); i++) {
				array[int(i)] = _to_variant(list->GetValue(i), p_depth + 1);
			}
			return array;
		}
		case VTYPE_DICTIONARY: {
			CefRefPtr<CefDictionaryValue> cef_dict = p_value->GetDictionary();
			Variant restored;
			if (_restore_tagged(cef_dict, restored)) {
				return restored;
			}
			CefDictionaryValue::KeyList keys;
			cef_dict->GetKeys(keys);
			Dictionary dict;
			for (const CefString &key : keys) {
				dict[to_godot_string(key)] = _to_variant(cef_dict->GetValue(key), p_depth + 1);
			}
			return dict;
		}
		default:
			return Variant();
	}
}

Variant cef_value_to_variant(const CefRefPtr<CefValue> &p_value) {
	return _to_variant(p_value, 0);
}

} // namespace GodotCefIpcData
