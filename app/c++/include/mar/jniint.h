#ifndef _JNIINT_H
#define _JNIINT_H

#include <cstdint>

#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>

JavaVM* getVM();
bool getEnv(JNIEnv*& env);
int get_screen_width();
int get_screen_height();
AAssetManager* get_asset_manager();
bool init_class_loader(JNIEnv *env);
jclass find_class_with_classloader(JNIEnv* env, const char* className);
std::string stack_dump(JNIEnv* env, std::string message);
#ifdef QUEUE_STATS
RunningStatistics<uint64_t, long double>& enqueued_stats(int cno);
#endif
#endif
