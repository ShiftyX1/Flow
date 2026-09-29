/**************************************************************************/
/*  godot_cef_scheme.cpp                                                  */
/**************************************************************************/

#include "godot_cef_scheme.h"

#include "godot_cef_include.h"
#include "common/godot_cef_ipc_contract.h"
#include "godot_cef_ipc.h"
#include "godot_cef_scheme_util.h"

#include "core/io/file_access.h"

using GodotCefIpcData::to_cef_string;
using GodotCefIpcData::to_godot_string;

namespace {

// Serves one file from the Godot virtual filesystem. CEF calls these methods on its IO
// thread; FileAccess is safe to use from there.
class GodotCefResourceHandler : public CefResourceHandler {
	Ref<FileAccess> file;
	String mime_type;
	int status = 404;
	int64_t file_size = 0;
	int64_t range_start = 0;
	int64_t range_end = -1;
	bool partial = false;
	int64_t remaining = 0;

public:
	bool Open(CefRefPtr<CefRequest> request, bool &handle_request, CefRefPtr<CefCallback> callback) override {
		handle_request = true;
		String path = GodotCefSchemeUtil::resolve_case_insensitive_root(GodotCefSchemeUtil::url_to_path(to_godot_string(request->GetURL())));
		if (path.is_empty()) {
			status = 400;
			return true;
		}
		file = FileAccess::open(path, FileAccess::READ);
		if (file.is_null() && !path.get_file().contains_char('.')) {
			path = GodotCefSchemeUtil::resolve_case_insensitive_root(path.path_join("index.html"));
			file = FileAccess::open(path, FileAccess::READ);
		}
		if (file.is_null()) {
			status = 404;
			return true;
		}
		mime_type = GodotCefSchemeUtil::get_mime_type(path);
		file_size = int64_t(file->get_length());
		range_start = 0;
		range_end = file_size - 1;

		const String range = to_godot_string(request->GetHeaderByName("Range"));
		if (!range.is_empty()) {
			int64_t start = 0;
			int64_t end = 0;
			if (GodotCefSchemeUtil::parse_range(range, file_size, start, end)) {
				range_start = start;
				range_end = end;
				partial = true;
			} else if (file_size > 0) {
				status = 416;
				file.unref();
				return true;
			}
		}
		file->seek(uint64_t(range_start));
		remaining = MAX<int64_t>(0, range_end - range_start + 1);
		status = partial ? 206 : 200;
		return true;
	}

	void GetResponseHeaders(CefRefPtr<CefResponse> response, int64_t &response_length, CefString &redirectUrl) override {
		response->SetStatus(status);
		CefResponse::HeaderMap headers;
		headers.emplace("Access-Control-Allow-Origin", "*");
		headers.emplace("Cache-Control", "no-cache");
		if (status == 200 || status == 206) {
			response->SetMimeType(to_cef_string(mime_type));
			headers.emplace("Accept-Ranges", "bytes");
			if (partial) {
				headers.emplace("Content-Range", to_cef_string(vformat("bytes %d-%d/%d", range_start, range_end, file_size)));
			}
			response_length = remaining;
		} else {
			if (status == 416) {
				headers.emplace("Content-Range", to_cef_string(vformat("bytes */%d", file_size)));
			}
			response->SetMimeType("text/plain");
			response_length = 0;
		}
		response->SetHeaderMap(headers);
	}

	bool Read(void *data_out, int bytes_to_read, int &bytes_read, CefRefPtr<CefResourceReadCallback> callback) override {
		bytes_read = 0;
		if (file.is_null() || remaining <= 0) {
			return false;
		}
		const int64_t to_read = MIN<int64_t>(bytes_to_read, remaining);
		const uint64_t read = file->get_buffer(static_cast<uint8_t *>(data_out), uint64_t(to_read));
		if (read == 0) {
			return false;
		}
		bytes_read = int(read);
		remaining -= int64_t(read);
		return true;
	}

	bool Skip(int64_t bytes_to_skip, int64_t &bytes_skipped, CefRefPtr<CefResourceSkipCallback> callback) override {
		if (file.is_null() || remaining <= 0) {
			bytes_skipped = -2; // ERR_FAILED
			return false;
		}
		const int64_t skip = MIN(bytes_to_skip, remaining);
		file->seek(file->get_position() + uint64_t(skip));
		remaining -= skip;
		bytes_skipped = skip;
		return true;
	}

	void Cancel() override {
		file.unref();
		remaining = 0;
	}

	IMPLEMENT_REFCOUNTING(GodotCefResourceHandler);
};

class GodotCefSchemeHandlerFactory : public CefSchemeHandlerFactory {
public:
	CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, const CefString &scheme_name, CefRefPtr<CefRequest> request) override {
		return new GodotCefResourceHandler();
	}

	IMPLEMENT_REFCOUNTING(GodotCefSchemeHandlerFactory);
};

} // namespace

void godot_cef_register_scheme_handlers() {
	CefRefPtr<GodotCefSchemeHandlerFactory> factory = new GodotCefSchemeHandlerFactory();
	CefRegisterSchemeHandlerFactory(GodotCefIpc::SCHEME_RES, "", factory);
	CefRegisterSchemeHandlerFactory(GodotCefIpc::SCHEME_USER, "", factory);
}
