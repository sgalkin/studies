#include <iostream>
#include <type_traits>
#include <functional>
#include <string>
#include <algorithm>
#include <sstream>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>

using namespace std::placeholders;

template<typename Acquire, typename Release>
class handle_
{
	typedef typename std::result_of<Acquire()>::type T;
public:
	handle_(const Acquire& acquire, const Release& release) : 
		h_(acquire()), r_(release) 
	{}

	handle_() = delete;
	handle_(const handle_&) = delete;
	handle_& operator=(const handle_&) = delete;

	handle_(handle_&& rhs) : h_(), r_([](T){}) { std::swap(h_, rhs.h_); }
	~handle_() { r_(h_); }

	operator T() { return h_; }
	operator const T&() const { return h_; }
	//operator T&() { return h_; }

private:
	T h_;
	Release r_;
};

template<typename Acquire, typename Release>
auto handle(const Acquire& a, const Release& r) -> decltype(handle_<Acquire, Release>(a, r))
{
	return handle_<Acquire, Release>(a, r);
}

#define CALL(a) (std::make_tuple((a), std::string(#a)))
#define THROW(type, message) \
	do { \
		std::stringstream s##__LINE__; \
		s##__LINE__ << message; \
		throw type(s##__LINE__.str()); \
	} while (false)

template<typename Call, typename F, typename... Args>
auto apply(Call&& call, F&& filter, Args&&... args) -> decltype(std::get<0>(call)(args...))
{
	return filter(std::get<0>(call)(args...), std::get<1>(call));
}

template<typename T>
T apicall(T&& result, const std::string& operation)
{
	if (result != (T)-1) return result;
	THROW(std::runtime_error, "unable to complete operation: " << operation << " with errno: " << errno);
}

int main()
{
	std::string filename = "main.cpp";

	try {
		auto h = handle([&filename]() {
			return apply(CALL([](const char* f) { 
				int hh = 0; _sopen_s(&hh, f, _O_RDONLY, _SH_DENYNO, _S_IREAD); return hh; 
			}), &apicall<int>, filename.c_str());
		}, &_close);

		auto seek_end = std::bind(_lseek, _1, _2, SEEK_END);

		//long sz = apply(CALL([](int h) { return _lseek(h, 0, SEEK_END); }), &apicall<long>, h);
		long sz = apply(CALL(seek_end), &apicall<long>, h, 0);
		long psz = std::min(sz, 16 * 1024 * 1024l);
		//long pos = apply(CALL([](int h, long pos) { return _lseek(h, -pos, SEEK_END); }), &apicall<long>, h, psz);
		long pos = apply(CALL(seek_end), &apicall<long>, h, -psz);
			
		std::string buf(psz, 0);
		//apply(CALL([](int h, std::string& buf) { return _read(h, &buf[0], buf.size()); }), &apicall<long>, h, std::ref(buf));
		apply(CALL(std::bind(_read, _1, &buf[0], buf.size())), &apicall<long>, h);

		std::cout << buf << std::endl;
	} catch (std::exception& ex) { 
		std::cerr << ex.what();
	}
	return 0;
}
