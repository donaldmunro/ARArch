#include "mar/acquisition/FrameInfo.h"

namespace toMAR
{
   unsigned char *FrameInfo::getColorData(void *&context)
   //------------------------------------------
   {
      JNIEnv *env;
      if (getEnv(env))
      {
         void *p = env->GetPrimitiveArrayCritical(rgba, 0);
         context = static_cast<void *>(env);
         rgbaAcquires.fetch_add(1);
         return static_cast<unsigned char *>(p);
      }
      return nullptr;
   }

   void FrameInfo::releaseColorData(void *context, unsigned char *p)
   //----------------------------------
   {
      JNIEnv *env = static_cast<JNIEnv *>(context);
      if (env == nullptr)
      {
         if (! getEnv(env))
         {
            __android_log_print(ANDROID_LOG_ERROR, "FrameInfo::releaseColorData",
                                "Context was null and could not obtain env");
            return;
         }
      }
      env->ReleasePrimitiveArrayCritical(rgba, p, 0);
      jthrowable exc = env->ExceptionOccurred();
      if (exc != nullptr)
         __android_log_print(ANDROID_LOG_ERROR, "FrameInfo::releaseColorData",
                             "Exception releasing color array");
      rgbaAcquires.fetch_add(-1);
   }

   unsigned char *FrameInfo::getMonoData(void *&context)
   //------------------------------------------
   {
      JNIEnv *env;
      if (getEnv(env))
      {
         void *p = env->GetPrimitiveArrayCritical(mono, 0);
         context = static_cast<void *>(env);
         monoAcquires.fetch_add(1);
         return static_cast<unsigned char *>(p);
      }
      return nullptr;
   }

   void FrameInfo::releaseMonoData(void *context, unsigned char *p)
   //---------------------------------------------------
   {
      JNIEnv *env = static_cast<JNIEnv *>(context);
      if (env == nullptr)
      {
         if (! getEnv(env))
         {
            __android_log_print(ANDROID_LOG_ERROR, "FrameInfo::releaseMonoData",
                                "Context was null and could not obtain env");
            return;
         }
      }
      env->ReleasePrimitiveArrayCritical(mono, p, 0);
      jthrowable exc = env->ExceptionOccurred();
      if (exc != nullptr)
         __android_log_print(ANDROID_LOG_ERROR, "FrameInfo::releaseMonoData",
               "Exception releasing mono array");
      monoAcquires.fetch_add(-1);
   }

   void FrameInfo::dispose()
   //-----------------------
   {
      // __android_log_print(ANDROID_LOG_INFO, "FrameInfo::dispose()", "Disposing camera %lu seq %lu", camera_id, seqno);
      JNIEnv *env;
      if (getEnv(env))
      {
         int expected = 0;
         if (! rgbaAcquires.compare_exchange_strong(expected, 0))
            __android_log_print(ANDROID_LOG_ERROR, "FrameInfo::dispose",
                                "rgba usage count %d when attempting to release camera %lu frame %lu. Java VM may ABEND.",
                                expected, camera_id, seqno);
         env->DeleteGlobalRef(rgba);
         if (mono)
         {
            expected = 0;
            if (! monoAcquires.compare_exchange_strong(expected, 0))
               __android_log_print(ANDROID_LOG_ERROR, "FrameInfo::dispose",
                                   "mono usage count %d when attempting to release camera %lu frame %lu. Java VM may ABEND.",
                                   expected, camera_id, seqno);
            env->DeleteGlobalRef(mono);
         }
      }
   }

   bool FrameInfo::getEnv(JNIEnv *&env)
   //---------------------------------
   {
      env = nullptr;
      if (vm == nullptr)
      {
         __android_log_print(ANDROID_LOG_ERROR, "jni::getEnv", "Call to getEnv before vm initialized");
         return false;
      }

      int status = vm->GetEnv((void **)&env, JNI_VERSION_1_6);
      if (status == JNI_OK)
         return true;
      else
      {
         if (status == JNI_EDETACHED)
         {
            __android_log_print(ANDROID_LOG_WARN, "jni::getEnv", "Thread not attached. Attempting to attach");
            if (vm->AttachCurrentThread(&env, NULL) != 0)
               __android_log_print(ANDROID_LOG_ERROR, "jni::getEnv", "Error attaching thread");
            else
               return true;
         } else if (status == JNI_EVERSION)
            __android_log_print(ANDROID_LOG_ERROR, "jni::getEnv", "Java version not supported");
      }
      return false;
   }
};
