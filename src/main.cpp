#include<symboli/prelude.hpp>
#include<optional>
#include<cstddef>
#include<nlohmann/json.hpp>
#include<fstream>
#include<memory>
#include<iostream>

static std::optional<symboli::prelude> prelude;

struct config_t{
	struct{
		bool request;
		bool response;
	}save;
	std::filesystem::path export_directory;
}static config = {
	.save = {
		.request = false,
		.response = false
	},
	.export_directory = ""
};
static inline void from_json(const nlohmann::json& j, config_t& conf){
	auto config_opt_read = [](const nlohmann::json& j, const nlohmann::json::object_t::key_type& key, auto& value){
		prelude->config_read<true>("Symboli Carotene :: config_read", j, key, value);
	};
	if(j.contains("save") && j["save"].is_object()){
		config_opt_read(j["save"], "request", conf.save.request);
		config_opt_read(j["save"], "response", conf.save.response);
	}
	if(j.contains("export_directory") && j["export_directory"].is_string()){
		std::string str;
		j["export_directory"].get_to(str);
		conf.export_directory = str;
	}
}

will::expected<void, std::error_code> write_file(std::filesystem::path path, const char* buffer, std::size_t len){
	::FILE* fp;
	auto ret = ::fopen_s(&fp, path.string().c_str(), "wb");
	if(ret)
		return will::make_unexpected(std::error_code{ret, std::generic_category()});
	std::unique_ptr<::FILE, decltype(&::fclose)> _{fp, &::fclose};
	::fwrite(buffer, 1, len, fp);
	return {};
}

struct lz4_decompress_safe_ext : symboli::hook_func<int(char*, char*, int, int), lz4_decompress_safe_ext>{
	static int func(char* src, char* dst, int compressed_size, int dst_capacity){
		const auto ret = orig(src, dst, compressed_size, dst_capacity);
		if(config.save.response){
			const auto current_time = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
			write_file(config.export_directory/(current_time+"R.msgpack"), dst, static_cast<std::size_t>(ret));
		}
		return ret;
	}
};

struct lz4_compress_default_ext : symboli::hook_func<int(char*, char*, int, int), lz4_compress_default_ext>{
	static int func(char* src, char* dst, int src_size, int dst_capacity){
		const auto ret = orig(src, dst, src_size, dst_capacity);
		if(config.save.request){
			const auto current_time = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
			write_file(config.export_directory/(current_time+"Q.msgpack"), src, static_cast<std::size_t>(src_size));
		}
		return ret;
	}
};

static inline BOOL process_attach(HINSTANCE hinst){
	const std::filesystem::path plugin_path{will::get_module_file_name(hinst).value()};
	prelude =+ symboli::prelude::create(plugin_path.parent_path()/"symboli_prelude.dll");

	prelude->enqueue_task([]{
		auto libnative =+ will::get_module_handle(_T("libnative.dll"));
		const auto LZ4_decompress_safe_ext =+ libnative.get_proc_address<int(char*, char*, int, int)>("LZ4_decompress_safe_ext");
		prelude->hook<lz4_decompress_safe_ext>(LZ4_decompress_safe_ext);
		const auto LZ4_compress_default_ext =+ libnative.get_proc_address<int(char*, char*, int, int)>("LZ4_compress_default_ext");
		prelude->hook<lz4_compress_default_ext>(LZ4_compress_default_ext);
	});
	return TRUE;
}

static inline BOOL process_detach(){
	if(prelude)
		prelude.reset();
	return TRUE;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID)try{
	switch(fdwReason){
	case DLL_PROCESS_ATTACH:
		return process_attach(hinstDLL);
	case DLL_PROCESS_DETACH:
		return process_detach();
	default:
		return TRUE;
	}
}catch(std::exception& e){
	::MessageBoxA(nullptr, e.what(), "Symboli Renderer exception", MB_OK|MB_ICONERROR|MB_SETFOREGROUND);
	return FALSE;
}
