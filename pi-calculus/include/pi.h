#ifndef PI_H_
#define PI_H_

#include <memory> 
#include <thread> 
#include <mutex> 
#include <condition_variable> 
#include <vector> 
#include <initializer_list> 
#include <type_traits>

namespace pi {

class Replication {
private:
    std::function<void()> invokable_;
public:
    Replication(const std::function<void()>& invokable);
    void operator()();
};

class Process {
    std::function<void()> invokable_;

public:
    Process() {}
    Process(const int i) : invokable_([](){}) {} // so we can use  0 syntax :)
    Process(const Process& p) = default;
    Process& operator=(const Process& p) = default;
    Process(Process&& p) : invokable_(std::move(p.invokable_)) {}

    template<typename _Invokable>
    Process(const _Invokable& invokable) : invokable_(invokable) {}

    Process(const std::function<void()>& invokable) : invokable_(invokable) {}

    template<typename _Invokable, typename _Object>
    Process(const _Invokable& invokable, _Object &obj) : invokable_(std::bind(invokable, obj)) {}

    void operator()() {
        invokable_();
    }

    void go() {
        std::thread(*this).join();
    }

    Process operator!() {
        return Replication(invokable_);
    }
};

template<typename _Tp>
class Var {
    // we need atomic<shared_ptr<_Tp>> to avoid all locks
    mutable std::recursive_mutex mtx_;
    std::shared_ptr<std::shared_ptr<_Tp>> ref_;

    template<typename _U> void send_to(Var<_U>& u) { u = *this; }
    template<typename _U> void receive_from(const Var<_U>& u) { *this = u; }

    template<typename T>
    struct has_function_call_op
    {
        template<typename U, Process(U::*)(Var<T>&)> struct SFINAE {};
        template<typename U> static char Test(SFINAE<U, &U::operator()>*);
        template<typename U> static int Test(...);
        static const bool value = sizeof(Test<T>(0)) == sizeof(char);
    };

    template<typename T>
    struct has_array_op
    {
        template<typename U, Process(U::*)(Var<T>&)> struct SFINAE {};
        template<typename U> static char Test(SFINAE<U, &U::operator[]>*);
        template<typename U> static int Test(...);
        static const bool value = sizeof(Test<T>(0)) == sizeof(char);
    };

public:
    Var() : Var(_Tp()) {}
    Var(const Var& var) : ref_( var.ref_ ) {}
    Var(const _Tp& val) {
        ref_.reset(new std::shared_ptr<_Tp>());
        ref_->reset(new _Tp(val));
    }

          _Tp& operator*() { std::lock_guard<std::recursive_mutex> lck(mtx_); return *(*ref_); }
    const _Tp& operator*() const { std::lock_guard<std::recursive_mutex> lck(mtx_); return *(*ref_); }

          _Tp* operator->() { std::lock_guard<std::recursive_mutex> lck(mtx_); return (*ref_).get(); }
    const _Tp* operator->() const { std::lock_guard<std::recursive_mutex> lck(mtx_); return (*ref_).get(); }

    _Tp& operator=(const _Tp& val) {
        std::lock_guard<std::recursive_mutex> lck(mtx_);
        *(*this) = val;
        return *(*this);
    }

    template<class _U>
    Var<_Tp>& operator=(const Var<_U>& val) {
        std::lock_guard<std::recursive_mutex> lck(mtx_);
        *(*this) = *val;
        return *this;
    }
    void make_local_construct_process() {
        std::lock_guard<std::recursive_mutex> lck(mtx_);
        ref_.reset(new std::shared_ptr<_Tp>());
        ref_->reset(new _Tp());
    }

    void make_local_exec_process() {
        std::lock_guard<std::recursive_mutex> lck(mtx_);
        ref_->reset(new _Tp());
    }

    //template<typename ..._OtherTp>
    //std::tuple<_Tp, Var<_OtherTp>...> operator,(const std::tuple<Var<_OtherTp>...>& tuple) {
    //    return tuple_cat(std::make_tuple(*this), tuple);
    //}

