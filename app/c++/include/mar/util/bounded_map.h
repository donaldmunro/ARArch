#ifndef _BOUNDED_MAP_H
#define _BOUNDED_MAP_H

#include <iostream>
#include <map>
#include <vector>
#include  <utility>

/**
 * A wrapped std::multimap which limits size by removing the smallest keys first.
 * Useful for timestamped datawhere the oldest items (smallest timestamps) can be removed.
 * Example:
 * bounded_map<uint_64t, DataClass*> m(500, std::numeric_limits<uint_64t>::min());
 * m.insert(std::make_pair<uint_64t, DataClass*>(1, pD));
 **/
template<typename K, typename V>
class bounded_map
//================
{
public:
   bounded_map(size_t max_size, K minkey) : max_size(max_size), minkey(minkey) {}

   typename std::multimap<K,V>::iterator insert(const std::pair<K, V>& value )
   {
      auto it = m.insert(value);
      if (m.size() > max_size)
         prune();
      return it;
   }

   template< class I> void insert( I first, I last )
   {
      auto it = m.insert(first, last);
      if (m.size() > max_size)
         prune();
      return it;
   }

   typename std::multimap<K,V>::iterator lower_bound(const K& key) { return m.lower_bound(key); }

   typename std::multimap<K,V>::iterator upper_bound(const K& key) { return m.upper_bound(key); }

   typename std::multimap<K,V>::iterator begin() { return m.begin(); }

   typename std::multimap<K,V>::iterator end() { return m.end(); }

   size_t size() { return m.size(); }

   // Convenience methods
   size_t between(const K& starting, const K& ending, std::multimap<K, V>& results)
   {
      auto it1 = m.lower_bound(starting);
      auto it2 = m.upper_bound(ending);
      size_t c = 0;
      if (it1 != m.end())
      {
         for (; it1 != it2; ++it1)
         {
            results.insert(*it1);
            c++;
         }
      }
      return c;
   }

   size_t all(std::multimap<K, V>& results)
   {
      size_t c = 0;
      for (auto it=m.begin(); it != m.end(); ++it)
      {
         results.insert(*it);
         c++;
      }
      return c;
   }

   size_t from(const K& starting, std::multimap<K, V>& results)
   {
      size_t c = 0;
      for (auto it = m.lower_bound(starting); it != m.end(); ++it)
      {
         results.insert(*it);
         c++;
      }
      return c;
   }

   const std::multimap<K,V>& map() const { return m; }

private:
   size_t max_size;
   std::multimap<K, V> m;
   K minkey;

   void prune()
   //----------
   {
      for (auto it = m.lower_bound(minkey); it != m.end(); ++it)
      {
         if (m.size() > max_size)
            it = m.erase(it);
         else
            break;
      }
   }
};

#endif
