# C++ Concurrency with Lambda and polymorphic Pi-Calculus 

My humble attempt to use the expressiveness of Pi-Calculus and static type checking of C++11.

Please learn Pi-Calculus first, than the next line will start to make sense:

`P = a(x) | a[x] | Q * R | (Q | R) | !Q | v{x} | If(pred).then(Q).else(R) | 0`

### Getting started with C++ version of Pi calculus syntax :

    receive x from a              P = a(x)
    send x to a                   P = a[x]  
    sequence                      P = Q * R, first Q then R
    parallel                      P = Q | R, both Q and R in separated threads
    replicate                     P = !Q, do Q forever
    local                         P = v{x}, make a local x
    decision                      P = If(pred).then(Q).else(R), this is a replacement to Q+R
    define an Input channel       IChannel<>
    define an Output channel      OChannel<>
    define a synchronous channel  Sync<_In, _Out> 

## Overview

Let's start with a producer/consumer process: 

The C++ static type part:

    Queue<int> queue; 
    Var<int> p;
    Var<int> q;
    IChannel<int> producer = [](){ static int index; return ++index; }
    OChannel<int> consumer = [](int x){ std::cout << x << std::endl; }

Then the Pi-Calculus part:

    producer(e)   // get a produced elements into e (forever)
    consumer[e]   // send e to consumer 
    queue[e]      // push e into queue
    queue(e)      // pop e from queue

I want one process to receive the item from `producer` and queue it on `queue`, and another process that get one item from `queue` and send it to the `consumer`.

    P1 = producer(p) * queue[p];
    P2 = queue(c) * consumer[c]; 

Now I want both processes in parallel for ever.

    P = !P1 | !P2;

### The code in C++

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
      IChannel<int> producer([](){ 
          static int index; 
          std::cout << "Gen " << ++index << std::endl; 
          return index; } );
      OChannel<int> consumer([](int x){std::cout << "Con " << x << std::endl; });

      // nothing will run, here just preparing
      Process P1 = producer(p) * queue[p];
      Process P2 = queue(c) * consumer[c];
      Process P = !P2 | !P1;

      // now,  goes
      std::thread thread(P);
      thread.join();

      return 0;
    }

## FAQ

### This is a pre-processing tool ? Do I need other libraries, like boost ? 
- No, just plain standard C++11. However boost is full of amazing useful features.

### Installation
- Just copy files in your system.

### Do you use this in production code ?
- No, I use a far more complex code then this one, both derived fro my initiated master degree in computer science. This is only an implementation of the syntax itself. 

### This example can be terser ?
Yes, you can create a process with a more direct syntax.

       IChannel<int> producer = [](){ static int index; return ++index; }
       OChannel<int> consumer = [](int x){ std::cout << x << std::endl; }
       Process P1 = producer(p) * queue[p];
       Process P2 = queue(c) * consumer[c]; 
       thread t(Process(!P1 | !P2));
       t.join();

### More terse ?
Yes, you can create a process with an even more direct syntax.

       IChannel<int> producer = [](){ static int index; return ++index; }
       OChannel<int> consumer = [](int x){ std::cout << x << std::endl; }
       std::thread(
          Process( !(producer(p) * queue[p]) | !(queue(c) * consumer[c]) )
       ).join();

### Terser ?
Yes, you can inline lambdas directly in the syntax.

       std::thread(
          Process( !(IChannel([](){ static int index; return ++index; })(p) * queue[p]) |
                   !(queue(c) * OChannel([](int x){ std::cout << x << std::endl; })[c]) )
       ).join();

### Ter...?
Sure not !! Comon !!!!! No way...!!! This is already a four liner... 

No ! Wait !

       Process( !(IChannel([](){ static int index; return ++index; })(p) * queue[p]) |
                !(queue(c) * OChannel([](int x){ std::cout << x << std::endl; })[c]) ).go();


### How do I make my class a channel ?

- Don't inherit from Channels, this is sooooooo 90's.
- You must implement some methods in your class that receive some `pi::Var<X>` parameters and returns a `pi::Process` object.
- If you use `operator()` and `operator[]` syntax and Process(In<>) or Process(Out<>) your code will receive my seal of approval.

note: please see `Queue<>::operator()` and `Queue<>::operator[]` reference implementation

### Parallel x Sequence FAQ

- `B|C` == `C|B`.
- `B|C|D` == `B|D|C` == `C|B|D` == `C|D|B` == `D|B|C` == `D|C|B` == `(B|C|D)` == `(B|C)|D` == `B|(C|D)` and other permutations and associations.
- `(B|C)|(D|E)` == `(B|C|D|E)`.
- `(B|C)*(D|E)` != `(B|C*D|E)` because C++ operator precedence rules, the first case executes B and C in parallel threads, wait for all of them to join, then starts both D and E in separated threads. The second case executes B in parallel with C*E in parallel with E.
- `A * (B|C|D|E) * F` execute A, than after A ends, starts B, C, D and E in separated threads, then after all B, C, D and E joins, then execute F.

### Var< T > FAQ

- You can safely use vars in processes and drop their original scope, they still **alive** when referenced from the process.

  **Ok** sample
  
      class QueuedObj {
          Queue<int> queue_;
      public:
          Process build() {
             Var<int> x;
             Process P = queue_(x);
             return P;
          }
      }

