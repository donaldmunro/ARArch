#include <cstdio>
#include <cstddef>
#include <cfloat>
#include <utility>
#include <vector>
#include <array>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <functional>
#include <sstream>

#include "mar/render/VulkanRenderer.h"
#include "mar/render/VulkanTools.h"
#include "mar/util/android.hh"

namespace toMAR
{
   bool VulkanRenderer::render(uint64_t seqno, unsigned long cameraNo)
   //-------------------------------------------------------
   {
      auto renderEnd = [cameraNo,seqno](Repository* p)
      {
         if (p != nullptr)
         {
            p->is_rendering.store(false);
            p->delete_frame(cameraNo, seqno);
            // __android_log_print(ANDROID_LOG_INFO, "VulkanRenderer::render", "deleted frame %lu for camera %lu", seqno, cameraNo);
         }
      };
      // __android_log_print(ANDROID_LOG_INFO, "VulkanRenderer::render", "render frame %lu camera %lu", seqno, cameraNo);
      std::unique_ptr<Repository, decltype(renderEnd)> renderFinally(repository, renderEnd);
      std::shared_ptr<FrameInfo> frame;
      if ( (repository->get_frame(cameraNo, seqno, frame)) && (frame) )
      {
         frame->isRendering.store(false);
         uint32_t index = UINT32_MAX;
         VkResult last_error;
         VkFence& fence = camera_fences[current_index];
         if ((last_error = fpWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX)) != VK_SUCCESS)
         {
            __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::render", "Error waiting for fence (vkWaitForFences %d %s)",
                                last_error, VulkanTools::result_string(last_error).c_str());
            return false;
         }
         if ((last_error = fpResetFences(device, 1, &fence)) != VK_SUCCESS)
         {
             __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::render", "Error resetting fence (vkResetFences %d %s)",
                                 last_error, VulkanTools::result_string(last_error).c_str());
            return false;
         }

         VkSemaphore& frame_available_semaphore = frame_available_semaphores[current_index];

         last_error = fpAcquireNextImageKHR(device, swapchain, UINT64_MAX, frame_available_semaphore, VK_NULL_HANDLE,
                                          &index);
         switch (last_error)
         {
            case VK_SUCCESS:
            case VK_SUBOPTIMAL_KHR:
               break;
            case VK_ERROR_OUT_OF_DATE_KHR:
            {
               __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::render",
                                   "Screen resize required (vkAcquireNextImageKHR %d %s)", last_error,
                                   VulkanTools::result_string(last_error).c_str());
               destroy(true);
               if (! recreate())
                  return false;
               break;
            }
            default:
            {
               __android_log_print(ANDROID_LOG_ERROR,
                                   "VulkanRenderer::render", "Error acquiring next image (vkAcquireNextImageKHR %d %s)",
                                   last_error, VulkanTools::result_string(last_error).c_str());
               return false;
            }
         }

         update_camera_texture(seqno, frame.get(), one_time_buffer);
         __android_log_print(ANDROID_LOG_INFO, "VulkanRenderer::render", "camera texture updated %lu %lu", cameraNo, seqno);

         VkSemaphore& render_complete_semaphore = render_complete_semaphores[current_index];
         VkCommandBuffer& buffer = command_buffers[index];

         VkSemaphore submitWaitSemaphores[] = { frame_available_semaphore };
         VkSemaphore submitSignalSemaphores[] = {render_complete_semaphore};
         VkPipelineStageFlags submitWaitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
         VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO};
         submitInfo.waitSemaphoreCount = 1;
         submitInfo.pWaitSemaphores = submitWaitSemaphores;
         submitInfo.pWaitDstStageMask = submitWaitStages;
         submitInfo.commandBufferCount = 1;
         submitInfo.pCommandBuffers = &buffer;
         submitInfo.signalSemaphoreCount = 1;
         submitInfo.pSignalSemaphores = submitSignalSemaphores;
         if ((last_error = fpQueueSubmit(graphics_queue, 1, &submitInfo, fence)) != VK_SUCCESS)
         {
             __android_log_print(ANDROID_LOG_ERROR,
                                   "VulkanRenderer::render", "Error submitting command buffer (vkQueueSubmit %d %s)",
                                   last_error, VulkanTools::result_string(last_error).c_str());
            return false;
         }

         VkSemaphore presentWaitSemaphores[] = {render_complete_semaphore};
         VkSwapchainKHR swapchains[] = { swapchain };
         VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
         presentInfo.waitSemaphoreCount = 1;
         presentInfo.pWaitSemaphores = presentWaitSemaphores;
         presentInfo.swapchainCount = 1;
         presentInfo.pSwapchains = swapchains;
         presentInfo.pImageIndices = &index;
         presentInfo.pResults = nullptr;

         (++current_index) %= swapchain_len;

         last_error = fpQueuePresentKHR(present_queue, &presentInfo);
         switch (last_error)
         {
            case VK_SUCCESS:
            case VK_SUBOPTIMAL_KHR:
               break;
            case VK_ERROR_OUT_OF_DATE_KHR:
            {
               __android_log_print(ANDROID_LOG_ERROR,
                                   "VulkanRenderer::render", "Screen resize required (vkQueuePresentKHR %d %s)",
                                   last_error, VulkanTools::result_string(last_error).c_str());
               destroy(true);
               if (!recreate())
                  return false;
               break;
            }
            default:
            {
               __android_log_print(ANDROID_LOG_ERROR,
                                   "VulkanRenderer::render", "Error displaying image (vkQueuePresentKHR %d %s)",
                                   last_error, VulkanTools::result_string(last_error).c_str());
               return false;
            }
         }
         return true;
      }
      else
         __android_log_print(ANDROID_LOG_WARN, "VulkanRenderer::render", "Lost render frame %lu camera %lu", seqno, cameraNo);
      return false;
   }

   bool VulkanRenderer::create_camera_texture(uint32_t w, uint32_t h)
   //-----------------------------------------------------------------
   {
      if (camera_texture_image != VK_NULL_HANDLE)
      {
         vmaDestroyImage(vma_allocator, camera_texture_image, camera_texture_image_alloc);
         camera_texture_image = VK_NULL_HANDLE;
         camera_texture_image_alloc = VK_NULL_HANDLE;
      }

      VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
      imageInfo.imageType = VK_IMAGE_TYPE_2D;
      imageInfo.extent.width = w;
      imageInfo.extent.height = h;
      imageInfo.extent.depth = 1;
      imageInfo.mipLevels = 1;
      imageInfo.arrayLayers = 1;
      imageInfo.format = surface_format;
      imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
      imageInfo.initialLayout = (is_staged) ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_PREINITIALIZED;
      imageInfo.tiling = (is_staged) ? VK_IMAGE_TILING_OPTIMAL : VK_IMAGE_TILING_LINEAR;
      imageInfo.usage = (is_staged) ? VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                                   : VK_IMAGE_USAGE_SAMPLED_BIT;

      VmaAllocationCreateInfo imageAllocCreateInfo = {};
      imageAllocCreateInfo.usage = (is_staged) ? VMA_MEMORY_USAGE_GPU_ONLY : VMA_MEMORY_USAGE_CPU_ONLY;
      imageAllocCreateInfo.flags = (is_staged) ? 0 : VMA_ALLOCATION_CREATE_MAPPED_BIT;
      VkResult last_error;
      if ((last_error = vmaCreateImage(vma_allocator, &imageInfo, &imageAllocCreateInfo, &camera_texture_image,
                                       &camera_texture_image_alloc, &camera_texture_image_allocinfo)) != VK_SUCCESS)
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_camera_texture",
                             "Error creating destination image for camera frame (vmaCreateImage %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
         return false;
      }
      return update_camera_texture_view();
   }

   bool VulkanRenderer::update_camera_texture(uint64_t seqno, FrameInfo* frame, const VkCommandBuffer& commandBuffer)
   //------------------------------------------------------------------------------------------
   {
//      uint32_t w = static_cast<uint32_t>(frame->width), h = static_cast<uint32_t>(frame->height);
      const uint32_t w = static_cast<uint32_t>(std::min(camera_width, frame->width)),
                     h = static_cast<uint32_t>(std::min(camera_height, frame->height));
//      uint32_t w = static_cast<uint32_t>(swapchain_extent.width), h = static_cast<uint32_t>(swapchain_extent.height);
      const size_t texsize = w * h * 4;
//      __android_log_print(ANDROID_LOG_INFO, "VulkanRenderer::render", "Tex size %dx%d (%dx%d) Seq %lu camera %lu", w, h, frame->width, frame->height, frame->seqno, frame->camera_id);

      if (is_staged)
      {
         void* data;
         if ((staging_buffer == VK_NULL_HANDLE) ||
             (vmaMapMemory(vma_allocator, staging_alloc, &data) != VK_SUCCESS))
         {
            VkBufferCreateInfo bufferInfo =
            {
               .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = texsize,
               .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT//, .sharingMode = VK_SHARING_MODE_EXCLUSIVE
            };

            VmaAllocationCreateInfo allocInfo = { .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT, .usage = VMA_MEMORY_USAGE_CPU_ONLY };
            VkResult last_error = vmaCreateBuffer(vma_allocator, &bufferInfo, &allocInfo, &staging_buffer, &staging_alloc,
                                                  &staging_alloc_info);
            if (last_error !=  VK_SUCCESS)
            {
               __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::update_camera_texture",
                             "Error creating staging buffer (vmaCreateBuffer %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
               return false;
            }
         }
         void* env;
         unsigned char* framedata = frame->getColorData(env);
         memcpy(staging_alloc_info.pMappedData, framedata, texsize);
         draw(frame, staging_alloc_info.pMappedData);
         frame->releaseColorData(env, framedata);
//#if !defined(NDEBUG)
//         tex_pattern(swapchain_extent.width, swapchain_extent.height, staging_alloc_info.pMappedData);
//#endif
         begin_single_command();

         VkImageMemoryBarrier imgMemBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
         imgMemBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
         imgMemBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
         imgMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
         imgMemBarrier.subresourceRange.baseMipLevel = 0;
         imgMemBarrier.subresourceRange.levelCount = 1;
         imgMemBarrier.subresourceRange.baseArrayLayer = 0;
         imgMemBarrier.subresourceRange.layerCount = 1;
         imgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
         imgMemBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
         imgMemBarrier.image = camera_texture_image;
         imgMemBarrier.srcAccessMask = 0;
         imgMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

         fpCmdPipelineBarrier(one_time_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              0, 0, nullptr, 0, nullptr, 1, &imgMemBarrier);
         VkBufferImageCopy region = {};
         region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
         region.imageSubresource.layerCount = 1;
         region.imageExtent.width = w;
         region.imageExtent.height = h;
         region.imageExtent.depth = 1;

         fpCmdCopyBufferToImage(one_time_buffer, staging_buffer, camera_texture_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                1, &region);

         imgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
         imgMemBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
         imgMemBarrier.image = camera_texture_image;
         imgMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
         imgMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

         fpCmdPipelineBarrier(one_time_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                              0, 0, nullptr, 0, nullptr, 1, &imgMemBarrier);

         end_single_command();
      }
      else
      {  // This works in terms of the expected output but with lots of warnings about expecting
         // VK_IMAGE_LAYOUT_PREINITIALIZED but getting VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, presumably because the
         // Image/View is converted but type is not updated per frame.
         // Ignoring for the moment as Android Vulkan does not support linear at the moment anyway.
         void* env;
         unsigned char* framedata = frame->getColorData(env);
         memcpy(camera_texture_image_allocinfo.pMappedData, framedata, texsize);
         draw(frame, staging_alloc_info.pMappedData);
         frame->releaseColorData(env, framedata);
//         cv::Mat m(frame->height, frame->width, CV_8UC4, frame->frame_data);
//         cv::imwrite("mat.png", m);

         begin_single_command();
         VulkanTools::set_image_layout(one_time_buffer, camera_texture_image, VK_IMAGE_ASPECT_COLOR_BIT,
                          VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_ACCESS_HOST_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT, //VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
         end_single_command();
      }

      return true;
   }

   bool VulkanRenderer::update_camera_texture_view()
   //-----------------------------------------------
   {
      if (camera_texture_image_view != VK_NULL_HANDLE)
         vkDestroyImageView(device, camera_texture_image_view, nullptr);

      VkImageViewCreateInfo textureImageViewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
      textureImageViewInfo.image = camera_texture_image;
      textureImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      textureImageViewInfo.format = surface_format;
      textureImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      textureImageViewInfo.subresourceRange.baseMipLevel = 0;
      textureImageViewInfo.subresourceRange.levelCount = 1;
      textureImageViewInfo.subresourceRange.baseArrayLayer = 0;
      textureImageViewInfo.subresourceRange.layerCount = 1;
      VkResult last_error;
      if ((last_error = vkCreateImageView(device, &textureImageViewInfo, nullptr, &camera_texture_image_view)) !=
          VK_SUCCESS)
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::update_camera_texture_view",
                             "Error creating texture image view for camera frame (vkCreateImageView %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
         return false;
      }

      VkDescriptorImageInfo descriptorImageInfo = {};
      descriptorImageInfo.imageLayout = (is_staged) ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_PREINITIALIZED;
      descriptorImageInfo.imageView = camera_texture_image_view;
      descriptorImageInfo.sampler = camera_texture_sampler;

      VkWriteDescriptorSet writeDescriptorSet = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
      writeDescriptorSet.dstSet = camera_tex_descriptor_set;
      writeDescriptorSet.dstBinding = 1;
      writeDescriptorSet.dstArrayElement = 0;
      writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      writeDescriptorSet.descriptorCount = 1;
      writeDescriptorSet.pImageInfo = &descriptorImageInfo;
      vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
      return true;
   }

   bool
   VulkanRenderer::create(void* nativeSurface, void* nativeConnection, int width, int height,
                          const char* shaderAssetOverrideDir)
   //---------------------------------------------------------------------------------------------
   {
      if (shaderAssetOverrideDir != nullptr)
         shadersAssetsDir = shaderAssetOverrideDir;
      if (shadersAssetsDir.empty())
         shadersAssetsDir = "";
      else if (shadersAssetsDir[shadersAssetsDir.size() - 1] != '/')
         shadersAssetsDir += '/';
      if (!is_asset_dir(repository->pAssetManager, shadersAssetsDir.c_str()))
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create",
                             "The specified shader directory %s is not a valid asset directory.", shadersAssetsDir.c_str());
         return false;
      }
      VkPhysicalDeviceType preferredType;
      preferredType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
      if (!create_instance())
         return false;
      get_instance_function_pointers();
      VulkanTools::init(instance);

      if (! create_surface(nativeSurface, nativeConnection))
         return false;

      if (! get_physical_device(preferredType))
         return false;

      if (!get_queue_families())
      {
         vkDestroyInstance(instance, nullptr);
         instance = VK_NULL_HANDLE;
         return false;
      }

      if (!create_logical_device())
      {
         vkDestroyInstance(instance, nullptr);
         instance = VK_NULL_HANDLE;
         return false;
      }

      VmaAllocatorCreateInfo allocatorInfo = {};
      allocatorInfo.physicalDevice = physical_device;
      allocatorInfo.device = device;
      if (vmaCreateAllocator(&allocatorInfo, &vma_allocator) != VK_SUCCESS)
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create", "Error creating Vulkan Memory Allocator instance");
         destroy();
         return false;
      }

      if (!create_swapchain())
      {
         destroy();
         return false;
      }
      camera_width = width;  camera_height = height;
      if (! create_depth_buffer())
      {
         destroy();
         return false;
      }
      if (!create_render_pass())
      {
         destroy();
         return false;
      }

      if (!create_framebuffers())
      {
         destroy();
         return false;
      }

      if (!create_command_buffers_and_pool())
      {
         destroy();
         return false;
      }

      if (!create_camera_tex_descriptor())
      {
         destroy();
         return false;
      }
      if (!create_camera_texture(swapchain_extent.width, swapchain_extent.height))
      {
         destroy();
         return false;
      }
      if (!create_camera_vertex_buffer())
      {
         destroy();
         return false;
      }
      if (!create_pipeline())
      {
         destroy();
         return false;
      }
      if (!record_default_commands())
      {
         destroy();
         return false;
      }
      return true;
   }

   bool VulkanRenderer::recreate()
   //----------------------------
   {
      if (!create_swapchain())
         return false;

      if (!create_render_pass())
         return false;

      if (!create_framebuffers())
         return false;

      if (!create_command_buffers_and_pool())
         return false;

      if (!create_camera_tex_descriptor())
         return false;
      if (!create_camera_texture(swapchain_extent.width, swapchain_extent.height))
         return false;
      if (!create_pipeline())
         return false;
      return true;
   }

   bool VulkanRenderer::create_logical_device()
   //-----------------------------------------
   {
      float priorities[] = {1.0f};
      std::vector<const char*> device_extensions = {"VK_KHR_swapchain"};
      queue_create_infos.clear();
      queue_create_infos.push_back(VkDeviceQueueCreateInfo
      {
         .sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
         .queueFamilyIndex = graphics_queuefamily_index, .queueCount = 1,
         .pQueuePriorities = priorities
      } );

      if (present_queuefamily_index != graphics_queuefamily_index)
      {
         queue_create_infos.push_back(VkDeviceQueueCreateInfo
         {
            .sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = present_queuefamily_index, .queueCount = 1,
            .pQueuePriorities = priorities
         } );
      }
      VkPhysicalDeviceFeatures deviceFeatures = {};
      //deviceFeatures.fillModeNonSolid = VK_TRUE;
      deviceFeatures.samplerAnisotropy = VK_TRUE;
      VkDeviceCreateInfo deviceCreateInfo {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
      deviceCreateInfo.queueCreateInfoCount = queue_create_infos.size();
      deviceCreateInfo.pQueueCreateInfos = queue_create_infos.data();
      deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
      deviceCreateInfo.ppEnabledExtensionNames = device_extensions.data();
      deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
      VkResult last_error;
      if ((last_error = vkCreateDevice(physical_device, &deviceCreateInfo, nullptr, &device)) != VK_SUCCESS)
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_logical_device",
                             "Error creating logical device (vkCreateDevice %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
         return false;
      }
      vkGetDeviceQueue(device, graphics_queuefamily_index, 0, &graphics_queue);
      vkGetDeviceQueue(device, present_queuefamily_index, 0, &present_queue);
      return ((graphics_queue != VK_NULL_HANDLE) && (present_queue != VK_NULL_HANDLE));
   }

   bool VulkanRenderer::create_swapchain()
   //--------------------------------------
   {
      VkSurfaceCapabilitiesKHR surfaceCapabilities;
      auto fpGetPhysicalDeviceSurfaceCapabilitiesKHR =
         reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(vkGetInstanceProcAddr(instance,
                                                                         "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
      VkResult last_error = VK_SUCCESS;
      if ((last_error = fpGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surfaceCapabilities)) !=
          VK_SUCCESS)
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_swapchain",
                             "Error getting surface caps (vkGetPhysicalDeviceSurfaceCapabilitiesKHR %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
         return false;
      }

      swapchain_extent = surfaceCapabilities.currentExtent;
      std::vector<VkSurfaceFormatKHR> formats;
      std::unordered_map<VkFormat, size_t> surface_format_indices;
      std::stringstream ss;
      if (VulkanTools::get_surface_formats(instance, physical_device, surface, formats, surface_format_indices, &ss) == 0)
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_swapchain",
                             "Error obtaining surface formats (%s)", ss.str().c_str());
         return false;
      }
      else
         __android_log_print(ANDROID_LOG_INFO, "VulkanRenderer::create_swapchain",
                             "Available surface formats:\n (%s)", ss.str().c_str());
      size_t surface_format_index;

      surface_format = VK_FORMAT_R8G8B8A8_UNORM;
      auto it = surface_format_indices.find(surface_format);
      if (it == surface_format_indices.end())
      {
         surface_format = VK_FORMAT_B8G8R8A8_UNORM;
         it = surface_format_indices.find(surface_format);
      }
      if (it == surface_format_indices.end())
      {
         surface_format = VK_FORMAT_UNDEFINED;
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_swapchain",
                             "Error selecting matching format (VK_FORMAT_R8G8B8A8_UNORM || VK_FORMAT_B8G8R8A8_UNORM)");
         return false;
      }
      else
         surface_format_index = surface_format_indices[surface_format];
      __android_log_print(ANDROID_LOG_INFO, "VulkanRenderer::create_swapchain",
                          "Selected surface format %s", VulkanTools::surface_format_string(surface_format).c_str());

      uint32_t cpresentmode = 0;
      present_mode = VK_PRESENT_MODE_FIFO_KHR;
