//This is <http://www.codeproject.com/Articles/136799/> Lightweight Generic C++ Callbacks (or, Yet Another Delegate Implementation)
//with a number of changes:
//- Callback was renamed to function, and the namespace was removed.
//- BIND_FREE_CB/BIND_MEM_CB were combined to a single bind(), by using the C99 preprocessor's __VA_ARGS__.
//- Instead of the thousand lines of copypasta, the implementations were merged by using some preprocessor macros.
//- The Arity, ReturnType and ParamNType constants/typedefs were removed.
//- NullCallback was removed.
//- BoundCallbackFactory and bind_arg was added, as a compatibility aid for the C++ conversion.

//Alternate libraries that do roughly the same thing:
//http://www.codeproject.com/Articles/7150/ Member Function Pointers and the Fastest Possible C++ Delegates
//http://www.codeproject.com/Articles/11015/ The Impossibly Fast C++ Delegates
//http://www.codeproject.com/Articles/13287/ Fast C++ Delegate
//http://www.codeproject.com/Articles/18886/ A new way to implement Delegate in C++
//http://www.codeproject.com/Articles/136799/ Lightweight Generic C++ Callbacks (or, Yet Another Delegate Implementation)

#ifndef UTIL_CALLBACK_HPP
#define UTIL_CALLBACK_HPP

#define UTIL_CALLBACK_HPP_INSIDE

#define bind(func, ...) (GetCallbackFactory(func).Bind<func>(__VA_ARGS__))
#define bind_arg(func, arg) (GetBoundCallbackFactory(func).Bind<func>(arg))

template<typename FuncSignature> class function;

#define JOIN2(a,b) a##b
#define JOIN(a,b) JOIN2(a,b)

#define FreeCallbackFactory JOIN(MemberCallbackFactory,COUNT)
#define MemberCallbackFactory JOIN(MemberCallbackFactory,COUNT)
#define ConstMemberCallbackFactory JOIN(ConstMemberCallbackFactory,COUNT)
#define BoundCallbackFactory JOIN(BoundCallbackFactory,COUNT)

#define ARG_TYPES_I(n) JOIN(P,n)
#define ARG_TYPES LOOP(ARG_TYPES_I)
#define ARG_TYPES_C COMMA_IF_ARGS ARG_TYPES
#define ARG_NAMES_I(n) JOIN(a,n)
#define ARG_NAMES LOOP(ARG_NAMES_I)
#define ARG_NAMES_C COMMA_IF_ARGS ARG_NAMES
#define ARG_TYPES_AND_NAMES_I(n) JOIN(P,n) JOIN(a,n)
#define ARG_TYPES_AND_NAMES LOOP(ARG_TYPES_AND_NAMES_I)
#define ARG_TYPES_AND_NAMES_C COMMA_IF_ARGS ARG_TYPES_AND_NAMES
#define TYPENAMES_I(n) typename JOIN(P,n)
#define TYPENAMES COMMA_IF_ARGS LOOP(TYPENAMES_I)
#define TYPENAMES2_I(n) typename JOIN(FP,n)
#define TYPENAMES2 COMMA_IF_ARGS LOOP(TYPENAMES2_I)

#define COUNT 0
#define LOOP(macro) /* */
#define COMMA_IF_ARGS /* */
#include "function.h"
#undef COMMA_IF_ARGS

#define COMMA_IF_ARGS ,
#define COUNT 1
#define LOOP(macro) macro(1)
#include "function.h"

#define COUNT 2
#define LOOP(macro) macro(1), macro(2)
#include "function.h"

#define COUNT 3
#define LOOP(macro) macro(1), macro(2), macro(3)
#include "function.h"

#define COUNT 4
#define LOOP(macro) macro(1), macro(2), macro(3), macro(4)
#include "function.h"

#define COUNT 5
#define LOOP(macro) macro(1), macro(2), macro(3), macro(4), macro(5)
#include "function.h"

#define COUNT 6
#define LOOP(macro) macro(1), macro(2), macro(3), macro(4), macro(5), macro(6)
#include "function.h"

#undef JOIN2
#undef JOIN
#undef FreeCallbackFactory
#undef MemberCallbackFactory
#undef ConstMemberCallbackFactory
#undef BoundCallbackFactory
#undef ARG_TYPES_I
#undef ARG_TYPES
#undef ARG_TYPES_C
#undef ARG_NAMES_I
#undef ARG_NAMES
#undef ARG_NAMES_C
#undef ARG_TYPES_AND_NAMES_I
#undef ARG_TYPES_AND_NAMES
#undef ARG_TYPES_AND_NAMES_C
#undef TYPENAMES_I
#undef TYPENAMES
#undef TYPENAMES2_I
#undef TYPENAMES2

#undef UTIL_CALLBACK_HPP_INSIDE

#endif