You can use vars as channels.

      Var<int> v;
      Var<int> w;
      Queue<int> queue;
      Process P = w[10] * v[w] * queue[v] * v(w);

   * The process P sets w = 10, then sets v = w, then send v to queue, finally set w = v 
 
   * `v[x]` sends x to v, same as attribution of value x to v 

   * `v(x)` sends v to x, same as attribution of value v to x 

You can pass channels thru channels, but you must be **REALLY** careful with this, it's necessary for polymorphic pi-calculus. The rules are the same for other POD types, so when you make a var-channel local, `v{x}` apply same rules of any POD (make a local copy). Normally it's not what you want, you must implement a channel handler class that shares the true channel. I think, the best way to deal with this is creating a Var<T&> or Var<std::reference_wrapper> specialization ( some day I'll do this ), but for now:

      Var<Event> event;
      Queue<Event> event_queue;
      Var<IChannel<Event>> handler;
      Queue<IChannel<Event>> queue_handler;
      // receive a handler, a event and send the event to it;
      Process P = queue_handler(handler) * event_queue(event) * handler[event];


### v{x} FAQ

- It is not renaming, it´s only making a local copy of x to the rest of the process. It's not possible make a new name in C++ RValues.
- It works only with vars, not with channels, it means that the process **maintains alive** the vars after the declarations goes out of scope but not the Channels;

**Problem** example

      Process build() {
          Queue<int> q;
          Var<int> x;
          Process P = v{x} * q(x);  // problem: reference q local
          return P;
      }

**Ok** sample

      // This is ok as far as queue q does not be destroyed during execution of P.
      Process build(Queue<int>& q) {
          Var<int> x;
          Process P = v{x} * q(x); // ok P maintains a copy of x and q is not local
          return P;
      }

  
- The above sample will works with only one var;

  In P1, `producer` and `queue` shares the same `e`;

  In P2, `queue` and `consumer` shares the same `e`;

  However the `e` in P1 is different than `e` in P2.

       Var<int> e;
       Process P1 = v{e} * producer(e) * queue[e]; 
       Process P2 = v{e} * queue(e) * consumer[e]; 

#### Is it sorcery ?
- Yes, i mean, no ! just plain standard C++11.

  Look into the implementation of Var<T>, you will see a bizzare shered_ptr<shared_ptr<>> thing. 

### Lambdas FAQ
You can create `Process` implicitly with any thing that `std::function<void()>` recognizes, such as lambdas of plain functions or function-objects.

      Process P = a(x) * [](){ do_wathever() };  // parameter-less lambdas... 
      Process Q ([](){ do_wathever() });         //   ...may appear as a Process

Lambdas that returns values may be used as input channels, any thing compatible with `std::function<T()>` can be used.

       IChannel<int> input( [](int x){} );
       
Parameterized lambdas may be used as output channels, any thing compatible with `std::function<void(T)>` can be used; 

      OChannel<int> output( [](){ return int(1); } );

You can wait for a `std::future` in some lambda ?

      std::future<int> fut;
      IChannel<int> rec([fut&](){ return fut.get(); })

Does this brings the same problem of blocking on future out-of-scope ? 
- yes


### Why not choice (P+Q) ?

- It is possible, but take a lot more work :).

  Each process P under a choice C must acquire an execution lock from direct parent choice C  (which may need to ask it's own for a parent C' choice if that is the case )  when P receive a signal to communication runs, it locks the execution thread from all ancestor choices C, C', etc...

- We have few different types of possible semantics for choice give-up trigger for Q.

  option 1 - at the time that any first communication parallel part of P starts.

  option 2 - at the time that any first communication parallel part P fully finishes.

  option 3 - at the time that whole P fully finishes.

### Did you implemented polyadic ? or bissimulation ?
 - Not yet, but Soon ;-) 

### I have to worry about exceptions?
- Yes.

### What is that `operator^` ?

- Is a syntax sugar for multiple instances of parallel operator, `P = Q ^ 3` means `P = Q | Q | Q`. 

### If i have a `Queue<int> q` and `Var<std::string> v` and via `P = q(v)` try to assign from `q` into `v` does the program compile ?
- No, the library uses C++ static type system.


### Who do I make a "assync-then" idiom ?
That's the sequence operator `P * Q`

      Process P = [](){ do_something(); };
      Process Q = [](){ do_after_something(); }
      Process U = P * Q;  // P then Q

### Can I pass a parameter from P to Q ?
Yes use a Var< T >.

      Var v<int> v;
      Process P = [](){ return do_something(); };
      Process Q = [](int x){ do_after_something(x); }
      Process U = P(v) * Q[v];  // receive v via P then send to Q

Another example

      TCPSocket sock;
      Var<TCPBuffer> buffer;
      Queue<TCPBuffer> queue;
      Process TCPReceive = sock(buffer) * queue[buffer];

### Can I have 100 threads of this ?
Sure, one single instance of TCPReceive accommodate 100 threads;

      Process TCPReceive = ( v{buffer} * sock(buffer) * queue[buffer] ) ^ 100;

If you want that separated:

      Process TCPReceive = ( v{buffer} * sock(buffer) * queue[buffer] );
      Process TCPReceiveGroup = TCPReceive ^ 100;


### What tha hell is v{buffer} ?
- That makes one local copy of `buffer` variable for each of 100 thread separated from the other threads;
- Take a Look at v{x} FAQ

### What if I want that the same buffer be shared among all threads ?
Don't make a local:

      Process TCPReceive = (sock(buffer) * queue[buffer]) ^ 100;

That's all