//      if (((last_error = fpGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &cpresentmode, nullptr)) ==
//           VK_SUCCESS) && (cpresentmode > 0))
//      {
//         std::vector<VkPresentModeKHR> present_modes(cpresentmode);
//         if ((last_error = fpGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &cpresentmode,
//                                                                     present_modes.data())) == VK_SUCCESS)
//         {
//            if (std::find(present_modes.begin(), present_modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) !=
//                present_modes.end())
//               present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
//         }
//      }
      VkSwapchainCreateInfoKHR swapchainCreateInfo
      {
         .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
         .pNext = nullptr,
         .flags = 0,
         .surface = surface,
         .minImageCount = 3, //surfaceCapabilities.minImageCount, // buffering
         .imageFormat = formats[surface_format_index].format,
         .imageColorSpace = formats[surface_format_index].colorSpace,
         .imageExtent = surfaceCapabilities.currentExtent,
         .imageArrayLayers = 1,
         .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
         .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
         .queueFamilyIndexCount = 1,
         .pQueueFamilyIndices = &graphics_queuefamily_index,
         .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
//         .compositeAlpha = VkCompositeAlphaFlagBitsKHR::VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
         .compositeAlpha = VkCompositeAlphaFlagBitsKHR::VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
         .presentMode = present_mode,
         .clipped = VK_FALSE,
         .oldSwapchain = VK_NULL_HANDLE,
      };
      if (graphics_queuefamily_index != present_queuefamily_index)
      {
         uint32_t indices[2];
         indices[0] = graphics_queuefamily_index; indices[1] = present_queuefamily_index;
         swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
         swapchainCreateInfo.compositeAlpha =  VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
         swapchainCreateInfo.queueFamilyIndexCount = 2;
         swapchainCreateInfo.pQueueFamilyIndices = indices;
      }
      auto fpCreateSwapchainKHR =
            reinterpret_cast<PFN_vkCreateSwapchainKHR>(vkGetInstanceProcAddr(instance, "vkCreateSwapchainKHR"));
      if ((last_error = fpCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain)) != VK_SUCCESS)
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_swapchain",
                             "Error creating swapchain (fpCreateSwapchainKHR %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
         return false;
      }
      auto fpGetSwapchainImagesKHR =
            reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(vkGetInstanceProcAddr(instance, "vkGetSwapchainImagesKHR"));
      fpGetSwapchainImagesKHR(device, swapchain, &swapchain_len, nullptr);

      VkFormatProperties formatProperties;
      vkGetPhysicalDeviceFormatProperties(physical_device, surface_format, &formatProperties);
      if (!((formatProperties.linearTilingFeatures | formatProperties.optimalTilingFeatures) &
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_swapchain",
                             "Physical device does not support sampling");
         return false;
      }

