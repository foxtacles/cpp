#include <limits>
#include <utility>
#include <cassert>

using namespace std;

class ScriptEngine;
typedef int data_type;
typedef data_type result_type;
typedef result_type (*function_type)(ScriptEngine*, const data_type*); 

constexpr int MyFunction(int a, int b) { return a + b; }
constexpr double OtherFunction(int a, int b, int c)  { return a * b / static_cast<double>(c); } 

template<typename T> struct sizeof_void { enum { value = sizeof(T) }; };  
template<> struct sizeof_void<void> { enum { value = 0 }; };

template<typename T, size_t t> struct TypeChar { static_assert(!t, "Unsupported type in variadic type list"); };  
template<typename T> struct TypeChar<T, sizeof(int)> { enum { value = 'i' }; };  
template<> struct TypeChar<double, sizeof(double)> { enum { value = 'f' }; };  
template<> struct TypeChar<char*, sizeof(char*)> { enum { value = 's' }; };  
template<> struct TypeChar<void, sizeof_void<void>::value> { enum { value = 'v' }; };

template<const char t> struct CharType { static_assert(!t, "Unsupported type in variadic type list"); };
template<> struct CharType<'i'> { typedef int type; };
template<> struct CharType<'f'> { typedef double type; };
template<> struct CharType<'s'> { typedef char* type; };
template<> struct CharType<'v'> { typedef void type; };

template<typename... Types>  
struct TypeString {  
    static constexpr char value[sizeof...(Types) + 1] = {
        TypeChar<Types, sizeof(Types)>::value...
    };
};

template<typename R, typename... Types>  
using Function = R(*)(Types...);

struct ScriptIdentity  
{
    Function<void> addr;
    const char* types;
    const char ret;
    const unsigned int numargs;

    template<typename R, typename... Types>
    constexpr ScriptIdentity(Function<R, Types...> addr) : addr(reinterpret_cast<Function<void>>(addr)), types(TypeString<Types...>::value), ret(TypeChar<R, sizeof_void<R>::value>::value), numargs(sizeof(TypeString<Types...>::value) - 1) {}
};

struct ScriptFunction  
{
    const char* name;
    const ScriptIdentity func;

	constexpr ScriptFunction(const char* name, ScriptIdentity func) : name(name), func(func) {}
};

struct ScriptWrapper
{
	const char* name;
	const function_type func;

	constexpr ScriptWrapper(const char* name, function_type func) : name(name), func(func) {}
};

template<typename... Types>
constexpr char TypeString<Types...>::value[];

static constexpr ScriptFunction functions[] {  
    {"MyFunction", MyFunction},
    {"OtherFunction", OtherFunction},
// ...
};

constexpr auto functions_num = sizeof(functions) / sizeof(functions[0]);
using functions_seq = make_index_sequence<functions_num>;

template<typename R, unsigned int I>
struct extract_ {
	inline static R extract(ScriptEngine*&&, const data_type*&& params) noexcept {
		return static_cast<R>(forward<const data_type*>(params)[I]);
	}
};

template<unsigned int I, unsigned int F>
struct dispatch_ {
	template<typename R, typename... Args>
	inline static R dispatch(ScriptEngine*&& se, const data_type*&& params, Args&&... args) noexcept {
		constexpr ScriptFunction const& F_ = functions[F];
		return dispatch_<I - 1, F>::template dispatch<R>(forward<ScriptEngine*>(se),
		                                                 forward<const data_type*>(params),
		                                                 extract_<typename CharType<F_.func.types[I - 1]>::type, I>::extract(forward<ScriptEngine*>(se),
		                                                                                                                     forward<const data_type*>(params)),
		                                                 forward<Args>(args)...);
	}
};

template<unsigned int F>
struct dispatch_<0, F> {
	template<typename R, typename... Args>
	inline static R dispatch(ScriptEngine*&&, const data_type*&&, Args&&... args) noexcept {
		constexpr ScriptFunction const& F_ = functions[F];
		return reinterpret_cast<R(*)(...)>(F_.func.addr)(forward<Args>(args)...);
	}
};

template<unsigned int F>
static result_type wrapper(ScriptEngine* se, const data_type* params) noexcept {
	auto result = dispatch_<functions[F].func.numargs, F>::template dispatch<typename CharType<functions[F].func.ret>::type>(forward<ScriptEngine*>(se), forward<const data_type*>(params));
	return static_cast<result_type>(result);
}

template<size_t... Is>
static const ScriptWrapper* wrappers(index_sequence<Is...>) {
	static constexpr ScriptWrapper wrappers_[] {
		{functions[Is].name, wrapper<Is>}...
	};

	constexpr auto wrapped_num = sizeof(wrappers_) / sizeof(wrappers_[0]);
	static_assert(functions_num == wrapped_num, "Not all functions wrapped");
    
	return wrappers_;
}

int main() {
	const ScriptWrapper* test = wrappers(functions_seq());
	
	const data_type test_call[] {1, 2, 3, 3};
	
	assert(test[0].func(nullptr, test_call) == MyFunction(2, 3));
	assert(test[1].func(nullptr, test_call) == OtherFunction(2, 3, 3));
	
	return 0;
}