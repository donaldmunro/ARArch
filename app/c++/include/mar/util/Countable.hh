#ifndef _MAR_COUNTABLE_H
#define _MAR_COUNTABLE_H

#include <atomic>

//Usage: class Foo : public Bar, public Countable<Foo>
// .. assert(instances() == x);
template <class Obj>
class Countable
{
public:
   Countable() { count.fetch_add(1); }
   Countable(const Countable& obj) { if (this != &obj) count.fetch_add(1); }
   ~Countable() { count.fetch_add(-1); }

   static int instances() { return count.load(); }

private:
   inline static std::atomic_int count{0};
};
#endif