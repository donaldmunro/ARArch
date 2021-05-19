/**
From C++ Concurrency in Action Anthony Williams ISBN 9781933988771 https://www.manning.com/books/c-plus-plus-concurrency-in-action

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
 */
#ifndef _LOCK_FREE_STACK_H_
#define _LOCK_FREE_STACK_H_

#include <atomic>
#include <memory>

template<typename T>
class LockFreeStackShared
{
private:
    struct node;
    struct counted_node_ptr
    {
        int external_count;
        node* ptr;
    };
    struct node
    {
        std::shared_ptr<T> data;
        std::atomic<int> internal_count;
        counted_node_ptr next;
        node(T const& data_):
           data(std::make_shared<T>(data_)),
           internal_count(0)
        {}
    };
    std::atomic<counted_node_ptr> head;
    void increase_head_count(counted_node_ptr& old_counter)
    {
       counted_node_ptr new_counter;
       do
       {
          new_counter=old_counter;
          ++new_counter.external_count;
       }
       while(!head.compare_exchange_strong(
          old_counter,new_counter,
          std::memory_order_acquire,
          std::memory_order_relaxed));
       old_counter.external_count=new_counter.external_count;
    }
public:
    ~lock_free_stack()
    {
       while(pop());
    }
    void push(T const& data)
    {
       counted_node_ptr new_node;
       new_node.ptr=new node(data);
       new_node.external_count=1;
       new_node.ptr->next=head.load(std::memory_order_relaxed);
       while(!head.compare_exchange_weak(
          new_node.ptr->next,new_node,
          std::memory_order_release,
          std::memory_order_relaxed));
    }
    std::shared_ptr<T> pop()
    {
       counted_node_ptr old_head=
          head.load(std::memory_order_relaxed);
       for(;;)
       {
          increase_head_count(old_head);
          node* const ptr=old_head.ptr;
          if(!ptr)
          {
             return std::shared_ptr<T>();
          }
          if(head.compare_exchange_strong(
             old_head,ptr->next,std::memory_order_relaxed))
          {
             std::shared_ptr<T> res;
             res.swap(ptr->data);
             int const count_increase=old_head.external_count-2;
             if(ptr->internal_count.fetch_add(
                count_increase,std::memory_order_release)==-count_increase)
             {
                delete ptr;
             }
             return res;
          }
          else if(ptr->internal_count.fetch_add(
             -1,std::memory_order_relaxed)==1)
          {
             ptr->internal_count.load(std::memory_order_acquire);
             delete ptr;
          }
       }
    }
};
}
#endif
namespace toMAR
{