//      is_staged = !(formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
      is_staged = true;
      return (swapchain_len > 0);
   }

   bool VulkanRenderer::create_render_pass()
   //---------------------------------------
   {
      VkAttachmentDescription attachments[2];
      attachments[0] = VkAttachmentDescription
      {
         .format = surface_format, .samples = VK_SAMPLE_COUNT_1_BIT,
         .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
         .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
         .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
      };
      attachments[1] = VkAttachmentDescription
      {
         .format = depth_format, .samples = VK_SAMPLE_COUNT_1_BIT,
         .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
         .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
         .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
      };
      VkAttachmentReference colorAttachmentRef = { .attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
      VkAttachmentReference depthStencilAttachmentRef = { .attachment = 1, .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
      VkSubpassDescription subpassDesc =
      {
         .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, .colorAttachmentCount = 1,
         .pColorAttachments = &colorAttachmentRef, .pDepthStencilAttachment = &depthStencilAttachmentRef
      };
      VkRenderPassCreateInfo renderPassInfo =
      { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
         .attachmentCount = 2, .pAttachments = attachments, .subpassCount = 1, .pSubpasses = &subpassDesc,
         .dependencyCount = 0
      };
      VkResult last_error;
      if ((last_error = vkCreateRenderPass(device, &renderPassInfo, nullptr, &render_pass)) != VK_SUCCESS)
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_render_pass",
                             "Error creating render pass (vkCreateRenderPass %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());

         return false;
      }
      return true;
   }

   bool VulkanRenderer::load_shader(const std::string& shaderAssetName, VkShaderModule& shader)
   //-----------------------------------------------------------------------------------------
   {
      std::vector<char> source;
      bool isRead = read_asset_vector(repository->pAssetManager, shaderAssetName.c_str(), source);
      if (! isRead)
      {
         if (shaderAssetName[0] == '/')
         {
            std::string path = shaderAssetName.substr(1);
            isRead = read_asset_vector(repository->pAssetManager, path.c_str(), source);
         }
         if (! isRead)
         {
            __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::load_shader",
                             "Error reading shader %s from assets", shaderAssetName.c_str());
            return false;
         }
      }
      VkShaderModuleCreateInfo shaderModuleCreateInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
      shaderModuleCreateInfo.codeSize = source.size();
      shaderModuleCreateInfo.pCode = (const uint32_t*) source.data();
      shader = VK_NULL_HANDLE;
      return (vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shader) == VK_SUCCESS);
   }

   bool VulkanRenderer::create_camera_tex_descriptor()
   //----------------------------------------------
   {
      VkSamplerCreateInfo samplerInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
      samplerInfo.magFilter = VK_FILTER_LINEAR;
      samplerInfo.minFilter = VK_FILTER_LINEAR;
      samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
      samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
      samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
      samplerInfo.anisotropyEnable = physical_device_features.samplerAnisotropy;
      samplerInfo.maxAnisotropy = (physical_device_features.samplerAnisotropy == VK_TRUE) ? 16 : 1;
      samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
      samplerInfo.unnormalizedCoordinates = VK_FALSE;
      samplerInfo.compareEnable = VK_FALSE;
      samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
      samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
      samplerInfo.mipLodBias = 0.f;
      samplerInfo.minLod = 0.f;
      samplerInfo.maxLod = FLT_MAX;
      VkResult last_error = VK_SUCCESS;
      if ((last_error = vkCreateSampler(device, &samplerInfo, nullptr, &camera_texture_sampler)) != VK_SUCCESS)
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_camera_tex_descriptor",
                             "Error creating camera texture sampler (vkCreateSampler %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
         return false;
      }

      VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
      samplerLayoutBinding.binding = 1;
      samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      samplerLayoutBinding.descriptorCount = 1;
      samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

      VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
      descriptorSetLayoutInfo.bindingCount = 1;
      descriptorSetLayoutInfo.pBindings = &samplerLayoutBinding;
      if ((last_error = vkCreateDescriptorSetLayout(device, &descriptorSetLayoutInfo, nullptr,
                                                    &descriptor_set_layout)) != VK_SUCCESS)
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_camera_tex_descriptor",
                             "Error creating descriptor set layout (vkCreateDescriptorSetLayout %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
         return false;
      }

      VkDescriptorPoolSize descriptorPoolSizes[1];
      memset(descriptorPoolSizes, 0, sizeof(descriptorPoolSizes));
      descriptorPoolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      descriptorPoolSizes[0].descriptorCount = 1;

      VkDescriptorPoolCreateInfo descriptorPoolInfo =
      {
         .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
         .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
         .maxSets = 1, .poolSizeCount = 1, .pPoolSizes = descriptorPoolSizes
      };
      if ((last_error = vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptor_pool)) != VK_SUCCESS)
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_camera_tex_descriptor",
                             "Error creating descriptor pool (vkCreateDescriptorPool %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
         return false;
      }

      camera_descriptor_set_layouts[0] = descriptor_set_layout;
      VkDescriptorSetAllocateInfo descriptorSetInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
      descriptorSetInfo.descriptorPool = descriptor_pool;
      descriptorSetInfo.descriptorSetCount = 1;
      descriptorSetInfo.pSetLayouts = camera_descriptor_set_layouts;
      if ((last_error = vkAllocateDescriptorSets(device, &descriptorSetInfo, &camera_tex_descriptor_set)) != VK_SUCCESS)
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_camera_tex_descriptor",
                             "Error allocating descriptor set (vkAllocateDescriptorSets %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
         return false;
      }
      return true;
   }

   bool VulkanRenderer::create_camera_vertex_buffer()
   //------------------------------------------------
   {
      static CameraTextureVertex vertices[] = { {{0, 0}}, {{0, 0}}, {{0, 0}}, {{0, 0}} };
      const size_t cvb = sizeof(vertices);
      VkBufferCreateInfo vbInfo = { .sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = cvb,
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT, .sharingMode = VK_SHARING_MODE_EXCLUSIVE };

      VmaAllocationCreateInfo vbAllocCreateInfo = {};
      vbAllocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
      vbAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

      VkBuffer stagingVertexBuffer = VK_NULL_HANDLE;
      VmaAllocation stagingVertexBufferAlloc = VK_NULL_HANDLE;
      VmaAllocationInfo stagingVertexBufferAllocInfo = {};
      VkResult last_error;
      if ((last_error = vmaCreateBuffer(vma_allocator, &vbInfo, &vbAllocCreateInfo, &stagingVertexBuffer,
                                        &stagingVertexBufferAlloc, &stagingVertexBufferAllocInfo)) != VK_SUCCESS)
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_camera_vertex_buffer",
                             "Error allocating staging vertex buffer (vmaCreateBuffer %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
         return false;
      }
      memcpy(stagingVertexBufferAllocInfo.pMappedData, vertices, cvb);
      vbInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
      vbAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
      vbAllocCreateInfo.flags = 0;
      if ((last_error = vmaCreateBuffer(vma_allocator, &vbInfo, &vbAllocCreateInfo, &camera_vertex_buffer,
                                        &camera_vertex_buffer_alloc, nullptr)) != VK_SUCCESS)
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_camera_vertex_buffer",
                             "Error allocating vertex buffer (vmaCreateBuffer %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
         return false;
      }

      if (!begin_single_command())
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_camera_vertex_buffer", "Error in start buffer command");
         return false;
      }

      VkBufferCopy vbCopyRegion = {};
      vbCopyRegion.srcOffset = 0;
      vbCopyRegion.dstOffset = 0;
      vbCopyRegion.size = vbInfo.size;
      vkCmdCopyBuffer(one_time_buffer, stagingVertexBuffer, camera_vertex_buffer, 1, &vbCopyRegion);

      if (!end_single_command())
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_camera_vertex_buffer", "Error in end buffer command");
         return false;
      }
      vmaDestroyBuffer(vma_allocator, stagingVertexBuffer, stagingVertexBufferAlloc);
      return true;
   }

   bool VulkanRenderer::create_pipeline()
   //------------------------------------
   {
      VkResult last_error = VK_SUCCESS;
      std::string vertex_shader_asset = shadersAssetsDir + CAMERA_VERTEX_SHADER;
      VkShaderModule vertex_shader;
      if (!load_shader(vertex_shader_asset, vertex_shader))
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_pipeline",
                             "Error reading camera texture vertex shader %s", vertex_shader_asset.c_str());
         return false;
      }
      std::string fragment_shader_asset = shadersAssetsDir + CAMERA_FRAGMENT_SHADER;
      VkShaderModule fragment_shader;
      if (!load_shader(fragment_shader_asset, fragment_shader))
      {
          __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_pipeline",
                             "Error reading camera texture fragment shader %s", fragment_shader_asset.c_str());
         vkDestroyShaderModule(device, vertex_shader, nullptr);
         return false;
      }

      VkPipelineShaderStageCreateInfo vertPipelineShaderStageInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
      vertPipelineShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
      vertPipelineShaderStageInfo.module = vertex_shader;
      vertPipelineShaderStageInfo.pName = "main";

      VkPipelineShaderStageCreateInfo fragPipelineShaderStageInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
      fragPipelineShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      fragPipelineShaderStageInfo.module = fragment_shader;
      fragPipelineShaderStageInfo.pName = "main";

      VkPipelineShaderStageCreateInfo pipelineShaderStageInfos[] =
      {
         vertPipelineShaderStageInfo,
         fragPipelineShaderStageInfo
      };

      VkVertexInputBindingDescription bindingDescription = {.binding = 0, .stride = sizeof(CameraTextureVertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};

      VkVertexInputAttributeDescription attributeDescriptions {.location = 0, .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT, .offset = 0};

      VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateInfo =
      {
         .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
         .vertexBindingDescriptionCount = 1, .pVertexBindingDescriptions = &bindingDescription,
         .vertexAttributeDescriptionCount = 1, .pVertexAttributeDescriptions = &attributeDescriptions
      };

      VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
      pipelineInputAssemblyStateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
      pipelineInputAssemblyStateInfo.primitiveRestartEnable = VK_TRUE;

      VkViewport viewport = {};
      viewport.x = 0.f;
      viewport.y = 0.f;
      viewport.width = static_cast<float>(camera_width); // swapchain_extent.width);
      viewport.height = static_cast<float>(camera_height); //swapchain_extent.height);
      viewport.minDepth = 0.f;
      viewport.maxDepth = 1.f;

      VkRect2D scissor = { .offset = { .x = 0, .y = 0}, .extent = swapchain_extent };
      VkPipelineViewportStateCreateInfo pipelineViewportStateInfo =
      {
         .sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
         .viewportCount = 1, .pViewports = &viewport,
         .scissorCount = 1, .pScissors = &scissor
      };

      VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
      pipelineRasterizationStateInfo.depthClampEnable = VK_FALSE;
      pipelineRasterizationStateInfo.rasterizerDiscardEnable = VK_FALSE;
      pipelineRasterizationStateInfo.polygonMode = VK_POLYGON_MODE_FILL;
      pipelineRasterizationStateInfo.lineWidth = 1.f;
      pipelineRasterizationStateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
      pipelineRasterizationStateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
      pipelineRasterizationStateInfo.depthBiasEnable = VK_FALSE;
      pipelineRasterizationStateInfo.depthBiasConstantFactor = 0.f;
      pipelineRasterizationStateInfo.depthBiasClamp = 0.f;
      pipelineRasterizationStateInfo.depthBiasSlopeFactor = 0.f;

      VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
      pipelineMultisampleStateInfo.sampleShadingEnable = VK_FALSE;
      pipelineMultisampleStateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
      pipelineMultisampleStateInfo.minSampleShading = 1.f;
      pipelineMultisampleStateInfo.pSampleMask = nullptr;
      pipelineMultisampleStateInfo.alphaToCoverageEnable = VK_FALSE;
      pipelineMultisampleStateInfo.alphaToOneEnable = VK_FALSE;

      VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState = {};
      pipelineColorBlendAttachmentState.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
      pipelineColorBlendAttachmentState.blendEnable = VK_FALSE;

      VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
      pipelineColorBlendStateInfo.logicOpEnable = VK_FALSE;
      pipelineColorBlendStateInfo.logicOp = VK_LOGIC_OP_COPY;
      pipelineColorBlendStateInfo.attachmentCount = 1;
      pipelineColorBlendStateInfo.pAttachments = &pipelineColorBlendAttachmentState;

      VkPipelineDepthStencilStateCreateInfo depthStencilStateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
      depthStencilStateInfo.depthTestEnable = VK_TRUE;
      depthStencilStateInfo.depthWriteEnable = VK_TRUE;
      depthStencilStateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
      depthStencilStateInfo.depthBoundsTestEnable = VK_FALSE;
      depthStencilStateInfo.stencilTestEnable = VK_FALSE;


      VkPipelineLayoutCreateInfo pipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
      pipelineLayoutInfo.setLayoutCount = 1;
      pipelineLayoutInfo.pSetLayouts = camera_descriptor_set_layouts;
      pipelineLayoutInfo.pushConstantRangeCount = 0;
      if ((last_error = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipeline_layout)) != VK_SUCCESS)
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_pipeline",
                             "Error creating pipeline layout (vkCreatePipelineLayout %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
         vkDestroyShaderModule(device, fragment_shader, nullptr);
         vkDestroyShaderModule(device, vertex_shader, nullptr);
         return false;
      }
      std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
      VkPipelineDynamicStateCreateInfo dynamicState = { .sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = 2, .pDynamicStates = dynamicStates.data() };
      VkGraphicsPipelineCreateInfo pipelineInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
      pipelineInfo.stageCount = 2;
      pipelineInfo.pStages = pipelineShaderStageInfos;
      pipelineInfo.pVertexInputState = &pipelineVertexInputStateInfo;
      pipelineInfo.pInputAssemblyState = &pipelineInputAssemblyStateInfo;
      pipelineInfo.pViewportState = &pipelineViewportStateInfo;
      pipelineInfo.pRasterizationState = &pipelineRasterizationStateInfo;
      pipelineInfo.pMultisampleState = &pipelineMultisampleStateInfo;
      pipelineInfo.pDepthStencilState = &depthStencilStateInfo;
      pipelineInfo.pColorBlendState = &pipelineColorBlendStateInfo;
      pipelineInfo.pDynamicState = &dynamicState;
      pipelineInfo.layout = pipeline_layout;
      pipelineInfo.renderPass = render_pass;
      pipelineInfo.subpass = 0;
      pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
      pipelineInfo.basePipelineIndex = -1;
      if ((last_error = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline)) !=
          VK_SUCCESS)
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_pipeline",
                             "Error creating pipeline (vkCreateGraphicsPipelines %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());

         vkDestroyShaderModule(device, fragment_shader, nullptr);
         vkDestroyShaderModule(device, vertex_shader, nullptr);
         return false;
      }
      vkDestroyShaderModule(device, fragment_shader, nullptr);
      vkDestroyShaderModule(device, vertex_shader, nullptr);
      return true;
   }

   bool VulkanRenderer::create_depth_buffer()
   //---------------------------------------
   {
      std::vector<VkFormat> depth_formats {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
                                           VK_FORMAT_D24_UNORM_S8_UINT};
      depth_format = VulkanTools::find_supported_format(physical_device, depth_formats, VK_IMAGE_TILING_OPTIMAL,
                                                        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
      if (depth_format == VK_FORMAT_UNDEFINED)
      {
         __android_log_print(ANDROID_LOG_WARN, "VulkanRenderer::create_depth_buffer", "Could not find a matching depth format");
         depth_image_view = VK_NULL_HANDLE;
         return false;
      }
      else
      {
         const VkImageCreateInfo depthImageInfo =
         {
               .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
               .imageType = VK_IMAGE_TYPE_2D,
               .format = depth_format,
               .extent = {swapchain_extent.width, swapchain_extent.height, 1},
               .mipLevels = 1,
               .arrayLayers = 1,
               .samples = VK_SAMPLE_COUNT_1_BIT,
               .tiling = VK_IMAGE_TILING_OPTIMAL,
               .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
         };

         VmaAllocationCreateInfo depthImageAllocCreateInfo = {};
         depthImageAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
         VkResult last_error;
         if ((last_error = vmaCreateImage(vma_allocator, &depthImageInfo, &depthImageAllocCreateInfo, &depth_image,
                                          &depth_image_alloc,
                                          nullptr)) != VK_SUCCESS)
         {
            __android_log_print(ANDROID_LOG_WARN, "VulkanRenderer::create_depth_buffer",
                             "Error creating depth image (vmaCreateImage %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
            depth_image_view = VK_NULL_HANDLE;
            return false;
         }
         else
         {
            VkImageViewCreateInfo depthImageViewInfo =
            {
                  .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = depth_image,
                  .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = depth_format,
                  .subresourceRange =
                  {  .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .baseMipLevel = 0, .levelCount = 1,
                     .baseArrayLayer = 0, .layerCount = 1
                  }
            };
            if ((last_error = vkCreateImageView(device, &depthImageViewInfo, nullptr, &depth_image_view)) != VK_SUCCESS)
            {
               __android_log_print(ANDROID_LOG_WARN, "VulkanRenderer::create_depth_buffer",
                             "Error creating depth image view (vkCreateImageView %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
               depth_image_view = VK_NULL_HANDLE;
               return false;
            }
            return true;
         }
      }
      return false;
   }

   bool VulkanRenderer::create_framebuffers()
   //------------------------------------------
   {
      swapchain_images.resize(swapchain_len);
      swapchain_views.resize(swapchain_len);
      VkResult last_error = VK_SUCCESS;
      PFN_vkGetSwapchainImagesKHR fpGetSwapchainImagesKHR =
         reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(vkGetInstanceProcAddr(instance, "vkGetSwapchainImagesKHR"));
      if ((last_error = fpGetSwapchainImagesKHR(device, swapchain, &swapchain_len, swapchain_images.data())) !=
          VK_SUCCESS)
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_framebuffers",
                             "Error getting swapchain images (vkGetSwapchainImagesKHR %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
         vkDestroyDevice(device, nullptr);
         vkDestroyInstance(instance, nullptr);
         return false;
      }
      for (uint32_t i = 0; i < swapchain_len; i++)
      {
         VkImageViewCreateInfo viewCreateInfo =
               {
                     .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                     .pNext = nullptr,
                     .flags = 0,
                     .image = swapchain_images[i],
                     .viewType = VK_IMAGE_VIEW_TYPE_2D,
                     .format = surface_format,
                     .components =
                           {
                                 .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                                 .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                                 .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                                 .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                           },
                     .subresourceRange =
                           {
                                 .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                 .baseMipLevel = 0,
                                 .levelCount = 1,
                                 .baseArrayLayer = 0,
                                 .layerCount = 1,
                           }
               };
         if ((last_error = vkCreateImageView(device, &viewCreateInfo, nullptr, &swapchain_views[i])) != VK_SUCCESS)
         {
            __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_framebuffers",
                                "Error creating swapchain image %d/%d (vkCreateImageView %d %s)", i, swapchain_len,
                                last_error, VulkanTools::result_string(last_error).c_str());
            vkDestroyDevice(device, nullptr);
            vkDestroyInstance(instance, nullptr);
            return false;
         }
      }

      swapchain_framebuffers.resize(swapchain_len);
      for (uint32_t i = 0; i < swapchain_len; i++)
      {
         VkImageView attachments[2] = {swapchain_views[i], depth_image_view};
         VkFramebufferCreateInfo fbCreateInfo
         {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = render_pass,
            .attachmentCount = static_cast<uint32_t>((depth_image_view == VK_NULL_HANDLE) ? 1 : 2),
            .pAttachments = attachments,
            .width = static_cast<uint32_t>(swapchain_extent.width),
             .height = static_cast<uint32_t>(swapchain_extent.height),
            .layers = 1,
         };

         if ((last_error = vkCreateFramebuffer(device, &fbCreateInfo, nullptr, &swapchain_framebuffers[i])) !=
             VK_SUCCESS)
         {
            __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_framebuffers",
                                "Error creating swapchain framebuffer %d/%d (vkCreateFramebuffer %d %s)", i, swapchain_len,
                                last_error, VulkanTools::result_string(last_error).c_str());
            return false;
         }
      }
      return true;
   }

   bool VulkanRenderer::create_command_buffers_and_pool()
   //-----------------------------------------------------
   {
      VkCommandPoolCreateInfo cmdPoolCreateInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
      cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
      VkResult last_error = VK_SUCCESS;
      if ((last_error = vkCreateCommandPool(device, &cmdPoolCreateInfo, nullptr, &command_pool)) != VK_SUCCESS)
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_command_pool",
                             "Error creating command pool (vkCreateCommandPool %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());

         return false;
      }
      command_buffers.resize(swapchain_len + 1); //One extra one-time buffer
      camera_fences.resize(swapchain_len);
      frame_available_semaphores.resize(swapchain_len);
      render_complete_semaphores.resize(swapchain_len);
      VkCommandBufferAllocateInfo cmdBufferCreateInfo
      {
         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
         .commandPool = command_pool,
         .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1,
      };
      VkFenceCreateInfo fenceCreateInfo = { .sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
      VkSemaphoreCreateInfo semaphoreCreateInfo = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
      for (uint32_t bi = 0; bi < (swapchain_len + 1); bi++)
      {
         if ((last_error = vkAllocateCommandBuffers(device, &cmdBufferCreateInfo, &command_buffers[bi])) != VK_SUCCESS)
         {
            __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_command_pool",
                             "Error creating command buffer (vkAllocateCommandBuffers %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
            clear_command_buffers();
            return false;
         }
         if (bi == swapchain_len)
         {
            one_time_buffer = command_buffers[bi];
            continue;
         }
         VkFence& fence = camera_fences[bi];
         if ((last_error = vkCreateFence(device, &fenceCreateInfo, nullptr, &fence)) != VK_SUCCESS)
         {
            __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_command_pool",
                             "Error creating fence (vkCreateFence %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
            clear_command_buffers();
            return false;
         }
         VkSemaphore& frame_available_semaphore = frame_available_semaphores[bi];
         if ( (last_error = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frame_available_semaphore)) != VK_SUCCESS)
         {
            __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_command_pool",
                             "Error creating frame available semaphore (vkCreateSemaphore %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
            clear_command_buffers();
            return false;
         }
         VkSemaphore& render_complete_semaphore = render_complete_semaphores[bi];
         if ( (last_error = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &render_complete_semaphore)) != VK_SUCCESS)
         {
            __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_command_pool",
                             "Error creating render complete semaphore (vkCreateSemaphore %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
            clear_command_buffers();
            return false;
         }
      }
      return true;
   }

   bool VulkanRenderer::begin_single_command()
   //------------------------------------------
   {
      VkCommandBufferBeginInfo cmdBufBeginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
      cmdBufBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      if (fpBeginCommandBuffer(one_time_buffer, &cmdBufBeginInfo) != VK_SUCCESS)
      {
        __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::begin_single_command",
                             "fpBeginCommandBuffer failed");
         return false;
      }
      return true;
   }

   bool VulkanRenderer::end_single_command()
   //-------------------------------------------------------------------------------------------------
   {
      VkResult last_error;
      if ((last_error = fpEndCommandBuffer(one_time_buffer)) != VK_SUCCESS)
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::end_single_command",
                             "vkEndCommandBuffer failed (%d %s)", last_error, VulkanTools::result_string(last_error).c_str());

         return false;
      }

      VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
      submitInfo.commandBufferCount = 1;
      submitInfo.pCommandBuffers = &one_time_buffer;
      if ((last_error = fpQueueSubmit(graphics_queue, 1, &submitInfo, VK_NULL_HANDLE)) != VK_SUCCESS)
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::end_single_command",
                             "vkQueueSubmit failed (%d %s)", last_error, VulkanTools::result_string(last_error).c_str());
         return false;
      }
      if ((last_error = fpQueueWaitIdle(graphics_queue)) != VK_SUCCESS)
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::end_single_command",
                             "vkQueueWaitIdle failed (%d %s)", last_error, VulkanTools::result_string(last_error).c_str());
         return false;
      }
      return true;
   }

    bool VulkanRenderer::record_default_commands()
    //--------------------------------------------
    {
       VkResult last_error;
       for (size_t bi = 0; bi < swapchain_len; bi++)
       {
          const VkCommandBuffer& commandbuf = command_buffers[bi];
          VkCommandBufferBeginInfo commandBufferBeginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
          commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
          if ((last_error = fpBeginCommandBuffer(commandbuf, &commandBufferBeginInfo)) != VK_SUCCESS)
          {
             __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::record_default_commands",
                             "Error beginning command buffer %ld (vkBeginCommandBuffer %d %s)",
                             bi, last_error, VulkanTools::result_string(last_error).c_str());

             return false;
          }

          VkClearValue clearValues[2];
          clearValues[0].color =  { { background_color[0],  background_color[1], background_color[2], 1.0f } } ;
          clearValues[0].depthStencil.depth = 0.0f;
          clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } } ;
          clearValues[1].depthStencil.depth = 1.0f;

          VkRenderPassBeginInfo renderPassBeginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
          renderPassBeginInfo.renderPass = render_pass;
          renderPassBeginInfo.framebuffer = swapchain_framebuffers[bi];
          renderPassBeginInfo.renderArea.offset = { .x = 0, .y = 0 };
          renderPassBeginInfo.renderArea.extent = swapchain_extent;
          renderPassBeginInfo.clearValueCount = 2;
          renderPassBeginInfo.pClearValues = clearValues;
          vkCmdBeginRenderPass(commandbuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
          VkViewport viewport =
          {
             .x =.0f, .y =.0f, .width = static_cast<float>(swapchain_extent.width),
             .height = static_cast<float>(swapchain_extent.height), .minDepth =.0f, .maxDepth = 1.0f,
          };
          vkCmdSetViewport(commandbuf, 0, 1, &viewport);

          VkRect2D scissor = { .offset = { .x = 0, .y = 0}, .extent = swapchain_extent };
          vkCmdSetScissor(commandbuf, 0, 1, &scissor);
          vkCmdBindDescriptorSets(commandbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1,
                                  &camera_tex_descriptor_set, 0, nullptr);
          vkCmdBindPipeline(commandbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
          VkBuffer vertexBuffers[] = {camera_vertex_buffer};
          VkDeviceSize offsets[] = {0};
          vkCmdBindVertexBuffers(commandbuf, 0, 1, vertexBuffers, offsets);

          vkCmdDraw(commandbuf, 4, 1, 0, 0);
          vkCmdEndRenderPass(commandbuf);
          fpEndCommandBuffer(commandbuf);
       }
       return true;
    }

   bool VulkanRenderer::create_instance()
   //------------------------------------
   {
      VkResult last_error = VK_SUCCESS;
      VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
      app.pApplicationName = "VulkanRenderer";
      app.applicationVersion = 0;
      app.pEngineName = "VulkanRenderer";
      app.engineVersion = 0;
      app.apiVersion = VK_API_VERSION_1_0;
#ifdef VK_USE_PLATFORM_ANDROID_KHR
      std::vector<const char*> instance_extensions = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME };
#elif VK_USE_PLATFORM_XCB_KHR
      std::vector<const char*> instance_extensions = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XCB_SURFACE_EXTENSION_NAME }; //, "VK_KHR_swapchain"};
#elif VK_USE_PLATFORM_XLIB_KHR
      std::vector<const char*> instance_extensions = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XLIB_SURFACE_EXTENSION_NAME }; //, "VK_KHR_swapchain"};
#elif VK_USE_PLATFORM_WIN32_KHR
      std::vector<const char*> instance_extensions = {"VK_KHR_surface", "VK_KHR_win32_surface", "VK_KHR_swapchain"};
#endif
#if !defined(NDEBUG)
      std::vector<const char*> instance_layers;
      debug_layers(instance_layers);
      if (instance_layers.size() > 0)
         instance_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif
      std::string missing_exts;
      if (! VulkanTools::check_extensions(instance_extensions, missing_exts))
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_instance",
                             "Some requested extensions are not supported: %s", missing_exts.c_str());
      VkInstanceCreateInfo instanceInfo = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
      instanceInfo.pApplicationInfo = &app;
      instanceInfo.enabledExtensionCount = static_cast<uint32_t>(instance_extensions.size());
      instanceInfo.ppEnabledExtensionNames = instance_extensions.data();
#if !defined(NDEBUG)
      instanceInfo.enabledLayerCount = static_cast<uint32_t>(instance_layers.size());
      instanceInfo.ppEnabledLayerNames = instance_layers.data();
