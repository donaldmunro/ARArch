#include <memory>
#include <fstream>

#include "mar/util/android.hh"

bool is_asset_dir(AAssetManager* pAssetManager, const char* assetdir)
//---------------------------------------------------------------------------
{
   if (pAssetManager == nullptr)
   {
      __android_log_print(ANDROID_LOG_ERROR, "is_asset_dir", "Asset manager null checking asset dir %s", assetdir);
      return false;
   }
   struct D { void operator()(AAssetDir* p) const { if (p) AAssetDir_close(p); }; };
   D d;
   std::unique_ptr<AAssetDir, D> dir(AAssetManager_openDir(pAssetManager, assetdir), d);
   if (dir)
      return true;
   return false;
}

AAsset* assetopen(AAssetManager* pAssetManager, const char* assetname)
//--------------------------------------------------------------------
{
   if (pAssetManager == nullptr)
   {
      __android_log_print(ANDROID_LOG_ERROR, "jni::assetopen", "Asset manager null reading asset %s", assetname);
      return nullptr;
   }
   AAsset* ass = AAssetManager_open(pAssetManager, assetname, AASSET_MODE_BUFFER);
   if (ass == nullptr)
   {
      if (assetname[0] == '/')
         ass = AAssetManager_open(pAssetManager, &assetname[1], AASSET_MODE_BUFFER);
      if (ass == nullptr)
      {
         __android_log_print(ANDROID_LOG_ERROR, "assetopen()", "Error opening asset %s", assetname);
         return nullptr;
      }
   }
   return ass;
}

bool read_asset_string(AAssetManager* pAssetManager, const char* assetname, std::string& s)
//-----------------------------------------------------------
{
   AAsset* ass = assetopen(pAssetManager, assetname);
   if (ass == nullptr)
   {
      s.clear();
      return false;
   }
   struct D { void operator()(AAsset* p) const { if (p) AAsset_close(p); }; };
   D d;
   std::unique_ptr<AAsset, D> asset_p(ass, d);
   off_t len = AAsset_getLength(asset_p.get());
   std::unique_ptr<char[]> pch(new char[len+1]);
   int rlen = AAsset_read(asset_p.get(), pch.get(), len);
   if (rlen < len)
   {
      __android_log_print(ANDROID_LOG_ERROR, "read_asset_string",
                          "Error reading from asset %s (length %d/%ld)", assetname, rlen, len);
      s.clear();
      return false;
   }
   pch[len] = 0;
   s = static_cast<const char *>(pch.get());
   return true;
}

bool copy_asset(AAssetManager* pAssetManager, const char* assetname, const char* localfile)
//-----------------------------------------------------------------------------------------
{
   AAsset* ass = assetopen(pAssetManager, assetname);
   if (ass == nullptr)
      return false;
   struct D { void operator()(AAsset* p) const { if (p) AAsset_close(p); }; };
   D d;
   std::unique_ptr<AAsset, D> asset_p(ass, d);
   std::ofstream ofs(localfile, std::ios::binary | std::ios::trunc);
   char buf[4096];
   while (AAsset_getRemainingLength(ass) > 0)
   {
      auto read = AAsset_read(ass, buf, 4096);
      if (read > 0)
         ofs.write(buf, read);
      else
         break;
   }
   ofs.close();
   return true;
}
