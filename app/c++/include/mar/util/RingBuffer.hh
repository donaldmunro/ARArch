#ifndef POSTPROCESSOR_RINGBUFFER_H
#define POSTPROCESSOR_RINGBUFFER_H

#include <vector>
#include <mutex>

namespace toMAR
{
   namespace util
   {
      template <typename T>
      class RingBuffer
      //==============
      {
      protected:
         std::vector<T> content;
         volatile int head =0, tail =0, length =0;
         int capacity =-1;
         std::mutex mtx;

         inline int indexIncrement(int i) { return (++i >= capacity) ? 0 : i; }
         inline int indexDecrement(int i) { return (0 == i) ? (length - 1) : (i - 1);  }

      public:
         RingBuffer() = delete;
         RingBuffer(int _capacity) : capacity(_capacity) { content.resize(static_cast<unsigned long>(_capacity)); };
         RingBuffer(RingBuffer& other) = default;
         RingBuffer(RingBuffer&& other) = default;

         void clear() { head = tail = length = 0; }
         bool empty() { return (length == 0); }
         bool full() { return (length >= capacity); }
         int size() { return length; }

         int push(T &data)
         //---------------
         {
            if (length >= capacity)
            {
               tail = indexIncrement(tail);
               length--;
            }
            content[head] = data;
            head = indexIncrement(head);
            length++;
            return capacity - length;
         }

         int push_mt(T &data)
         //---------------
         {
            std::lock_guard<std::mutex> lock(mtx);
            if (length >= capacity)
            {
               tail = indexIncrement(tail);
               length--;
            }
            content[head] = data;
            head = indexIncrement(head);
            length++;
            return capacity - length;
         }

         bool pop(T& popped)
         //-----------------
         {
            if (length > 0)
            {
               popped = content[tail];
               tail = indexIncrement(tail);
               length--;
               return true;
            }
            return false;
         }

         bool peek(T& item)
         //----------------
         {
            if (length > 0)
            {
               item = content[tail];
               tail = indexIncrement(tail);
               return true;
            }
            return false;
         }

         bool pop_mt(T& popped)
         //-----------------
         {
            std::lock_guard<std::mutex> lock(mtx);
            if (length > 0)
            {
               popped = content[tail];
               tail = indexIncrement(tail);
               length--;
               return true;
            }
            return false;
         }

         bool peek_mt(T& item)
         //-------------------
         {
            std::lock_guard<std::mutex> lock(mtx);
            if (length > 0)
            {
               item = content[tail];
               tail = indexIncrement(tail);
               return true;
            }
            return false;
         }

         int peekList(std::vector<T>& contents)
         //--------------------------------------------------
         {
            contents.clear();
            int c = 0;
            if (length > 0)
            {
               int len = length;
               int t = tail;
               while (len > 0)
               {
                  contents.push_back(content[t]);
                  t = indexIncrement(t);
                  len--;
                  c++;
               }
            }
            return c;
         }

         int peekList_mt(std::vector<T>& contents)
         //--------------------------------------------------
         {
            contents.clear();
            int c = 0;
            if (length > 0)
            {
               std::lock_guard<std::mutex> lock(mtx);
               int len = length;
               int t = tail;
               while (len > 0)
               {
                  contents.push_back(content[t]);
                  t = indexIncrement(t);
                  len--;
                  c++;
               }
            }
            return c;
         }
      };
   }
}
#endif //POSTPROCESSOR_RINGBUFFER_H
