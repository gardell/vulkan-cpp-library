#ifndef _GLTF_OPTIONAL_H_
#define _GLTF_OPTIONAL_H_


inline std::optional<std::reference_wrapper<const json>> optional_ref(const json &j, std::string key) {
	auto it(j.find(key));
	auto v = it != j.end() ? std::make_optional(std::cref(*it)) : std::optional<std::reference_wrapper<const json>>();
	return v;
}

template<typename T>
std::optional<T> optional_cast(const json &json, std::string key) {
	auto it(json.find(key));
	return it != json.end() ? std::make_optional(it->get<T>()) : std::optional<T>();
}

template<typename Out>
struct map_func_type {
	template<typename T, typename F>
	static std::optional<Out> map(T optional, F &&f) {
		return optional.has_value() ? f(*std::forward<T>(optional)) : std::optional<Out>();
	}
};

template<>
struct map_func_type<void> {
	template<typename T, typename F>
	static void map(T optional, F &&f) {
		if (optional.has_value()) {
			f(*std::forward<T>(optional));
		}
	}
};

template<typename T, typename F>
auto map(std::optional<T> &&optional, F &&f) {
	typedef decltype(f(std::declval<T>())) result_type;
	return map_func_type<result_type>::map(
		std::forward<std::optional<T>>(optional),
		std::forward<F>(f));
}

template<typename T, typename F>
auto map(const std::optional<T> &optional, F &&f) {
	typedef decltype(f(std::declval<T>())) result_type;
	return map_func_type<result_type>::map<const std::optional<T> &>(
		optional,
		std::forward<F>(f));
}

#endif // _GLTF_OPTIONAL_H_
