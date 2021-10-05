#include<symboli/prelude.hpp>
#include<optional>
#include<cstddef>
#include<memory>
#include<iostream>
#include<mutex>
#include<condition_variable>
#include<thread>
#include"symboli/carotene/core_version.hpp"

static std::optional<symboli::prelude> prelude;

extern "C"{

__declspec(dllexport) unsigned int major_version(){
	return SYMBOLI_CAROTENE_CORE_EXPECTED_VERSION_MAJOR;
}

__declspec(dllexport) unsigned int minor_version(){
	return SYMBOLI_CAROTENE_CORE_EXPECTED_VERSION_MINOR;
}

__declspec(dllexport) unsigned int patch_version(){
	return SYMBOLI_CAROTENE_CORE_EXPECTED_VERSION_PATCH;
}

__declspec(dllexport) void* get_prelude(){
	return static_cast<void*>(&*prelude);
}

}

class task_t{
	struct input{
		const char* ptr;
		int size;
		enum class message_type{
			request,
			response
		}type;
	}input_data = {nullptr, 0};
	std::mutex mtx;
	std::condition_variable cv_ready_to_copy;
	std::condition_variable cv_finish_copy;
	bool met_demise = false;
	std::vector<std::function<void(const std::vector<std::byte>&)>> request_funcs;
	std::vector<std::function<void(const std::vector<std::byte>&)>> response_funcs;
	template<input::message_type Type>
	void copy(const char* ptr, int size){
		std::unique_lock lock{mtx};
		if constexpr(Type == input::message_type::request){
			if(request_funcs.empty())
				return;
		}
		else if constexpr(Type == input::message_type::response){
			if(response_funcs.empty())
				return;
		}
		input_data.ptr = ptr;
		input_data.size = size;
		input_data.type = Type;
		cv_ready_to_copy.notify_one();
		cv_finish_copy.wait(lock, [&]{return input_data.ptr == nullptr || met_demise;});
	}
public:
	task_t() = default;
	void demise(){
		std::scoped_lock lock{mtx};
		met_demise = true;
		cv_ready_to_copy.notify_all();
		cv_finish_copy.notify_all();
	}
	void request(const char* ptr, int size){
		this->copy<input::message_type::request>(ptr, size);
	}
	void response(const char* ptr, int size){
		this->copy<input::message_type::response>(ptr, size);
	}
	void add_request_func(std::function<void(const std::vector<std::byte>&)> f){
		std::scoped_lock lock{mtx};
		request_funcs.emplace_back(std::move(f));
	}
	void add_response_func(std::function<void(const std::vector<std::byte>&)> f){
		std::scoped_lock lock{mtx};
		response_funcs.emplace_back(std::move(f));
	}
	void operator()(){
		while(true){
			std::vector<std::byte> data;
			input::message_type type;
			{
				std::unique_lock lock{mtx};
				cv_ready_to_copy.wait(lock, [&]{return input_data.ptr != nullptr || met_demise;});
				if(met_demise)
					break;
				data.assign(reinterpret_cast<const std::byte*>(input_data.ptr), reinterpret_cast<const std::byte*>(input_data.ptr+input_data.size));
				type = input_data.type;
				input_data = {};
				cv_finish_copy.notify_one();
			}
			switch(type){
			case input::message_type::request:
				for(auto&& f : request_funcs)try{
					f(data);
				}catch(const std::exception& e){
					prelude->diagnostic("Carotene Core :: request", e.what());
				}
				break;
			case input::message_type::response:
				for(auto&& f : response_funcs)try{
					f(data);
				}catch(const std::exception& e){
					prelude->diagnostic("Carotene Core :: response", e.what());
				}
			}
		}
	}
}static task = {};
static std::thread task_thread;

struct lz4_decompress_safe_ext : symboli::hook_func<int(char*, char*, int, int), lz4_decompress_safe_ext>{
	static int func(char* src, char* dst, int compressed_size, int dst_capacity){
		const auto ret = orig(src, dst, compressed_size, dst_capacity);
		task.response(dst, ret);
		return ret;
	}
};

struct lz4_compress_default_ext : symboli::hook_func<int(char*, char*, int, int), lz4_compress_default_ext>{
	static int func(char* src, char* dst, int src_size, int dst_capacity){
		const auto ret = orig(src, dst, src_size, dst_capacity);
		task.request(src, src_size);
		return ret;
	}
};

__declspec(dllexport) void add_request_func(std::function<void(const std::vector<std::byte>&)> f){
	task.add_request_func(std::move(f));
}

__declspec(dllexport) void add_response_func(std::function<void(const std::vector<std::byte>&)> f){
	task.add_response_func(std::move(f));
}

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

	task_thread = std::thread{std::ref(task)};
	return TRUE;
}

static inline BOOL process_detach(){
	if(prelude)
		prelude.reset();
	task.demise();
	task_thread.join();
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
	::MessageBoxA(nullptr, e.what(), "Carotene Core exception", MB_OK|MB_ICONERROR|MB_SETFOREGROUND);
	return FALSE;
}