    //template<typename _OtherTp>
    //std::tuple<_Tp, _OtherTp> operator,(const Var<_OtherTp>& other) {
    //    return std::make_tuple(*this, other);
    //}

    template<typename _U>
    Process operator()(const Var<_U>& var) {
        struct has_op {
            static Process build_process(Var<_Tp>& var, const Var<_U>& parm) {
                return Process([&](){ var(parm); });
            }
        };
        struct not_has_op {
            static Process build_process(Var<_Tp>& var, const Var<_U>& parm) {
                return Process([&](){ *var = *parm; });
            }
        };

        return typename std::conditional_t<has_function_call_op<_Tp>::value, has_op, not_has_op>::build_process(*this, var);
    }

    template<typename _U>
    Process operator()(const _U& val) {
        return Process([&](){ *this = val; });
    }

    template<typename _U>
    Process operator[](Var<_U>& var) {
        struct has_op {
            static Process build_process(Var<_Tp>& var, Var<_U>& parm) {
                return Process([&](){ var[parm]; });
            }
        };
        struct not_has_op {
            static Process build_process(Var<_Tp>& var, Var<_U>& parm) {
                return Process([&](){ parm = var; });
            }
        };

        return typename std::conditional_t<has_array_op<_Tp>::value, has_op, not_has_op>::build_process(*this, var);
    }

    template<typename _U>
    Process operator[](Var<_U>& var) const {
        struct has_op {
            static Process build_process(const Var<_Tp>& var, Var<_U>& parm) {
                return Process([&](){ var[parm]; });
            }
        };
        struct not_has_op {
            static Process build_process(const Var<_Tp>& var, Var<_U>& parm) {
                return Process([&](){ parm = var; });
            }
        };

        return typename std::conditional_t<has_array_op<_Tp>::value, has_op, not_has_op>::build_process(*this, var);
    }

    template<typename _U>
    Process operator[](_U& val) {
        return Process([&](){ val = *(*this); });
    }

    template<typename _U>
    Process operator[](_U& val) const {
        return Process([&](){ val = *(*this); });
    }
};

template<>
class Var<void> {
public:
    Var() {}
    void make_local_construct_process() {}
    void make_local_exec_process() {}
};

class v {

    class VarWrapperRef {
        struct Base {
            virtual void make_local_construct_process() = 0;
            virtual void make_local_exec_process() = 0;
        };

        template<typename _Tp>
        class Impl : public Base {
            Var<_Tp> var_;
        public:
            Impl(const Var<_Tp>& var) : var_(var) {}
            void make_local_construct_process() { var_.make_local_construct_process(); }
            void make_local_exec_process() { var_.make_local_exec_process(); }
        };

        std::shared_ptr<Base> pimpl_;
    public:
        template<typename _Tp>
        VarWrapperRef(const Var<_Tp>& var) : pimpl_(std::make_shared<Impl<_Tp>>(var)) {}

        VarWrapperRef() = default;
        VarWrapperRef(const VarWrapperRef& ref) = default;
        VarWrapperRef& operator=(const VarWrapperRef&) = default;

        void make_local_construct_process() { pimpl_->make_local_construct_process(); }
        void make_local_exec_process() { pimpl_->make_local_exec_process(); }
    };

    std::vector<VarWrapperRef> vars_;
public:
    v(const std::initializer_list<VarWrapperRef>& vars);
    void operator()();
};

class Stop {
public:
    void operator()();
};

class Sequence {
    std::vector<Process> processes_;
public:
    Sequence();
    Sequence(const std::initializer_list<Process>& p);
    Sequence& operator*(const Process& p);
    void operator()();
};

class Parallel {
    std::vector<Process> processes_;
public:
    Parallel( const std::initializer_list<Process>& p );
    Parallel( Process p, int num );
    Parallel& operator|(const Process& p);
    Parallel& operator|(const Parallel& p);
    void operator()();
};

class Choice {
// not yet :)
};

class If {
    class ElseT {
    protected:
        Process then_;
        Process else_;
        std::function<bool()> pred_;