#ifdef UTIL_CALLBACK_HPP_INSIDE
template<typename R TYPENAMES>
class function<R (ARG_TYPES)>
{
public:
    function()                    : func(0), obj(0) {}
    function(const function& rhs) : func(rhs.func), obj(rhs.obj) {}
    ~function() {} 

    function& operator=(const function& rhs)
        { obj = rhs.obj; func = rhs.func; return *this; }

    inline R operator()(ARG_TYPES_AND_NAMES) const
    {
        return (*func)(obj ARG_NAMES_C);
    }

private:
    typedef const void* function::*SafeBoolType;
public:
    inline operator SafeBoolType() const
        { return func != 0 ? &function::obj : 0; }
    inline bool operator!() const
        { return func == 0; }

private:
    typedef R (*FuncType)(const void* ARG_TYPES_C);
    function(FuncType f, const void* o) : func(f), obj(o) {}

private:
    FuncType func;
    const void* obj;

    template<typename FR TYPENAMES2>
    friend class FreeCallbackFactory;
    template<typename FR, class FT TYPENAMES2>
    friend class MemberCallbackFactory;
    template<typename FR, class FT TYPENAMES2>
    friend class ConstMemberCallbackFactory;
};

template<typename R TYPENAMES>
void operator==(const function<R (ARG_TYPES)>&,
                const function<R (ARG_TYPES)>&);
template<typename R TYPENAMES>
void operator!=(const function<R (ARG_TYPES)>&,
                const function<R (ARG_TYPES)>&);

template<typename R TYPENAMES>
class FreeCallbackFactory
{
private:
    template<R (*Func)(ARG_TYPES)>
    static R Wrapper(const void* ARG_TYPES_AND_NAMES_C)
    {
        return (*Func)(ARG_NAMES);
    }

public:
    template<R (*Func)(ARG_TYPES)>
    inline static function<R (ARG_TYPES)> Bind()
    {
        return function<R (ARG_TYPES)>
            (&FreeCallbackFactory::Wrapper<Func>, 0);
    }
};

template<typename R TYPENAMES>
inline FreeCallbackFactory<R ARG_TYPES_C>
GetCallbackFactory(R (*)(ARG_TYPES))
{
    return FreeCallbackFactory<R ARG_TYPES_C>();
}

template<typename R, class T TYPENAMES>
class MemberCallbackFactory
{
private:
    template<R (T::*Func)(ARG_TYPES)>
    static R Wrapper(const void* o ARG_TYPES_AND_NAMES_C)
    {
        T* obj = const_cast<T*>(static_cast<const T*>(o));
        return (obj->*Func)(ARG_NAMES);
    }

public:
    template<R (T::*Func)(ARG_TYPES)>
    inline static function<R (ARG_TYPES)> Bind(T* o)
    {
        return function<R (ARG_TYPES)>
            (&MemberCallbackFactory::Wrapper<Func>,
            static_cast<const void*>(o));
    }
};

template<typename R, class T TYPENAMES>
inline MemberCallbackFactory<R, T ARG_TYPES_C>
GetCallbackFactory(R (T::*)(ARG_TYPES))
{
    return MemberCallbackFactory<R, T ARG_TYPES_C>();
}

template<typename R, class T TYPENAMES>
class ConstMemberCallbackFactory
{
private:
    template<R (T::*Func)(ARG_TYPES) const>
    static R Wrapper(const void* o ARG_TYPES_AND_NAMES_C)
    {
        const T* obj = static_cast<const T*>(o);
        return (obj->*Func)(ARG_NAMES);
    }

public:
    template<R (T::*Func)(ARG_TYPES) const>
    inline static function<R (ARG_TYPES)> Bind(const T* o)
    {
        return function<R (ARG_TYPES)>
            (&ConstMemberCallbackFactory::Wrapper<Func>,
            static_cast<const void*>(o));
    }
};

template<typename R, class T TYPENAMES>
inline ConstMemberCallbackFactory<R, T ARG_TYPES_C>
GetCallbackFactory(R (T::*)(ARG_TYPES) const)
{
    return ConstMemberCallbackFactory<R, T ARG_TYPES_C>();
}



template<typename R TYPENAMES>
class BoundCallbackFactory
{
private:
    template<R (*Func)(ARG_TYPES)>
    static R Wrapper(const void* o ARG_TYPES_AND_NAMES_C)
    {
        return (*Func)(o ARG_NAMES_C);
    }

public:
    template<R (*Func)(ARG_TYPES)>
    inline static function<R (ARG_TYPES)> Bind(void* ptr)
    {
        return function<R (ARG_TYPES)>
            (&BoundCallbackFactory::Wrapper<Func>, ptr);
    }
};

template<typename R TYPENAMES>
inline BoundCallbackFactory<R ARG_TYPES_C>
GetBoundCallbackFactory(R (*)(ARG_TYPES))
{
    return FreeCallbackFactory<R ARG_TYPES_C>();
}

#undef COUNT
#undef LOOP
#endif
