#ifndef MAR_JNIUTIL_H
#define MAR_JNIUTIL_H

#include <jni.h>
#include <vector>
#include <utility>

struct JniParameters
{

   JNIEnv* env;
   jclass klass;
   jobject obj;
   std::vector<std::pair<jmethodID, std::string>> methods;

   JniParameters() : env(nullptr), klass(nullptr), obj(nullptr) {}
   JniParameters(JNIEnv* env, jobject obj) : env(env), klass(nullptr), obj(obj) {}
   JniParameters(JNIEnv* env, jclass klass, jobject obj) : env(env), klass(klass), obj(obj) {}
};

#endif //MAR_JNIUTIL_H