    public:
        ElseT(const std::function<bool()>& pred, const Process& then, const Process& else_process);
        void operator()();
    };

    class ThenT {
        Process then_;
        std::function<bool()> pred_;
    public:
        ThenT(const std::function<bool()>& pred, const Process& then);
        ElseT Else(const Process& p);
        void operator()();
    };

    std::function<bool()> pred_;
public:
    If(const std::function<bool()>& pred);
    ThenT Then(const Process& p);
};

Sequence operator*(const pi::v& p1, const Process& p2);
Sequence operator*(const Process& p1, const pi::v& p2);
Sequence operator*(const Process& p1, const Process& p2);
Parallel operator|(const Process& p1, const Process& p2);
Parallel operator^(const Process& p1, int num);

template<typename _Tp>
class In {
    std::function<_Tp()> invokable_;
    Var<_Tp> var_;
public:
    In(const std::function<_Tp()>& invokable, Var<_Tp>& var) : invokable_(invokable), var_(var) {}

    template<typename _Invokable, typename _Object>
    In(_Invokable invokable, _Object &obj, Var<_Tp> var) :
        invokable_(std::bind(invokable, &obj)), var_(var) {}

    void operator()() {
        var_ = invokable_();
    }
};

template<typename _Tp>
class IChannel {
    std::function<_Tp()> invokable_;
public:
    IChannel(const std::function<_Tp()>& invokable) : invokable_(invokable) {}

    template<typename _U>
    Process operator()(Var<_U>& var) {
        return Process( In<_Tp>(invokable_, var) );
    }
};

template<typename _Tp>
class Out {
    std::function<void(const _Tp&)> invokable_;
    Var<_Tp> var_;
public:
    Out(const std::function<void(const _Tp&)>& invokable, const Var<_Tp>& var) : invokable_(invokable), var_(var) {}

    template<typename _Invokable, typename _Object>
    Out(_Invokable invokable, _Object &obj, const Var<_Tp>& var) :
        invokable_(std::bind(invokable, &obj, std::placeholders::_1)), var_(var) {}

    void operator()() {
        invokable_(*var_);
    }
};

template<typename _Tp>
class OChannel {
    std::function<void(const _Tp&)> invokable_;
public:
    OChannel(const std::function<void(const _Tp&)>& invokable) : invokable_(invokable) {}

    template<typename _U>
    Process operator[](Var<_U>& var) {
        return Process(Out<_Tp>(invokable_, var));
    }
};

template<typename _In, typename _Out = _In>
class Sync {
    std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;

    _In pop() {
        std::unique_lock<std::mutex> mlock(mutex_);
        while (!element_) {
            not_empty_.wait(mlock);
        }
        _In result = invokable_(*element_);
        element_ = nullptr;
        //mlock.unlock();
        not_full_.notify_one();
        return result;
    }

    void push(const _Out& item) {
        std::unique_lock<std::mutex> mlock(mutex_);
        while (element_) {
            not_full_.wait(mlock);
        }
        element_ = &item;
        //mlock.unlock();
        not_empty_.notify_one();
    }

    std::function< _In(const _Out& input)> invokable_;

    const _Out* element_ = nullptr;
public:
    template<typename _Invokable>
    Sync(const _Invokable& invokable) : invokable_(invokable) {}

    Sync() : invokable_([](const _Out& input) -> _In { return input; }) {}

    pi::Process operator()(pi::Var<_In>& var)       { return pi::Process(In<_In>(&Sync<_In, _Out>::pop, *this, var)); }
    pi::Process operator[](const pi::Var<_Out>& var) { return pi::Process(Out<_Out>(&Sync<_In, _Out>::push, *this, var)); }
};

} /* namespace pi */
#endif

