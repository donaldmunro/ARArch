#ifndef _VULKAN_RENDERER_H
#define _VULKAN_RENDERER_H

#include "mar/render/Renderer.h"

#include <vulkan/vulkan.h>

#include <memory>

#include "VmaUsage.h"

namespace toMAR
{
   class VulkanRenderer : public Renderer
   //==============================================
   {
   public:

      bool is_single_threaded() override { return false; }
      bool render(uint64_t seqno, unsigned long cameraNo =0) override;

      const char* name() override { return renderer_name; }

      virtual ~VulkanRenderer() { destroy(); delete[] renderer_name; }

   protected:
      VulkanRenderer(const char *appName, const char* assetsDir, bool isShowFPS =false);
      virtual void draw(FrameInfo* frame, void *texture) {}

      std::string shadersAssetsDir;
      const std::string CAMERA_VERTEX_SHADER{"camera.vert.spv"}, CAMERA_FRAGMENT_SHADER{"camera.frag.spv"};
      //cache some of the main loop calls to improve performance (VulkanMemoryAlloc doesn't play nicely with volk as at 02/2019)
      PFN_vkAcquireNextImageKHR fpAcquireNextImageKHR;
      PFN_vkQueuePresentKHR fpQueuePresentKHR;
      PFN_vkWaitForFences fpWaitForFences;
      PFN_vkResetFences fpResetFences;
      PFN_vkQueueSubmit fpQueueSubmit;
      PFN_vkCmdPipelineBarrier fpCmdPipelineBarrier;
      PFN_vkCmdCopyBufferToImage fpCmdCopyBufferToImage;
      PFN_vkBeginCommandBuffer fpBeginCommandBuffer;
      PFN_vkEndCommandBuffer fpEndCommandBuffer;
      PFN_vkQueueWaitIdle fpQueueWaitIdle;
      VkInstance instance = VK_NULL_HANDLE;
      VkSurfaceKHR surface = VK_NULL_HANDLE;
      VkPhysicalDevice physical_device = VK_NULL_HANDLE;
      VkPhysicalDeviceFeatures physical_device_features;
      VkDevice device = VK_NULL_HANDLE;;
      VkSwapchainKHR swapchain = VK_NULL_HANDLE;
      uint32_t graphics_queuefamily_index =UINT32_MAX, present_queuefamily_index =UINT32_MAX, swapchain_len = 0,
               current_index = 0;
      std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
      VkFormat surface_format;
      VkQueue graphics_queue = VK_NULL_HANDLE, present_queue = VK_NULL_HANDLE;
      VkExtent2D swapchain_extent;
      VkRenderPass render_pass = VK_NULL_HANDLE;
      VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
      VkDescriptorSetLayout camera_descriptor_set_layouts[1];
      VkDescriptorSet camera_tex_descriptor_set = VK_NULL_HANDLE;
      VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
      VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
      VkPipeline pipeline = VK_NULL_HANDLE;
      VkPresentModeKHR present_mode;
      VkFormat depth_format;
      VkImage depth_image  = VK_NULL_HANDLE;
      VmaAllocation depth_image_alloc = VK_NULL_HANDLE;
      VkImageView depth_image_view  = VK_NULL_HANDLE;
      std::vector<VkImage> swapchain_images;
      std::vector<VkImageView> swapchain_views;
      std::vector<VkFramebuffer> swapchain_framebuffers;
      VkCommandPool command_pool = VK_NULL_HANDLE;
      std::vector<VkCommandBuffer> command_buffers;
      VkCommandBuffer one_time_buffer = VK_NULL_HANDLE;
      std::vector<VkFence> camera_fences;
      std::vector<VkSemaphore> frame_available_semaphores;
      std::vector<VkSemaphore> render_complete_semaphores;
      VmaAllocator vma_allocator = VK_NULL_HANDLE;
      VkBuffer staging_buffer = VK_NULL_HANDLE;
      VmaAllocation staging_alloc = VK_NULL_HANDLE;
      VmaAllocationInfo staging_alloc_info = {};
      bool is_staged = true;
      VkSampler camera_texture_sampler = VK_NULL_HANDLE;;
      VkImage camera_texture_image = VK_NULL_HANDLE;
      VkImageView camera_texture_image_view = VK_NULL_HANDLE;
      VmaAllocationInfo camera_texture_image_allocinfo;
      VmaAllocation camera_texture_image_alloc = VK_NULL_HANDLE;;
      struct CameraTextureVertex { float pos[2]; }; //dummy used to force render, the actual tex coordinates are const in shader
      VkBuffer camera_vertex_buffer = VK_NULL_HANDLE;
      VmaAllocation camera_vertex_buffer_alloc = VK_NULL_HANDLE;
      int camera_width =-1, camera_height =-1;

      bool create(void* nativeSurface, void* nativeConnection, int width, int height,
                  const char* shaderAssetOverrideDir =nullptr);
      bool recreate();
      void destroy(bool isRenderOnly=false);
      VkPhysicalDevice select_device(const std::vector<VkPhysicalDevice>& physicalDevices, const VkPhysicalDeviceType preferredType);
      bool begin_single_command();
      bool end_single_command();
      bool load_shader(const std::string& shaderAssetName, VkShaderModule& shader);
      bool create_logical_device();
      bool create_depth_buffer();
      bool create_swapchain();
      bool create_render_pass();
      bool create_camera_tex_descriptor();
      bool create_camera_vertex_buffer();
      bool create_pipeline();
      bool create_framebuffers();
      bool create_instance();
      bool get_physical_device(const VkPhysicalDeviceType &preferredType);
      bool create_surface(void* nativeSurface, void *nativeConnection = nullptr);
      bool get_queue_families();
      void destroy_framebuffer();
      void clear_command_buffers();
      uint32_t find_memory_prop(uint32_t memoryTypeBits, VkMemoryPropertyFlags properties);
      bool create_command_buffers_and_pool();
      bool create_camera_texture(uint32_t w, uint32_t h);
      bool update_camera_texture_view();
      bool update_camera_texture(uint64_t seqno, FrameInfo* frame, const VkCommandBuffer& commandBuffer);
      bool record_default_commands();
#if !defined(NDEBUG)
      VkDebugReportCallbackEXT debug_report;
      void debug_layers(std::vector<const char *>& instance_layers);
      void setup_debug_callback();
      static VKAPI_ATTR VkBool32 VKAPI_CALL debug_report_callback(VkDebugReportFlagsEXT msgFlags,
                                                                  VkDebugReportObjectTypeEXT objType,
                                                                  uint64_t srcObject, size_t location, int32_t msgCode,
                                                                  const char * pLayerPrefix, const char * pMsg, void * pUserData);
      static void tex_pattern(uint32_t w, uint32_t h, void* data);
#endif
   private:
      friend class RendererFactory;

      void get_instance_function_pointers();

      char* renderer_name;
   };
};

#endif