#endif
      if ((last_error = vkCreateInstance(&instanceInfo, nullptr, &instance)) != VK_SUCCESS)
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_instance",
                             "Error creating Vulkan instance (vkCreateInstance %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
         return false;
      }
#if !defined(NDEBUG)
      setup_debug_callback();
#endif
      return true;
   }

   bool VulkanRenderer::get_physical_device(const VkPhysicalDeviceType &preferredType)
   //--------------------------------------------------------------------------------
   {
      uint32_t cdevices = 0;
      VkResult last_error;
      if ( ((last_error = vkEnumeratePhysicalDevices(instance, &cdevices, nullptr)) != VK_SUCCESS) || (cdevices == 0) )
      {
         vkDestroyInstance(instance, nullptr);
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::get_physical_device",
                             "Error enumerating physical devices or none found (vkEnumeratePhysicalDevices %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
         return false;
      }
      std::vector<VkPhysicalDevice> physicalDevices(cdevices);
      if (((last_error = vkEnumeratePhysicalDevices(instance, &cdevices, physicalDevices.data())) != VK_SUCCESS) ||
          (cdevices == 0))
      {
         vkDestroyInstance(instance, nullptr);
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::get_physical_device",
                             "Error enumerating physical devices or none found 2 (vkEnumeratePhysicalDevices %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
         return false;
      }
      if (physicalDevices.size() == 1)
         physical_device = physicalDevices[0];
      else
         physical_device = select_device(physicalDevices, preferredType);
      VkPhysicalDeviceProperties devProps;
      vkGetPhysicalDeviceProperties(physical_device, &devProps);
      std::stringstream surfaceFormatDesc;
      if (surface != VK_NULL_HANDLE)
      {
         std::vector<VkSurfaceFormatKHR> formats;
         std::unordered_map<VkFormat, size_t> surface_format_indices;
         if (VulkanTools::get_surface_formats(instance, physical_device, surface, formats, surface_format_indices,
                                              &surfaceFormatDesc) == 0)
         {
            __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::get_physical_device",
                                "Error obtaining surface formats (%s)", surfaceFormatDesc.str().c_str());
            vkDestroyInstance(instance, nullptr);
            return false;
         }
      }
      __android_log_print(ANDROID_LOG_INFO, "VulkanRenderer::get_physical_device",
                          "Selected Physical device %s %s API %d.%d Surface Formats:\n %s", devProps.deviceName,
                          VulkanTools::physical_device_typename(devProps.deviceType).c_str(),
                          VK_VERSION_MAJOR(devProps.apiVersion), VK_VERSION_MINOR(devProps.apiVersion),
                          surfaceFormatDesc.str().c_str());
      vkGetPhysicalDeviceFeatures(physical_device, &physical_device_features);
      return true;
   }

   VkPhysicalDevice VulkanRenderer::select_device(const std::vector<VkPhysicalDevice>& physicalDevices,
                                                  const VkPhysicalDeviceType preferredType)
   //---------------------------------------------------------------------------------------------------
   {
      auto fpGetSurfaceSupportKHR =
            reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(vkGetInstanceProcAddr(instance,
                                                                                             "vkGetPhysicalDeviceSurfaceSupportKHR"));
      auto cmp = [](std::pair<int, VkPhysicalDevice> left, std::pair<int, VkPhysicalDevice> right)
      { return (left.first < right.first); };
      std::priority_queue<std::pair<int, VkPhysicalDevice>, std::vector<std::pair<int, VkPhysicalDevice>>, decltype(cmp)>
            selected_devices(cmp);
      for (VkPhysicalDevice physicalDevice : physicalDevices)
      {
         VkPhysicalDeviceProperties devProps;
         vkGetPhysicalDeviceProperties(physical_device, &devProps);
         std::vector<VkSurfaceFormatKHR> formats;
         std::unordered_map<VkFormat, size_t> surface_format_indices;
         std::stringstream surfaceFormatDesc;
         if (VulkanTools::get_surface_formats(instance, physicalDevice, surface, formats, surface_format_indices,
                                              &surfaceFormatDesc) == 0)
            __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::select_device",
                                "Error obtaining surface formats for physical device %s (%s)", devProps.deviceName,
                                surfaceFormatDesc.str().c_str());

         else
             __android_log_print(ANDROID_LOG_INFO, "VulkanRenderer::select_device",
                                "Supported Surface formats for physical device %s %s", devProps.deviceName,
                                surfaceFormatDesc.str().c_str());
         uint32_t cfamilies = 0;
         vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &cfamilies, nullptr);
         if (cfamilies == 0) continue;
         std::vector<VkQueueFamilyProperties> queueFamilyProperties(cfamilies);
         vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &cfamilies, queueFamilyProperties.data());
         bool is_surface_ok = false;
         VkSurfaceCapabilitiesKHR surfaceCapabilities;
         auto fpGetPhysicalDeviceSurfaceCapabilitiesKHR =
               reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(vkGetInstanceProcAddr(instance,
                                                                                                     "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
         for (size_t i = 0; i < queueFamilyProperties.size(); i++)
         {
            VkBool32 vok;
            if ( (fpGetSurfaceSupportKHR) && (fpGetPhysicalDeviceSurfaceCapabilitiesKHR) &&
                (fpGetSurfaceSupportKHR(physicalDevice, i, surface, &vok) == VK_SUCCESS) && (vok == VK_TRUE) &&
                (fpGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities) == VK_SUCCESS)
               )
            {
               is_surface_ok = true;
               break;
            }
         }
         if (is_surface_ok)
         {
            int priority = 999;
            VkPhysicalDeviceProperties dev_props;
            vkGetPhysicalDeviceProperties(physicalDevice, &dev_props);
            __android_log_print(ANDROID_LOG_INFO, "VulkanRenderer::select_device",
                                "Candidate Physical device %s %s API %d.%d Formats %s", dev_props.deviceName,
                                 VulkanTools::physical_device_typename(dev_props.deviceType).c_str(),
                                 VK_VERSION_MAJOR(dev_props.apiVersion), VK_VERSION_MINOR(dev_props.apiVersion),
                                 surfaceFormatDesc.str().c_str());
            if (dev_props.deviceType == preferredType)
               priority = 0;
            else
            {
               switch (dev_props.deviceType)
               {
                  case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                     priority = 1;
                     break;
                  case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                     priority = 2;
                     break;
                  default:
                     priority = 3;
                     break;
               }
            }
            selected_devices.push(std::make_pair(priority, physicalDevice));
         }
      }
      if (selected_devices.size() == 0)
         return physicalDevices[0];
      return selected_devices.top().second;
   }

   bool VulkanRenderer::create_surface(void* nativeSurface, void *nativeConnection)
   //------------------------------------------------------------------------------
   {
      VkResult last_error;
#if defined(__ANDROID__)
      auto fpCreateAndroidSurfaceKHR =
            reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(vkGetInstanceProcAddr(instance, "vkCreateAndroidSurfaceKHR"));
      if (!fpCreateAndroidSurfaceKHR)
      {
         last_error = VK_ERROR_INCOMPATIBLE_DISPLAY_KHR;
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_surface", "Error getting function pointer for vkCreateAndroidSurfaceKHR");
         return false;
      }
      VkAndroidSurfaceCreateInfoKHR surfinfo = {VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};
      ANativeWindow* asurface = reinterpret_cast<ANativeWindow*>(nativeSurface);
      surfinfo.window = asurface;
      if ((last_error = fpCreateAndroidSurfaceKHR(instance, &surfinfo, nullptr, &surface)) != VK_SUCCESS)
      {
         vkDestroyInstance(instance, nullptr);
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_surface",
                             "Error creating Android surface using supplied Android surface (VkAndroidSurfaceCreateInfoKHR %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
         return false;
      }
#endif
#ifdef VK_USE_PLATFORM_XCB_KHR
      auto fpCreateXcbSurfaceKHR =
            reinterpret_cast<PFN_vkCreateXcbSurfaceKHR>(vkGetInstanceProcAddr(instance, "vkCreateXcbSurfaceKHR"));
      VkXcbSurfaceCreateInfoKHR surfinfo = {VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR};
      xcb_window_t* pwindow = reinterpret_cast<xcb_window_t*>(nativeSurface);
      surfinfo.window = *pwindow;
      xcb_connection_t* pconnection = reinterpret_cast<xcb_connection_t*>(nativeConnection);
      surfinfo.connection = pconnection;
      if ((last_error = fpCreateXcbSurfaceKHR(instance, &surfinfo, nullptr, &surface)) != VK_SUCCESS)
      {
         vkDestroyInstance(instance, nullptr);
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_surface",
                             "Error creating XCB surface using supplied window (VkAndroidSurfaceCreateInfoKHR %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
         return false;
      }
#endif
#ifdef VK_USE_PLATFORM_XLIB_KHR
      auto fpCreateXlibSurfaceKHR =
            reinterpret_cast<PFN_vkCreateXlibSurfaceKHR>(vkGetInstanceProcAddr(instance, "vkCreateXlibSurfaceKHR"));
      VkXlibSurfaceCreateInfoKHR surfinfo = { VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR };
      Window* pwindow = reinterpret_cast<Window*>(nativeSurface);
      surfinfo.window = *pwindow;
      Display* pconnection = reinterpret_cast<Display*>(nativeConnection);
      surfinfo.dpy = pconnection;
      if ((last_error = fpCreateXlibSurfaceKHR(instance, &surfinfo, nullptr, &surface)) != VK_SUCCESS)
      {
         vkDestroyInstance(instance, nullptr);
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_surface",
                             "Error creating XCB surface using supplied window (VkAndroidSurfaceCreateInfoKHR %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
         return false;
      }
#endif
#ifdef VK_USE_PLATFORM_WIN32_KHR
      __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::create_surface", "Please implement me!!!");
      return false;
#endif
      return true;
   }

   bool VulkanRenderer::get_queue_families()
   //-------------------------------------
   {
      uint32_t queueFamilyCount;
      vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queueFamilyCount, nullptr);
      if (queueFamilyCount == 0)
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::get_queue_families",
                             "No queue families found for selected physical device (vkGetPhysicalDeviceQueueFamilyProperties)");
         return false;
      }
      std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
      vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queueFamilyCount, queueFamilyProperties.data());
      const uint32_t flags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
      for (uint32_t i = 0; i < queueFamilyCount; i++)
      {
         if (queueFamilyProperties[i].queueCount == 0) continue;
         if ((graphics_queuefamily_index == UINT32_MAX) && ((queueFamilyProperties[i].queueFlags & flags) == flags))
            graphics_queuefamily_index = i;
         if (present_queuefamily_index == UINT32_MAX)
         {
            VkBool32 surfaceSupported = 0;
            PFN_vkGetPhysicalDeviceSurfaceSupportKHR fpGetPhysicalDeviceSurfaceSupportKHR =
                  reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(vkGetInstanceProcAddr(instance,
                                                                             "vkGetPhysicalDeviceSurfaceSupportKHR"));
            VkResult res = fpGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &surfaceSupported);
            if ((res >= 0) && (surfaceSupported == VK_TRUE))
               present_queuefamily_index = i;
         }
         if ((graphics_queuefamily_index != UINT32_MAX) && (present_queuefamily_index != UINT32_MAX))
            break;
      }
      if (graphics_queuefamily_index >= queueFamilyCount)
      {
          __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::get_queue_families",
                              "No queue family supporting graphics found in queue family (vkGetPhysicalDeviceQueueFamilyProperties count: %d)",
               queueFamilyCount);
         return false;
      }

      return true;
   }

   void VulkanRenderer::get_instance_function_pointers()
   //---------------------------------------------------
   {
      fpAcquireNextImageKHR =
            reinterpret_cast<PFN_vkAcquireNextImageKHR>(vkGetInstanceProcAddr(instance, "vkAcquireNextImageKHR"));
      fpQueuePresentKHR  =
            reinterpret_cast<PFN_vkQueuePresentKHR>(vkGetInstanceProcAddr(instance, "vkQueuePresentKHR"));
      fpWaitForFences  =
            reinterpret_cast<PFN_vkWaitForFences>(vkGetInstanceProcAddr(instance, "vkWaitForFences"));
      fpResetFences =
            reinterpret_cast<PFN_vkResetFences>(vkGetInstanceProcAddr(instance, "vkResetFences"));
      fpQueueSubmit =
            reinterpret_cast<PFN_vkQueueSubmit>(vkGetInstanceProcAddr(instance, "vkQueueSubmit"));
      fpCmdPipelineBarrier =
            reinterpret_cast<PFN_vkCmdPipelineBarrier>(vkGetInstanceProcAddr(instance, "vkCmdPipelineBarrier"));
      fpCmdCopyBufferToImage =
            reinterpret_cast<PFN_vkCmdCopyBufferToImage>(vkGetInstanceProcAddr(instance, "vkCmdCopyBufferToImage"));
      fpBeginCommandBuffer =
            reinterpret_cast<PFN_vkBeginCommandBuffer>(vkGetInstanceProcAddr(instance, "vkBeginCommandBuffer"));
      fpEndCommandBuffer =
            reinterpret_cast<PFN_vkEndCommandBuffer>(vkGetInstanceProcAddr(instance, "vkEndCommandBuffer"));
      fpQueueWaitIdle =
            reinterpret_cast<PFN_vkQueueWaitIdle>(vkGetInstanceProcAddr(instance, "vkQueueWaitIdle"));
   }

   void VulkanRenderer::destroy_framebuffer()
   //---------------------------------------
   {
      for (uint32_t i = 0; i < swapchain_len; i++)
      {
         VkImage img = swapchain_images[i];
         vkDestroyImage(device, img, nullptr);
         VkImageView imvw = swapchain_views[i];
         vkDestroyImageView(device, imvw, nullptr);
         VkFramebuffer buf = swapchain_framebuffers[i];
         vkDestroyFramebuffer(device, buf, nullptr);
      }
      swapchain_images.clear();
      swapchain_views.clear();
      swapchain_framebuffers.clear();
   }

