#pragma once
#include<will/_windows.hpp>
#include<functional>
#include<cstddef>
#include<symboli/prelude.hpp>
#include"core_version.hpp"

namespace symboli::carotene{

class core : will::module_handle{
	core(will::module_handle&& mod, void (*add_request_func)(std::function<void(const std::vector<std::byte>&)>), void (*add_response_func)(std::function<void(const std::vector<std::byte>&)>), symboli::prelude* prelude, unsigned int major_version, unsigned int minor_version, unsigned int patch_version) : will::module_handle{std::move(mod)}, add_request_func_{add_request_func}, prelude{prelude}, add_response_func_{add_response_func}, major_version_{major_version}, minor_version_{minor_version}, patch_version_{patch_version}{}
	void (*add_request_func_)(std::function<void(const std::vector<std::byte>&)>);
	void (*add_response_func_)(std::function<void(const std::vector<std::byte>&)>);
	symboli::prelude* prelude;
	unsigned int major_version_;
	unsigned int minor_version_;
	unsigned int patch_version_;
public:
	core(core&&) = default;
	core& operator=(core&&) = default;
	static will::expected<core, will::winapi_last_error> create(const std::filesystem::path& dll_path = "symboli_carotene_core.dll"){
		auto module = will::load_library(will::tchar::to_tstring(dll_path));
		if(!module)
			return will::make_unexpected(module.error());
		const auto add_request_func = module->get_proc_address<void(std::function<void(const std::vector<std::byte>&)>)>("?add_request_func@@YAXV?$function@$$A6AXAEBV?$vector@W4byte@std@@V?$allocator@W4byte@std@@@2@@std@@@Z@std@@@Z");
		if(!add_request_func)
			return will::make_unexpected(add_request_func.error());
		const auto add_response_func = module->get_proc_address<void(std::function<void(const std::vector<std::byte>&)>)>("?add_response_func@@YAXV?$function@$$A6AXAEBV?$vector@W4byte@std@@V?$allocator@W4byte@std@@@2@@std@@@Z@std@@@Z");
		if(!add_response_func)
			return will::make_unexpected(add_response_func.error());
		const auto get_prelude = module->get_proc_address<void*()>("get_prelude");
		if(!get_prelude)
			return will::make_unexpected(get_prelude.error());
		const auto major_version = module->get_proc_address<unsigned int()>("major_version");
		if(!major_version)
			return will::make_unexpected(major_version.error());
		const auto minor_version = module->get_proc_address<unsigned int()>("minor_version");
		if(!minor_version)
			return will::make_unexpected(minor_version.error());
		const auto patch_version = module->get_proc_address<unsigned int()>("patch_version");
		if(!patch_version)
			return will::make_unexpected(patch_version.error());
		const unsigned int major = (*major_version)();
		const unsigned int minor = (*minor_version)();
		const unsigned int patch = (*patch_version)();
		if(!symboli::prelude::version_check(major, minor, patch, SYMBOLI_CAROTENE_CORE_EXPECTED_VERSION_MAJOR, SYMBOLI_CAROTENE_CORE_EXPECTED_VERSION_MINOR, SYMBOLI_CAROTENE_CORE_EXPECTED_VERSION_PATCH))
			return will::make_unexpected<will::winapi_last_error>(_T("symboli::carotene::core::version_check"), PEERDIST_ERROR_VERSION_UNSUPPORTED);
		return core{std::move(*module), *add_request_func, *add_response_func, static_cast<symboli::prelude*>((*get_prelude)()), major, minor, patch};
	}
	
	void add_request_func(std::function<void(const std::vector<std::byte>&)> f)const{
		add_request_func_(std::move(f));
	}

	void add_response_func(std::function<void(const std::vector<std::byte>&)> f)const{
		add_response_func_(std::move(f));
	}

	unsigned int major_version()const{
		return major_version_;
	}
	unsigned int minor_version()const{
		return minor_version_;
	}
	unsigned int patch_version()const{
		return patch_version_;
	}

	void diagnostic(const char* module, const char* message)const{
		prelude->diagnostic(module, message);
	}

	template<bool Opt, typename J, typename T>
	void config_read(const char* diagnostic_module, const J& j, const typename J::object_t::key_type& key, T& t)const{
		prelude->config_read<Opt>(diagnostic_module, j, key, t);
	}
};

}
