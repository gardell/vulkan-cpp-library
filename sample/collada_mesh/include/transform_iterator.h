#ifndef _SAMPLE_COLLADA_TRANSFORM_ITERATOR_H_
#define _SAMPLE_COLLADA_TRANSFORM_ITERATOR_H_

#include <iterator>

template<typename It, typename F>
struct output_transform_iterator_type {

	decltype(auto) operator++() {
		++iterator;
		return *this;
	}

	auto operator++(int) {
		return output_transform_iterator_type<It>(iterator++);
	}

	decltype(auto) operator*() {
		return *this;
	}

	template<typename T>
	decltype(auto) operator=(T value) {
		return it.iterator = f(std::forward<T>(value));
	}

	It iterator;
	F f;
};

template<typename It, typename F>
auto make_output_transform_iterator(It iterator, F f) {
	return output_transform_iterator_type<It, F>{
		std::forward<It>(iterator),
		std::forward<F>(f)
	};
}

template<typename It, typename F>
struct input_transform_iterator_type : public std::iterator<
	std::input_iterator_tag,
	decltype(std::declval<F>()(*std::declval<It>()))> {

	template<typename It_, typename F_>
	input_transform_iterator_type(It_ iterator, F_ f)
		: iterator(std::forward<It>(iterator))
		, f(std::forward<F>(f)) {}

	decltype(auto) operator++() {
		++iterator;
		return *this;
	}

	auto operator++(int) {
		return input_transform_iterator_type<It>(iterator++);
	}

	auto operator*() {
		return f(*iterator);
	}

	template<typename T>
	decltype(auto) operator=(T value) {
		return iterator = f(std::forward<T>(value));
	}

	bool operator==(const input_transform_iterator_type &other) const {
		return iterator == other.iterator;
	}
	bool operator!=(const input_transform_iterator_type &other) const {
		return !this->operator==(other);
	}

	It iterator;
	F f;
};

template<typename It, typename F>
auto make_input_transform_iterator(It iterator, F f) {
	return input_transform_iterator_type<It, F>(
		std::forward<It>(iterator),
		std::forward<F>(f));
}

#endif // _SAMPLE_COLLADA_TRANSFORM_ITERATOR_H_