#if !defined(NDEBUG)

   void VulkanRenderer::debug_layers(std::vector<const char*>& instance_layers)
   //----------------------------------------------------------------------------
   {
      uint32_t clayers = 0;
      VkResult last_error = VK_SUCCESS;
      instance_layers.clear();
      if ((last_error = vkEnumerateInstanceLayerProperties(&clayers, nullptr)) != VK_SUCCESS)
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::debug_layers",
                             "Error in vkEnumerateInstanceLayerProperties (%d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
         return;
      }
      if (clayers == 0)
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::debug_layers",
               "VulkanRenderer::debug_layers: vkEnumerateInstanceLayerProperties returned 0; Change build.gradle to include layers "
               "eg sourceSets {main jniLibs.srcDirs = [String.valueOf(System.getenv(\"NDK_ROOT\")) + \"/sources/third_party/vulkan/src/build-android/jniLibs\""
               " (see https://developer.android.com/ndk/guides/graphics/validation-layer)");
         return;
      }
      instance_layers = {"VK_LAYER_GOOGLE_threading", "VK_LAYER_LUNARG_parameter_validation",
                         "VK_LAYER_LUNARG_object_tracker", "VK_LAYER_LUNARG_core_validation",
                         "VK_LAYER_GOOGLE_unique_objects" }; //,
