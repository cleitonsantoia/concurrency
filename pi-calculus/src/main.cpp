#include <iostream>
#include <thread>

#include "queue.h"
#include "pi.h"

using namespace pi;
using namespace util;

int main() {
    Queue<int> queue(100);
    Var<int> p;
    Var<int> c;
    Var<Queue<int>> r;
    IChannel<int> producer([](){ static int index; std::cout << "Gen " << ++index << std::endl; return index; });
    OChannel<int> consumer([](int x){std::cout << "Con " << x << std::endl; });
    Sync<int> sync([](int x){ std::cout << "sync " << x << std::endl; return x; });
    // nothing will run, here just preparing
    Process P1 = producer(p) * queue[p];
    Process P2 = queue(c) * consumer[c];
    Process P = !P2 | !P1;

    // now,  goes
    std::thread thread(P);
    thread.join();

    return 0;
}