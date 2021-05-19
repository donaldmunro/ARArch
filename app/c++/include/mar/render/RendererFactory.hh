#ifndef _MAR_RENDERER_FACTORY_H
#define _MAR_RENDERER_FACTORY_H

#ifdef HAS_BULB
#include "mar/render/BulbRenderer.hh"
#include "filament/Engine.h"
#endif
#ifdef HAS_SIMPLE_RENDERER
#include "mar/render/VulkanRenderer.h"
#include "mar/render/ArchVulkanRenderer.h"
#endif

namespace toMAR
{
    class RendererFactory
        //===================
    {
    public:
        static RendererFactory& instance()
        //--------------------------------
        {
           static RendererFactory the_instance;
           return the_instance;
        }

        RendererFactory() = default;
        RendererFactory(RendererFactory const&) = delete;
        RendererFactory(RendererFactory&&) = delete;
        RendererFactory& operator=(RendererFactory const&) = delete;
        RendererFactory& operator=(RendererFactory &&) = delete;

#ifdef HAS_BULB
        BulbRenderer* make_bulb_renderer(void* nativeSurface, const char *assetsDir, int screenWidth, int screenHeight,
                                         int texWidth, int texHeight, double perspectiveFOV, double perspectiveNear,
                                         double perspectiveFar, const char *appName,
                                         filament::Engine::Backend backend = filament::Engine::Backend::VULKAN,
                                         bool isShowFPS =false)
        //------------------------------------------------------------------------------------------------------
        {
           BulbRenderer* renderer = new BulbRenderer(appName, assetsDir, isShowFPS);
           BulbRendererParams* params = new BulbRendererParams { .surface = nativeSurface, .screenWidth = screenWidth,
                                                                 .screenHeight = screenHeight, .texWidth = texWidth,
                                                                 .texHeight = texHeight, .perspectiveFOV = perspectiveFOV,
                                                                 .perspectiveNear = perspectiveNear,
                                                                 .perspectiveFar = perspectiveFar,
                                                                 .backend = backend };
           renderer->set_initialization_parameters(params);
           return renderer;
        }

#endif
#ifdef HAS_SIMPLE_RENDERER
        /**
       * Barebones Vulkan renderer (no scene graph)
       * @param nativeSurface
       * @param nativeConnection
       * @param shaderDir
       * @param width
       * @param height
       * @param appName
       * @param isShowFPS
       * @return
       */
      VulkanRenderer* make_vulkan_renderer(int id, std::string type, void* nativeSurface,
                                           void* nativeConnection, const char *assetsDir,
                                           int width, int height, const char *appName,
                                           bool isAprilTags =false,
                                           FaceRenderType faceRenderType =FaceRenderType::NONE,
                                           bool isShowFPS =false)
      //------------------------------------------------------------------------------------------------------
      {
         VulkanRenderer* renderer = nullptr;
         if (type == "TBB")
         {
            renderer = new ArchVulkanRenderer(id, appName, assetsDir, isAprilTags, faceRenderType, isShowFPS);
            if (! renderer->create(nativeSurface, nativeConnection, width, height, nullptr))
            {
               delete renderer;
               return nullptr;
            }
         }
         return renderer;
      }

      void destroy_vulkan_renderer(VulkanRenderer* renderer)
      //-----------------------------------------------------
      {
         if (renderer != nullptr)
            delete renderer;
      }
#endif
    };
};

#endif