//                         "VK_LAYER_RENDERDOC_Capture"};
      std::vector<VkLayerProperties> supported_layers(clayers);
      vkEnumerateInstanceLayerProperties(&clayers, supported_layers.data());
      std::unordered_set<std::string> supported_layer_set;
      for (const VkLayerProperties& supported_layer : supported_layers)
      {
         __android_log_print(ANDROID_LOG_INFO, "VulkanRenderer::debug_layers",
                             "Supported debug layer: %s", supported_layer.layerName);
         supported_layer_set.emplace(supported_layer.layerName);
      }
      for (auto it = instance_layers.begin(); it != instance_layers.end(); ++it)
      {
         const char* extname = *it;
         std::string s = extname;
         if  (supported_layer_set.find(s) == supported_layer_set.end())
         {
            it = instance_layers.erase(it);
             __android_log_print(ANDROID_LOG_WARN, "VulkanRenderer::debug_layers","Debug layer %s not found", extname);
         }
      }
   }

   void VulkanRenderer::setup_debug_callback()
   //-----------------------------------------
   {
      PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT =
            reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance,
                                                                                       "vkCreateDebugReportCallbackEXT"));
      if (vkCreateDebugReportCallbackEXT == nullptr)
      {
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::setup_debug_callback",
                             "Error getting function pointer for vkCreateDebugReportCallbackEXT");
         return;
      }
      VkDebugReportCallbackCreateInfoEXT debugReportCallbackCreateInfo = {
            VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT};
      debugReportCallbackCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT |
                                            VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
      debugReportCallbackCreateInfo.pfnCallback = &VulkanRenderer::debug_report_callback;
      VkResult last_error = VK_SUCCESS;
      if ((last_error = vkCreateDebugReportCallbackEXT(instance, &debugReportCallbackCreateInfo, nullptr,
                                                       &debug_report)) != VK_SUCCESS)
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::setup_debug_callback",
                             "Error creating debug callback (vkCreateDebugReportCallbackEXT %d %s)",
                             last_error, VulkanTools::result_string(last_error).c_str());
   }

   VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRenderer::debug_report_callback(VkDebugReportFlagsEXT msgFlags,
                                                                        VkDebugReportObjectTypeEXT objType,
                                                                        uint64_t srcObject, size_t location,
                                                                        int32_t msgCode,
                                                                        const char* pLayerPrefix, const char* pMsg,
                                                                        void* pUserData)
   //---------------------------------------------------------------------------------------------------------------------------------
   {
      if (msgFlags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
         __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer::debug_report_callback",
                             "ERROR: [%s] Code %d : %s", pLayerPrefix, msgCode, pMsg);
      else if (msgFlags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
         __android_log_print(ANDROID_LOG_WARN, "VulkanRenderer::debug_report_callback",
                             "WARNING: [%s] Code %d : %s", pLayerPrefix, msgCode, pMsg);
      else if (msgFlags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
         __android_log_print(ANDROID_LOG_WARN, "VulkanRenderer::debug_report_callback",
                             "PERFORMANCE WARNING: [%s] Code %d : %s", pLayerPrefix, msgCode, pMsg);
      else if (msgFlags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
         __android_log_print(ANDROID_LOG_INFO, "VulkanRenderer::debug_report_callback",
                             "INFO: [%s] Code %d : %s", pLayerPrefix, msgCode, pMsg);
      else if (msgFlags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
         __android_log_print(ANDROID_LOG_INFO, "VulkanRenderer::debug_report_callback",
                             "DEBUG: [%s] Code %d : %s", pLayerPrefix, msgCode, pMsg);
      return VK_FALSE;
   }

   void VulkanRenderer::tex_pattern(uint32_t w, uint32_t h, void* data)
   //------------------------------------------------------------------
   {
      char* pImageData = static_cast<char*>(data);
      uint8_t* pRowData = (uint8_t*)pImageData;
      for(uint32_t y = 0; y < w; ++y)
      {
         uint32_t* pPixelData = (uint32_t*)pRowData;
         for(uint32_t x = 0; x < h; ++x)
         {
            *pPixelData =
                  ((x & 0x18) == 0x08 ? 0x000000FF : 0x00000000) |
                  ((x & 0x18) == 0x10 ? 0x0000FFFF : 0x00000000) |
                  ((y & 0x18) == 0x08 ? 0x0000FF00 : 0x00000000) |
                  ((y & 0x18) == 0x10 ? 0x00FF0000 : 0x00000000);
            ++pPixelData;
         }
         pRowData += h * 4;
      }
   }
#endif

   void VulkanRenderer::destroy(bool isRenderOnly)
   //---------------------------------------------
   {
      if (device != VK_NULL_HANDLE)
         vkDeviceWaitIdle(device);
      if ((device != VK_NULL_HANDLE) && (descriptor_pool != VK_NULL_HANDLE) &&
          (camera_tex_descriptor_set != VK_NULL_HANDLE))
         vkFreeDescriptorSets(device, descriptor_pool, 1, &camera_tex_descriptor_set);
      camera_tex_descriptor_set = VK_NULL_HANDLE;
      if ((device != VK_NULL_HANDLE) && (descriptor_pool != VK_NULL_HANDLE))
         vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
      descriptor_pool = VK_NULL_HANDLE;

      for (VkCommandBuffer buffer : command_buffers)
      {
         if ((device != VK_NULL_HANDLE) && (buffer != VK_NULL_HANDLE))
            vkFreeCommandBuffers(device, command_pool, 1, &buffer);
      }
      if ((vma_allocator != VK_NULL_HANDLE) && (staging_buffer != VK_NULL_HANDLE))
         vmaDestroyBuffer(vma_allocator, staging_buffer, staging_alloc);
      if ((device != VK_NULL_HANDLE) && (one_time_buffer != VK_NULL_HANDLE))
         vkFreeCommandBuffers(device, command_pool, 1, &one_time_buffer);
      one_time_buffer = VK_NULL_HANDLE;
      command_buffers.resize(0);
      if ((device != VK_NULL_HANDLE) && (pipeline_layout != VK_NULL_HANDLE))
         vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
      pipeline_layout = VK_NULL_HANDLE;
      if ((device != VK_NULL_HANDLE) && (pipeline != VK_NULL_HANDLE))
         vkDestroyPipeline(device, pipeline, nullptr);
      pipeline = VK_NULL_HANDLE;
      if ((device != VK_NULL_HANDLE) && (camera_texture_sampler != VK_NULL_HANDLE))
         vkDestroySampler(device, camera_texture_sampler, nullptr);
      camera_texture_sampler = VK_NULL_HANDLE;
      if ((device != VK_NULL_HANDLE) && (depth_image_view != VK_NULL_HANDLE))
         vkDestroyImageView(device, depth_image_view, nullptr);
      depth_image_view = VK_NULL_HANDLE;
      if ((device != VK_NULL_HANDLE) && (depth_image != VK_NULL_HANDLE))
         vkDestroyImage(device, depth_image, nullptr);
      depth_image = VK_NULL_HANDLE;
      clear_command_buffers();
      destroy_framebuffer();
      for (size_t i = 0; i < swapchain_views.size(); i++)
      {
         if ((device != VK_NULL_HANDLE) && (swapchain_views[i] != VK_NULL_HANDLE))
            vkDestroyImageView(device, swapchain_views[i], nullptr);
         if ((device != VK_NULL_HANDLE) && (swapchain_images[i] != VK_NULL_HANDLE))
            vkDestroyImage(device, swapchain_images[i], nullptr);
      }
      swapchain_views.resize(0);
      swapchain_images.resize(0);
      if ((device != VK_NULL_HANDLE) && (swapchain != VK_NULL_HANDLE))
      {
         PFN_vkDestroySwapchainKHR fpDestroySwapchainKHR =
               reinterpret_cast<PFN_vkDestroySwapchainKHR>(vkGetInstanceProcAddr(instance, "vkDestroySwapchainKHR"));
         fpDestroySwapchainKHR(device, swapchain, nullptr);
      }
      swapchain = VK_NULL_HANDLE;
      if ((device != VK_NULL_HANDLE) && (command_pool != VK_NULL_HANDLE))
         vkDestroyCommandPool(device, command_pool, nullptr);
      command_pool = VK_NULL_HANDLE;
      if (! isRenderOnly)
      {
#if !defined(NDEBUG)
         PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT =
               reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance,
                                                                                           "vkDestroyDebugReportCallbackEXT"));
         if ((instance != VK_NULL_HANDLE) && (vkDestroyDebugReportCallbackEXT))
            vkDestroyDebugReportCallbackEXT(instance, debug_report, nullptr);
#endif
         if (device != VK_NULL_HANDLE)
            vkDestroyDevice(device, nullptr);
         device = VK_NULL_HANDLE;
         if (instance != VK_NULL_HANDLE)
            vkDestroyInstance(instance, nullptr);
         instance = VK_NULL_HANDLE;
         if (vma_allocator != VK_NULL_HANDLE)
            vmaDestroyAllocator(vma_allocator);
         vma_allocator = VK_NULL_HANDLE;
      }
   }

   void VulkanRenderer::clear_command_buffers()
   //------------------------------------------
   {
      for (VkCommandBuffer buf : command_buffers)
         vkFreeCommandBuffers(device, command_pool, 1, &buf);
      command_buffers.clear();
      for (VkFence fence : camera_fences)
      {
         vkWaitForFences(device, 1, &fence, VK_TRUE, 100000000);
         vkDestroyFence(device, fence, nullptr);
      }
      camera_fences.clear();
   }

   uint32_t
   VulkanRenderer::find_memory_prop(uint32_t memoryTypeBits, VkMemoryPropertyFlags properties)
   //----------------------------------------------------------------------------------------
   {
      VkPhysicalDeviceMemoryProperties phys_props;
      vkGetPhysicalDeviceMemoryProperties(physical_device, &phys_props);
      for (uint32_t i = 0; i < phys_props.memoryTypeCount; ++i)
      {
         if ((memoryTypeBits & (1 << i)) &&
             ((phys_props.memoryTypes[i].propertyFlags & properties) == properties))
            return i;
      }
      return std::numeric_limits<uint32_t>::max();
   }

   VulkanRenderer::VulkanRenderer(const char *appName, const char *assetsDir, bool isShowFPS) :
            Renderer(appName, assetsDir, isShowFPS), shadersAssetsDir(assetDir + "/shaders")
   //-----------------------------------------------------------------------------------------
   {
      char pch[512];
      sprintf(pch, "VulkanRenderer@%p", this);
      renderer_name = new char[strlen(pch)+1];
      strcpy(renderer_name, pch);
   }
}
