#pragma once

//Given class U, where U supports operator T() and operator=(T), intwrap<U> enables all the integer operators.
//Most are already supported by casting to the integer type, but this one adds the assignment operators too.
template<typename U, typename T = U> class intwrap : public U {
	T get() { return *this; }
	void set(T val) { this->U::operator=(val); }
public:
	T operator++(int) { T r = get(); set(r+1); return r; }
	T operator--(int) { T r = get(); set(r-1); return r; }
	intwrap<U,T>& operator++() { set(get()+1); return *this; }
	intwrap<U,T>& operator--() { set(get()-1); return *this; }
	intwrap<U,T>& operator  =(const T i) { set(        i); return *this; }
	intwrap<U,T>& operator +=(const T i) { set(get() + i); return *this; }
	intwrap<U,T>& operator -=(const T i) { set(get() - i); return *this; }
	intwrap<U,T>& operator *=(const T i) { set(get() * i); return *this; }
	intwrap<U,T>& operator /=(const T i) { set(get() / i); return *this; }
	intwrap<U,T>& operator %=(const T i) { set(get() % i); return *this; }
	intwrap<U,T>& operator &=(const T i) { set(get() & i); return *this; }
	intwrap<U,T>& operator |=(const T i) { set(get() | i); return *this; }
	intwrap<U,T>& operator ^=(const T i) { set(get() ^ i); return *this; }
	intwrap<U,T>& operator<<=(const T i) { set(get()<< i); return *this; }
	intwrap<U,T>& operator>>=(const T i) { set(get()>> i); return *this; }
	
	intwrap() {}
	intwrap(T i) { set(i); }
	template<typename T1> intwrap(T1 v1) : U(v1) {}
	template<typename T1, typename T2> intwrap(T1 v1, T2 v2) : U(v1, v2) {}
	template<typename T1, typename T2, typename T3> intwrap(T1 v1, T2 v2, T3 v3) : U(v1, v2, v3) {}
};
