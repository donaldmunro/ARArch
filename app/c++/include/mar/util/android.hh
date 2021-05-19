#ifndef MAR_UTIL_ANDROID_HH
#define MAR_UTIL_ANDROID_HH

#include <string>
#include <vector>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include <android/log.h>

bool is_asset_dir(AAssetManager* pAssetManager, const char* assetdir);

bool read_asset_string(AAssetManager* pAssetManager, const char* assetname, std::string& s);

bool copy_asset(AAssetManager* pAssetManager, const char* assetname, const char* localfile);

template<typename T>
bool read_asset_vector(AAssetManager* asset_manager, const char* assetname, std::vector<T>& v)
//----------------------------------------------------------------------------------------------------------
{
   if (asset_manager == nullptr)
   {
      __android_log_print(ANDROID_LOG_ERROR, "read_asset_vector()", "Asset manager null reading asset %s", assetname);
      v.clear();
      return false;
   }
   struct D { void operator()(AAsset* p) const { if (p) AAsset_close(p); }; };
   D d;
   AAsset* ass = AAssetManager_open(asset_manager, assetname, AASSET_MODE_BUFFER);
   if (ass == nullptr)
   {
      if (assetname[0] == '/')
         ass = AAssetManager_open(asset_manager, &assetname[1], AASSET_MODE_BUFFER);
      if (ass == nullptr)
      {
         __android_log_print(ANDROID_LOG_ERROR, "read_asset_vector()", "Error opening asset %s", assetname);
         v.clear();
         return false;
      }
   }
   std::unique_ptr<AAsset, D> asset_p(ass, d);
//   std::unique_ptr<AAsset, D> asset_p(AAssetManager_open(asset_manager, assetname, AASSET_MODE_BUFFER), d);
   if (! asset_p)
   {
      __android_log_print(ANDROID_LOG_ERROR, "read_asset_vector()", "Error opening asset %s", assetname);
      v.clear();
      return false;
   }
   off_t len = AAsset_getLength(asset_p.get());
   v.resize(len);
   int rlen = AAsset_read(asset_p.get(), v.data(), len);
   if (rlen < len)
   {
      __android_log_print(ANDROID_LOG_ERROR, "read_asset_vector()", ""
                          "Error reading from asset %s (length %d/%zu)", assetname, rlen, len);
      v.clear();
      return false;
   }
   return true;
}
#endif //MAR_ANDROID_HH
