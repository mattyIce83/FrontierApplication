/*
 ***********************************************************************************************************************
 *
 * (c) COPYRIGHT, 2023 USA Firmware Corporation
 *
 * All rights reserved. This file is the intellectual property of USA Firmware Corporation and it may not be disclosed
 * to others or used for any purposes without the written consent of USA Firmware Corporation.
 *
 ***********************************************************************************************************************
 */

/**
 ***********************************************************************************************************************
 *
 * @brief   A template class for a thread safe queue, based on std::array.
 * @file    safe_queue.h
 * @author  Roger Chaplin
 *
 ***********************************************************************************************************************
 */
// TODO: Take out section headers of sections that are not used when finished developing this module.


/*
 ***********************************************************************************************************************
 *                                                      MODULE
 ***********************************************************************************************************************
 */
#pragma once


/*
 ***********************************************************************************************************************
 *                                                   INCLUDE FILES
 ***********************************************************************************************************************
 */
/****************************************************** System ********************************************************/
#include <array>
#include <chrono>
#include <mutex>
#include <condition_variable>

/*****************************************************   User   *******************************************************/


/*
 ***********************************************************************************************************************
 *                                                  Class Definition
 ***********************************************************************************************************************
 */
template <typename T, int N> class SafeQueue
{
public:
   SafeQueue()
   : capacity(N),
     front(-1),
     rear(-1)
   {
   }

   virtual ~SafeQueue()
   {
   }

   /**
    * @brief Put an element into the queue with no timeout.
    *
    * If the queue is full, this method returns immediately with an error indication.
    *
    * @param[in] elem The element to put into the queue.
    *
    * @retval 0 The put completed successfully.
    * @retval -ENOMEM The queue was full, the put failed.
    */
   int put(const T &elem)
   {
      int retval = 0;

      {
         std::lock_guard<std::mutex> lg(queue_mutex);
         if (is_full())
         {
            retval = -ENOMEM;
         }
         else
         {
            enqueue(elem);
            queue_cv.notify_all();
         }
      }

      return retval;
   }

   /**
    * @brief Force an element to the front of the queue.
    *
    * If the queue is empty, this simply enqueues the element since there is no front element in the queue
    * to overwrite. If the queue is not empty, write elem over top of the front element (i.e., the next one
    * to be returned by get()).
    *
    * @param[in] elem The element to be forced into the queue.
    *
    * @return 0 (this method has no failure scenario).
    */
   int shove_front(const T &elem)
   {
      int retval = 0;

      {
         std::lock_guard<std::mutex> lg(queue_mutex);
         if (is_empty())
         {
            enqueue(elem);
         }
         else
         {
            the_array[front] = elem;
         }

         queue_cv.notify_all();
      }

      return retval;
   }

   /**
    * @brief Put an element into the queue, with timeout.
    *
    * If there is no room in the queue for the duration of the timeout, this method returns
    * an error indication and does not modify the queue.
    *
    * @param[in] elem The element to put into the queue.
    *
    * @param[in] timeout_ms The timeout in milliseconds. Zero means time out immediately.
    *
    * @retval 0 The put completed successfully.
    * @retval -ETIMEDOUT The queue was full and the timeout expired, the put failed.
    */
   int put(const T &elem, int timeout_ms)
   {
      int retval = 0;

      // Make a deadline (time_point) out of the timeout.
      std::chrono::steady_clock::time_point deadline(std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()) + std::chrono::milliseconds(timeout_ms));

      {
         std::unique_lock<std::mutex> ul(queue_mutex);
         bool has_room = queue_cv.wait_until(ul, deadline, [&]{ return !is_full(); });
         if (!has_room)
         {
            retval = -ETIMEDOUT;
         }
         else
         {
            // Enqueue the element and notify.
            enqueue(elem);
            queue_cv.notify_all();
         }
      }

      return retval;
   }

   /**
    * @brief Get and remove the oldest element from the queue, with deadline.
    *
    * If the queue is empty and remains so until the deadline, this method returns
    * an error indication and does not modify the queue.
    *
    * @param[out] elem A reference to the element to return from the queue.
    *
    * @param[in] deadline The deadline for there to be an element in the queue.
    *
    * @retval 0 The get completed successfully.
    * @retval -ETIMEDOUT The queue was empty and remained so until the deadline arrived, the get failed.
    */
   int get(T &elem, std::chrono::steady_clock::time_point deadline)
   {
      int retval = 0;

      {
         std::unique_lock<std::mutex> ul(queue_mutex);
         bool has_elem = queue_cv.wait_until(ul, deadline, [&]{ return !is_empty(); });
         if (!has_elem)
         {
            retval = -ETIMEDOUT;
         }
         else
         {
            elem = dequeue();
            queue_cv.notify_all();
         }
      }

      return retval;
   }

   /**
    * @brief Get and remove the oldest element from the queue, with timeout.
    *
    * If the queue is empty and remains so until the timeout, this method returns an error
    * indication and does not modify the queue.
    *
    * @param[out] elem A reference to the element to return from the queue.
    *
    * @param[in] timeout_ms The timeout for there to be an element in the queue. Zero means time out immediately.
    *
    * @retval 0 The get completed successfully.
    * @retval -ETIMEDOUT The queue was empty and remained so until the timeout expired, the get failed.
    */
   int get(T &elem, int timeout_ms)
   {
      // Make a deadline (time_point) out of the timeout.
      std::chrono::steady_clock::time_point deadline(std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()) + std::chrono::milliseconds(timeout_ms));

      return get(elem, deadline);
   }

protected:

private:
   /** The queue's capacity, statically set by the template argument N. */
   int capacity;

   /** The array of elements, of type T and size N. */
   std::array<T, N> the_array;

   /** The mutex used to guard queue operations. */
   std::mutex queue_mutex;

   /** The condition variable used to notify waiting threads. */
   std::condition_variable queue_cv;

   /** The index to the front queue element. Elements are written to the front. */
   int front;

   /** The index to the rear queue element. Elements are read from the rear. */
   int rear;

   /**
    * @brief Return whether the queue is empty.
    *
    * @return true The queue is empty.
    * @return false The queue is not empty.
    */
   bool is_empty()
   {
      return -1 == front;
   }

   /**
    * @brief Return whether the queue is full.
    *
    * @return true The queue is full.
    * @return false The queue is not full.
    */
   bool is_full()
   {
      return ((0 == front) && ((capacity - 1) == rear)) || (((front - 1) % (capacity - 1)) == rear);
   }

   /**
    * @brief Add the given element to the queue.
    *
    * @param[in] elem The element to add to the queue.
    *
    * @note This method assumes the queue is not full. Invoking it when the queue is full will corrupt the queue.
    */
   void enqueue(const T &elem)
   {
      if (-1 == front)
      {
         front = 0;
         rear = 0;
      }
      else if ((rear == (capacity - 1) && (0 != front)))
      {
         rear = 0;
      }
      else
      {
         ++rear;
      }

      the_array[rear] = elem;
   }

   /**
    * @brief Remove and return the oldest element from the queue.
    *
    * @return The oldest element from the queue.
    *
    * @note This method assumes the queue is not empty. Invoking it when the queue is empty will corrupt the queue.
    */
   T& dequeue()
   {
      // Fetch the element at front.
      T& retval = the_array[front];

      if (front == rear)
      {
         front = -1;
         rear = -1;
      }
      else if (front == (capacity - 1))
      {
         front = 0;
      }
      else
      {
         ++front;
      }

      return retval;
   }
};


/*********************************************      End of file        ************************************************/
