#include "pi.h"

namespace pi {

v::v(const std::initializer_list<VarWrapperRef>& vars) : vars_(vars) {
    for (auto& var : vars_) {
        var.make_local_construct_process();
    }
};

void v::operator()() {
    for (auto& var : vars_) {
        var.make_local_exec_process();
    }
}

void Stop::operator()() {
}

Sequence::Sequence( const std::initializer_list<Process>& p ) : processes_(p) {
}

void Sequence::operator()() {
    for(auto& p : processes_) {
        p();
    }
}

Sequence& Sequence::operator*(const Process& p) {
    processes_.push_back( p );
    return *this;
}

void Parallel::operator()() {
    std::vector<std::thread> threads_;
    for(auto& p : processes_) {
        threads_.emplace_back(&Process::operator(), p);
    }
    for(auto& t : threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
}

Parallel::Parallel( const std::initializer_list<Process>& p ) : processes_(p) {}

Parallel::Parallel(Process p, int num) : processes_(num, p) {
}

Parallel& Parallel::operator|(const Process& p) {
    processes_.push_back( p );
    return *this;
}

Parallel& Parallel::operator|(const Parallel& p) {
    processes_.insert(processes_.end(), p.processes_.begin(), p.processes_.end());
    processes_.push_back(p);
    return *this;
}

Replication::Replication(const std::function<void()>& invokable) : invokable_(invokable) {
}

void Replication::operator()() { 
    while (1) { 
        invokable_();
    }
}

If::ElseT::ElseT(const std::function<bool()>& pred, const Process& then, const Process& else_process) 
    : pred_(pred), then_(then), else_(else_process) {
}

void If::ElseT::operator()() {
    if (pred_()) {
        then_();
    } else {
        else_();
    }
}

void If::ThenT::operator()() {
    if (pred_()) {
        then_();
    }
}

If::ThenT::ThenT(const std::function<bool()>& pred, const Process& then) : then_(then), pred_(pred) {
}

If::ElseT If::ThenT::Else(const Process& p) {
    return ElseT(pred_, then_, p);
}

If::If(const std::function<bool()>& pred) 
    : pred_(pred) {
}

If::ThenT If::Then(const Process& p) {
    return ThenT(pred_, p);
}

Sequence operator*(const pi::v&, const Process& p2) {
    return Sequence({ p2 });
}

Sequence operator*(const Process& p1, const pi::v&)  {
    return Sequence({ p1 });
}

Sequence operator*(const Process& p1, const Process& p2) {
    return Sequence( {p1, p2} );
}

Parallel operator|(const Process& p1, const Process& p2 ) {
    return Parallel( {p1, p2} );
}

Parallel operator^(const Process& p1, int num) {
    return Parallel(p1, num);
}

} /* namespace pi */
