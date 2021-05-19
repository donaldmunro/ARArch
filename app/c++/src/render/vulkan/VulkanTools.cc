#include <vector>
#include <cstring>
#include <sstream>

#include "mar/render/VulkanTools.h"

PFN_vkAcquireNextImageKHR VulkanTools::fpAcquireNextImageKHR = nullptr;
PFN_vkQueuePresentKHR VulkanTools::fpQueuePresentKHR = nullptr;
PFN_vkWaitForFences VulkanTools::fpWaitForFences = nullptr;
PFN_vkResetFences VulkanTools::fpResetFences = nullptr;
PFN_vkQueueSubmit VulkanTools::fpQueueSubmit = nullptr;
PFN_vkCmdPipelineBarrier VulkanTools::fpCmdPipelineBarrier = nullptr;
PFN_vkCmdCopyBufferToImage VulkanTools::fpCmdCopyBufferToImage = nullptr;
PFN_vkBeginCommandBuffer VulkanTools::fpBeginCommandBuffer = nullptr;
PFN_vkEndCommandBuffer VulkanTools::fpEndCommandBuffer = nullptr;
PFN_vkQueueWaitIdle VulkanTools::fpQueueWaitIdle = nullptr;

bool VulkanTools::init(const VkInstance& instance)
{
   VulkanTools::fpAcquireNextImageKHR =
         reinterpret_cast<PFN_vkAcquireNextImageKHR>(vkGetInstanceProcAddr(instance, "vkAcquireNextImageKHR"));
   VulkanTools::fpQueuePresentKHR  =
         reinterpret_cast<PFN_vkQueuePresentKHR>(vkGetInstanceProcAddr(instance, "vkQueuePresentKHR"));
   VulkanTools::fpWaitForFences  =
         reinterpret_cast<PFN_vkWaitForFences>(vkGetInstanceProcAddr(instance, "vkWaitForFences"));
   VulkanTools::fpResetFences =
         reinterpret_cast<PFN_vkResetFences>(vkGetInstanceProcAddr(instance, "vkResetFences"));
   VulkanTools::fpQueueSubmit =
         reinterpret_cast<PFN_vkQueueSubmit>(vkGetInstanceProcAddr(instance, "vkQueueSubmit"));
   VulkanTools::fpCmdPipelineBarrier =
         reinterpret_cast<PFN_vkCmdPipelineBarrier>(vkGetInstanceProcAddr(instance, "vkCmdPipelineBarrier"));
   VulkanTools::fpCmdCopyBufferToImage =
         reinterpret_cast<PFN_vkCmdCopyBufferToImage>(vkGetInstanceProcAddr(instance, "vkCmdCopyBufferToImage"));
   VulkanTools::fpBeginCommandBuffer =
         reinterpret_cast<PFN_vkBeginCommandBuffer>(vkGetInstanceProcAddr(instance, "vkBeginCommandBuffer"));
   VulkanTools::fpEndCommandBuffer =
         reinterpret_cast<PFN_vkEndCommandBuffer>(vkGetInstanceProcAddr(instance, "vkEndCommandBuffer"));
   VulkanTools::fpQueueWaitIdle =
         reinterpret_cast<PFN_vkQueueWaitIdle>(vkGetInstanceProcAddr(instance, "vkQueueWaitIdle"));
   return true;
}

VkFormat VulkanTools::find_supported_format(const VkPhysicalDevice& physicalDevice, const std::vector<VkFormat>& candidates,
                                            VkImageTiling tiling, VkFormatFeatureFlags features)
//--------------------------------------------------------------------------------------------------------
{
   for (VkFormat format : candidates)
   {
      VkFormatProperties props;
      vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

      if ( (tiling == VK_IMAGE_TILING_LINEAR) && ((props.linearTilingFeatures & features) == features) )
         return format;
      else
      {
         if ( (tiling == VK_IMAGE_TILING_OPTIMAL) && ((props.optimalTilingFeatures & features) == features) )
            return format;
      }
   }
   return VK_FORMAT_UNDEFINED;
}

bool VulkanTools::make_extension_map(std::unordered_set<std::string> extension_map)
//-------------------------------------------------------------------
{
   uint32_t no = 0;
   if ( (vkEnumerateInstanceExtensionProperties(nullptr, &no, nullptr) != VK_SUCCESS) || (no == 0) )
      return false;
   std::vector<VkExtensionProperties> extensions(no);
   if (vkEnumerateInstanceExtensionProperties(nullptr, &no, extensions.data()) != VK_SUCCESS)
      return false;
   for (VkExtensionProperties ext : extensions)
      extension_map.emplace(ext.extensionName);
   return (no > 0);
}

void VulkanTools::record_barrier(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout oldImageLayout,
                                    VkImageLayout newImageLayout, VkPipelineStageFlags srcStages, VkPipelineStageFlags destStages)
//----------------------------------------------------------------------------
{
   VkImageMemoryBarrier imageMemoryBarrier =
   {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .pNext = NULL,
      .srcAccessMask = 0,
      .dstAccessMask = 0,
      .oldLayout = oldImageLayout,
      .newLayout = newImageLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange =
         {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
         },
   };

   switch (oldImageLayout)
   {
      case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
         imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
           break;

      case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
         imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
           break;

      case VK_IMAGE_LAYOUT_PREINITIALIZED:
         imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
           break;

//         case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: ??

      default:break;
   }

   switch (newImageLayout)
   {
      case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
         imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
           break;

      case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
         imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
           break;

      case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
         imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
           break;

      case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
         imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
           break;

      case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
         imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
           break;

      default:break;
   }

   fpCmdPipelineBarrier(cmdBuffer, srcStages, destStages, 0, 0, NULL, 0, NULL, 1,
                        &imageMemoryBarrier);
}

std::string VulkanTools::physical_device_typename(const VkPhysicalDeviceType& type)
//--------------------------------------------------------------------------------
{
   switch (type)
   {
      case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
         return "Discrete";
      case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
         return "Integrated";
      case VK_PHYSICAL_DEVICE_TYPE_CPU:
         return "CPU";
      case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
         return "Virtual";
   }
   return "Unknown";
}

bool VulkanTools::check_extensions(std::vector<const char*>& instance_extensions,
                                   std::string& missing_exts)
//-------------------------------------------------------------------------------
{
   std::stringstream ss;
   uint32_t cexts = 0;
   vkEnumerateInstanceExtensionProperties(nullptr, &cexts, nullptr);
   std::vector<VkExtensionProperties> supported_exts(cexts);
   vkEnumerateInstanceExtensionProperties(nullptr, &cexts, supported_exts.data());
   for (auto it = instance_extensions.begin(); it != instance_extensions.end(); ++it)
   {
      const char* extname = *it;
      bool ok = false;
      for (const VkExtensionProperties& supported_ext : supported_exts)
      {
         if (strcmp(extname, supported_ext.extensionName) == 0)
         {
            ok = true;
            break;
         }
      }
      if (!ok)
      {
         it = instance_extensions.erase(it);
         if (! ss.str().empty()) ss << ", ";
         ss << extname;
      }
   }
   missing_exts = ss.str();
   return (missing_exts.empty());
}

std::string VulkanTools::result_string(VkResult result)
//---------------------------------------------------
{
   switch(result)
   {
      case VK_SUCCESS:
         return "VK_SUCCESS";
      case VK_NOT_READY:
         return "VK_NOT_READY";
      case VK_TIMEOUT:
         return "VK_TIMEOUT";
      case VK_EVENT_SET:
         return "VK_EVENT_SET";
      case VK_EVENT_RESET:
         return "VK_EVENT_RESET";
      case VK_INCOMPLETE:
         return "VK_INCOMPLETE";
      case VK_ERROR_OUT_OF_HOST_MEMORY:
         return "VK_ERROR_OUT_OF_HOST_MEMORY";
      case VK_ERROR_OUT_OF_DEVICE_MEMORY:
         return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
      case VK_ERROR_INITIALIZATION_FAILED:
         return "VK_ERROR_INITIALIZATION_FAILED";
      case VK_ERROR_DEVICE_LOST:
         return "VK_ERROR_DEVICE_LOST";
      case VK_ERROR_MEMORY_MAP_FAILED:
         return "VK_ERROR_MEMORY_MAP_FAILED";
      case VK_ERROR_LAYER_NOT_PRESENT:
         return "VK_ERROR_LAYER_NOT_PRESENT";
      case VK_ERROR_EXTENSION_NOT_PRESENT:
         return "VK_ERROR_EXTENSION_NOT_PRESENT";
      case VK_ERROR_FEATURE_NOT_PRESENT:
         return "VK_ERROR_FEATURE_NOT_PRESENT";
      case VK_ERROR_INCOMPATIBLE_DRIVER:
         return "VK_ERROR_INCOMPATIBLE_DRIVER";
      case VK_ERROR_TOO_MANY_OBJECTS:
         return "VK_ERROR_TOO_MANY_OBJECTS";
      case VK_ERROR_FORMAT_NOT_SUPPORTED:
         return "VK_ERROR_FORMAT_NOT_SUPPORTED";
      case VK_ERROR_FRAGMENTED_POOL:
         return "VK_ERROR_FRAGMENTED_POOL";
      case VK_ERROR_SURFACE_LOST_KHR:
         return "VK_ERROR_SURFACE_LOST_KHR";
      case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
         return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
      case VK_SUBOPTIMAL_KHR:
         return "VK_SUBOPTIMAL_KHR";
      case VK_ERROR_OUT_OF_DATE_KHR:
         return "VK_ERROR_OUT_OF_DATE_KHR";
      case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
         return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
      case VK_ERROR_VALIDATION_FAILED_EXT:
         return "VK_ERROR_VALIDATION_FAILED_EXT";
      case VK_ERROR_OUT_OF_POOL_MEMORY_KHR:
         return "VK_ERROR_OUT_OF_POOL_MEMORY_KHR";
      case VK_ERROR_INVALID_SHADER_NV:
         return "VK_ERROR_INVALID_SHADER_NV";
      case VK_ERROR_NOT_PERMITTED_EXT:
         return "VK_ERROR_NOT_PERMITTED_EXT";
      case VK_ERROR_FRAGMENTATION_EXT:
         return "VK_ERROR_FRAGMENTATION_EXT";
      case VK_ERROR_INVALID_EXTERNAL_HANDLE:
         return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
      case VK_RESULT_MAX_ENUM:
//      case VK_RESULT_RANGE_SIZE:
         break;
   }
   if(result < 0)
      return "VK_ERROR_<Unknown>";
   return "VK_<Unknown>";
}

void VulkanTools::set_image_layout(VkCommandBuffer buffer, VkImage image, VkImageAspectFlags aspectMask,
                                   VkImageLayout old_image_layout, VkImageLayout new_image_layout,
                                   VkAccessFlagBits srcAccessMask, VkPipelineStageFlags src_stages,
                                   VkPipelineStageFlags dest_stages)
//---------------------------------------------------------------------------------------------------------------
{
   VkImageMemoryBarrier image_memory_barrier =
   {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = srcAccessMask, .dstAccessMask = 0,
      .oldLayout = old_image_layout, .newLayout = new_image_layout,
      .image = image, .subresourceRange = {aspectMask, 0, 1, 0, 1}
   };
   switch (new_image_layout)
   {
      case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
         image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
         break;

      case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
         image_memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
         break;

      case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
         image_memory_barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
         break;

      case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
         image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
         break;

      case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
         image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
         break;

      case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
         image_memory_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
         break;

      default:
         image_memory_barrier.dstAccessMask = 0;
         break;
   }
   fpCmdPipelineBarrier(buffer, src_stages, dest_stages, 0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);
}

uint32_t VulkanTools::get_surface_formats(const VkInstance instance,  const VkPhysicalDevice physical_device,
                                          const VkSurfaceKHR surface, std::vector<VkSurfaceFormatKHR>& formats,
                                          std::unordered_map<VkFormat, size_t>& surface_format_indices,
                                          std::stringstream* out)
//-------------------------------------------------------------------------------------------------------------
{
   uint32_t cformats = 0;
   auto fpGetPhysicalDeviceSurfaceFormatsKHR =
         reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(vkGetInstanceProcAddr(instance,
                                                                                          "vkGetPhysicalDeviceSurfaceFormatsKHR"));
   fpGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &cformats, nullptr);
   formats.resize(cformats);
   VkResult last_error;
   if ((last_error = fpGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &cformats, formats.data())) !=
       VK_SUCCESS)
   {
      if (out)
         *out <<"Error getting surface formats (vkGetPhysicalDeviceSurfaceCapabilitiesKHR " << last_error << ":"
              << VulkanTools::result_string(last_error);
      return 0;
   }
   for (size_t i = 0; i < cformats; i++)
   {
      VkFormat format = formats[i].format;
      if (out)
         *out << "Surface format " <<  VulkanTools::surface_format_string(format) << " supported\n";
      surface_format_indices[format] = i;
   }
   return cformats;
}

std::string VulkanTools::surface_format_string(const VkFormat format)
{
   switch (format)
   {
      case VK_FORMAT_UNDEFINED :
         return "VK_FORMAT_UNDEFINED";
      case VK_FORMAT_R4G4_UNORM_PACK8 :
         return "VK_FORMAT_R4G4_UNORM_PACK8";
      case VK_FORMAT_R4G4B4A4_UNORM_PACK16 :
         return "VK_FORMAT_R4G4B4A4_UNORM_PACK16";
      case VK_FORMAT_B4G4R4A4_UNORM_PACK16 :
         return "VK_FORMAT_B4G4R4A4_UNORM_PACK16";
      case VK_FORMAT_R5G6B5_UNORM_PACK16 :
         return "VK_FORMAT_R5G6B5_UNORM_PACK16";
      case VK_FORMAT_B5G6R5_UNORM_PACK16 :
         return "VK_FORMAT_B5G6R5_UNORM_PACK16";
      case VK_FORMAT_R5G5B5A1_UNORM_PACK16 :
         return "VK_FORMAT_R5G5B5A1_UNORM_PACK16";
      case VK_FORMAT_B5G5R5A1_UNORM_PACK16 :
         return "VK_FORMAT_B5G5R5A1_UNORM_PACK16";
      case VK_FORMAT_A1R5G5B5_UNORM_PACK16 :
         return "VK_FORMAT_A1R5G5B5_UNORM_PACK16";
      case VK_FORMAT_R8_UNORM :
         return "VK_FORMAT_R8_UNORM";
      case VK_FORMAT_R8_SNORM :
         return "VK_FORMAT_R8_SNORM";
      case VK_FORMAT_R8_USCALED :
         return "VK_FORMAT_R8_USCALED";
      case VK_FORMAT_R8_SSCALED :
         return "VK_FORMAT_R8_SSCALED";
      case VK_FORMAT_R8_UINT :
         return "VK_FORMAT_R8_UINT";
      case VK_FORMAT_R8_SINT :
         return "VK_FORMAT_R8_SINT";
      case VK_FORMAT_R8_SRGB :
         return "VK_FORMAT_R8_SRGB";
      case VK_FORMAT_R8G8_UNORM :
         return "VK_FORMAT_R8G8_UNORM";
      case VK_FORMAT_R8G8_SNORM :
         return "VK_FORMAT_R8G8_SNORM";
      case VK_FORMAT_R8G8_USCALED :
         return "VK_FORMAT_R8G8_USCALED";
      case VK_FORMAT_R8G8_SSCALED :
         return "VK_FORMAT_R8G8_SSCALED";
      case VK_FORMAT_R8G8_UINT :
         return "VK_FORMAT_R8G8_UINT";
      case VK_FORMAT_R8G8_SINT :
         return "VK_FORMAT_R8G8_SINT";
      case VK_FORMAT_R8G8_SRGB :
         return "VK_FORMAT_R8G8_SRGB";
      case VK_FORMAT_R8G8B8_UNORM :
         return "VK_FORMAT_R8G8B8_UNORM";
      case VK_FORMAT_R8G8B8_SNORM :
         return "VK_FORMAT_R8G8B8_SNORM";
      case VK_FORMAT_R8G8B8_USCALED :
         return "VK_FORMAT_R8G8B8_USCALED";
      case VK_FORMAT_R8G8B8_SSCALED :
         return "VK_FORMAT_R8G8B8_SSCALED";
      case VK_FORMAT_R8G8B8_UINT :
         return "VK_FORMAT_R8G8B8_UINT";
      case VK_FORMAT_R8G8B8_SINT :
         return "VK_FORMAT_R8G8B8_SINT";
      case VK_FORMAT_R8G8B8_SRGB :
         return "VK_FORMAT_R8G8B8_SRGB";
      case VK_FORMAT_B8G8R8_UNORM :
         return "VK_FORMAT_B8G8R8_UNORM";
      case VK_FORMAT_B8G8R8_SNORM :
         return "VK_FORMAT_B8G8R8_SNORM";
      case VK_FORMAT_B8G8R8_USCALED :
         return "VK_FORMAT_B8G8R8_USCALED";
      case VK_FORMAT_B8G8R8_SSCALED :
         return "VK_FORMAT_B8G8R8_SSCALED";
      case VK_FORMAT_B8G8R8_UINT :
         return "VK_FORMAT_B8G8R8_UINT";
      case VK_FORMAT_B8G8R8_SINT :
         return "VK_FORMAT_B8G8R8_SINT";
      case VK_FORMAT_B8G8R8_SRGB :
         return "VK_FORMAT_B8G8R8_SRGB";
      case VK_FORMAT_R8G8B8A8_UNORM :
         return "VK_FORMAT_R8G8B8A8_UNORM";
      case VK_FORMAT_R8G8B8A8_SNORM :
         return "VK_FORMAT_R8G8B8A8_SNORM";
      case VK_FORMAT_R8G8B8A8_USCALED :
         return "VK_FORMAT_R8G8B8A8_USCALED";
      case VK_FORMAT_R8G8B8A8_SSCALED :
         return "VK_FORMAT_R8G8B8A8_SSCALED";
      case VK_FORMAT_R8G8B8A8_UINT :
         return "VK_FORMAT_R8G8B8A8_UINT";
      case VK_FORMAT_R8G8B8A8_SINT :
         return "VK_FORMAT_R8G8B8A8_SINT";
      case VK_FORMAT_R8G8B8A8_SRGB :
         return "VK_FORMAT_R8G8B8A8_SRGB";
      case VK_FORMAT_B8G8R8A8_UNORM :
         return "VK_FORMAT_B8G8R8A8_UNORM";
      case VK_FORMAT_B8G8R8A8_SNORM :
         return "VK_FORMAT_B8G8R8A8_SNORM";
      case VK_FORMAT_B8G8R8A8_USCALED :
         return "VK_FORMAT_B8G8R8A8_USCALED";
      case VK_FORMAT_B8G8R8A8_SSCALED :
         return "VK_FORMAT_B8G8R8A8_SSCALED";
      case VK_FORMAT_B8G8R8A8_UINT :
         return "VK_FORMAT_B8G8R8A8_UINT";
      case VK_FORMAT_B8G8R8A8_SINT :
         return "VK_FORMAT_B8G8R8A8_SINT";
      case VK_FORMAT_B8G8R8A8_SRGB :
         return "VK_FORMAT_B8G8R8A8_SRGB";
      case VK_FORMAT_A8B8G8R8_UNORM_PACK32 :
         return "VK_FORMAT_A8B8G8R8_UNORM_PACK32";
      case VK_FORMAT_A8B8G8R8_SNORM_PACK32 :
         return "VK_FORMAT_A8B8G8R8_SNORM_PACK32";
      case VK_FORMAT_A8B8G8R8_USCALED_PACK32 :
         return "VK_FORMAT_A8B8G8R8_USCALED_PACK32";
      case VK_FORMAT_A8B8G8R8_SSCALED_PACK32 :
         return "VK_FORMAT_A8B8G8R8_SSCALED_PACK32";
      case VK_FORMAT_A8B8G8R8_UINT_PACK32 :
         return "VK_FORMAT_A8B8G8R8_UINT_PACK32";
      case VK_FORMAT_A8B8G8R8_SINT_PACK32 :
         return "VK_FORMAT_A8B8G8R8_SINT_PACK32";
      case VK_FORMAT_A8B8G8R8_SRGB_PACK32 :
         return "VK_FORMAT_A8B8G8R8_SRGB_PACK32";
      case VK_FORMAT_A2R10G10B10_UNORM_PACK32 :
         return "VK_FORMAT_A2R10G10B10_UNORM_PACK32";
      case VK_FORMAT_A2R10G10B10_SNORM_PACK32 :
         return "VK_FORMAT_A2R10G10B10_SNORM_PACK32";
      case VK_FORMAT_A2R10G10B10_USCALED_PACK32 :
         return "VK_FORMAT_A2R10G10B10_USCALED_PACK32";
      case VK_FORMAT_A2R10G10B10_SSCALED_PACK32 :
         return "VK_FORMAT_A2R10G10B10_SSCALED_PACK32";
      case VK_FORMAT_A2R10G10B10_UINT_PACK32 :
         return "VK_FORMAT_A2R10G10B10_UINT_PACK32";
      case VK_FORMAT_A2R10G10B10_SINT_PACK32 :
         return "VK_FORMAT_A2R10G10B10_SINT_PACK32";
      case VK_FORMAT_A2B10G10R10_UNORM_PACK32 :
         return "VK_FORMAT_A2B10G10R10_UNORM_PACK32";
      case VK_FORMAT_A2B10G10R10_SNORM_PACK32 :
         return "VK_FORMAT_A2B10G10R10_SNORM_PACK32";
      case VK_FORMAT_A2B10G10R10_USCALED_PACK32 :
         return "VK_FORMAT_A2B10G10R10_USCALED_PACK32";
      case VK_FORMAT_A2B10G10R10_SSCALED_PACK32 :
         return "VK_FORMAT_A2B10G10R10_SSCALED_PACK32";
      case VK_FORMAT_A2B10G10R10_UINT_PACK32 :
         return "VK_FORMAT_A2B10G10R10_UINT_PACK32";
      case VK_FORMAT_A2B10G10R10_SINT_PACK32 :
         return "VK_FORMAT_A2B10G10R10_SINT_PACK32";
      case VK_FORMAT_R16_UNORM :
         return "VK_FORMAT_R16_UNORM";
      case VK_FORMAT_R16_SNORM :
         return "VK_FORMAT_R16_SNORM";
      case VK_FORMAT_R16_USCALED :
         return "VK_FORMAT_R16_USCALED";
      case VK_FORMAT_R16_SSCALED :
         return "VK_FORMAT_R16_SSCALED";
      case VK_FORMAT_R16_UINT :
         return "VK_FORMAT_R16_UINT";
      case VK_FORMAT_R16_SINT :
         return "VK_FORMAT_R16_SINT";
      case VK_FORMAT_R16_SFLOAT :
         return "VK_FORMAT_R16_SFLOAT";
      case VK_FORMAT_R16G16_UNORM :
         return "VK_FORMAT_R16G16_UNORM";
      case VK_FORMAT_R16G16_SNORM :
         return "VK_FORMAT_R16G16_SNORM";
      case VK_FORMAT_R16G16_USCALED :
         return "VK_FORMAT_R16G16_USCALED";
      case VK_FORMAT_R16G16_SSCALED :
         return "VK_FORMAT_R16G16_SSCALED";
      case VK_FORMAT_R16G16_UINT :
         return "VK_FORMAT_R16G16_UINT";
      case VK_FORMAT_R16G16_SINT :
         return "VK_FORMAT_R16G16_SINT";
      case VK_FORMAT_R16G16_SFLOAT :
         return "VK_FORMAT_R16G16_SFLOAT";
      case VK_FORMAT_R16G16B16_UNORM :
         return "VK_FORMAT_R16G16B16_UNORM";
      case VK_FORMAT_R16G16B16_SNORM :
         return "VK_FORMAT_R16G16B16_SNORM";
      case VK_FORMAT_R16G16B16_USCALED :
         return "VK_FORMAT_R16G16B16_USCALED";
      case VK_FORMAT_R16G16B16_SSCALED :
         return "VK_FORMAT_R16G16B16_SSCALED";
      case VK_FORMAT_R16G16B16_UINT :
         return "VK_FORMAT_R16G16B16_UINT";
      case VK_FORMAT_R16G16B16_SINT :
         return "VK_FORMAT_R16G16B16_SINT";
      case VK_FORMAT_R16G16B16_SFLOAT :
         return "VK_FORMAT_R16G16B16_SFLOAT";
      case VK_FORMAT_R16G16B16A16_UNORM :
         return "VK_FORMAT_R16G16B16A16_UNORM";
      case VK_FORMAT_R16G16B16A16_SNORM :
         return "VK_FORMAT_R16G16B16A16_SNORM";
      case VK_FORMAT_R16G16B16A16_USCALED :
         return "VK_FORMAT_R16G16B16A16_USCALED";
      case VK_FORMAT_R16G16B16A16_SSCALED :
         return "VK_FORMAT_R16G16B16A16_SSCALED";
      case VK_FORMAT_R16G16B16A16_UINT :
         return "VK_FORMAT_R16G16B16A16_UINT";
      case VK_FORMAT_R16G16B16A16_SINT :
         return "VK_FORMAT_R16G16B16A16_SINT";
      case VK_FORMAT_R16G16B16A16_SFLOAT :
         return "VK_FORMAT_R16G16B16A16_SFLOAT";
      case VK_FORMAT_R32_UINT :
         return "VK_FORMAT_R32_UINT";
      case VK_FORMAT_R32_SINT :
         return "VK_FORMAT_R32_SINT";
      case VK_FORMAT_R32_SFLOAT :
         return "VK_FORMAT_R32_SFLOAT";
      case VK_FORMAT_R32G32_UINT :
         return "VK_FORMAT_R32G32_UINT";
      case VK_FORMAT_R32G32_SINT :
         return "VK_FORMAT_R32G32_SINT";
      case VK_FORMAT_R32G32_SFLOAT :
         return "VK_FORMAT_R32G32_SFLOAT";
      case VK_FORMAT_R32G32B32_UINT :
         return "VK_FORMAT_R32G32B32_UINT";
      case VK_FORMAT_R32G32B32_SINT :
         return "VK_FORMAT_R32G32B32_SINT";
      case VK_FORMAT_R32G32B32_SFLOAT :
         return "VK_FORMAT_R32G32B32_SFLOAT";
      case VK_FORMAT_R32G32B32A32_UINT :
         return "VK_FORMAT_R32G32B32A32_UINT";
      case VK_FORMAT_R32G32B32A32_SINT :
         return "VK_FORMAT_R32G32B32A32_SINT";
      case VK_FORMAT_R32G32B32A32_SFLOAT :
         return "VK_FORMAT_R32G32B32A32_SFLOAT";
      case VK_FORMAT_R64_UINT :
         return "VK_FORMAT_R64_UINT";
      case VK_FORMAT_R64_SINT :
         return "VK_FORMAT_R64_SINT";
      case VK_FORMAT_R64_SFLOAT :
         return "VK_FORMAT_R64_SFLOAT";
      case VK_FORMAT_R64G64_UINT :
         return "VK_FORMAT_R64G64_UINT";
      case VK_FORMAT_R64G64_SINT :
         return "VK_FORMAT_R64G64_SINT";
      case VK_FORMAT_R64G64_SFLOAT :
         return "VK_FORMAT_R64G64_SFLOAT";
      case VK_FORMAT_R64G64B64_UINT :
         return "VK_FORMAT_R64G64B64_UINT";
      case VK_FORMAT_R64G64B64_SINT :
         return "VK_FORMAT_R64G64B64_SINT";
      case VK_FORMAT_R64G64B64_SFLOAT :
         return "VK_FORMAT_R64G64B64_SFLOAT";
      case VK_FORMAT_R64G64B64A64_UINT :
         return "VK_FORMAT_R64G64B64A64_UINT";
      case VK_FORMAT_R64G64B64A64_SINT :
         return "VK_FORMAT_R64G64B64A64_SINT";
      case VK_FORMAT_R64G64B64A64_SFLOAT :
         return "VK_FORMAT_R64G64B64A64_SFLOAT";
      case VK_FORMAT_B10G11R11_UFLOAT_PACK32 :
         return "VK_FORMAT_B10G11R11_UFLOAT_PACK32";
      case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32 :
         return "VK_FORMAT_E5B9G9R9_UFLOAT_PACK32";
      case VK_FORMAT_D16_UNORM :
         return "VK_FORMAT_D16_UNORM";
      case VK_FORMAT_X8_D24_UNORM_PACK32 :
         return "VK_FORMAT_X8_D24_UNORM_PACK32";
      case VK_FORMAT_D32_SFLOAT :
         return "VK_FORMAT_D32_SFLOAT";
      case VK_FORMAT_S8_UINT :
         return "VK_FORMAT_S8_UINT";
      case VK_FORMAT_D16_UNORM_S8_UINT :
         return "VK_FORMAT_D16_UNORM_S8_UINT";
      case VK_FORMAT_D24_UNORM_S8_UINT :
         return "VK_FORMAT_D24_UNORM_S8_UINT";
      case VK_FORMAT_D32_SFLOAT_S8_UINT :
         return "VK_FORMAT_D32_SFLOAT_S8_UINT";
      case VK_FORMAT_BC1_RGB_UNORM_BLOCK :
         return "VK_FORMAT_BC1_RGB_UNORM_BLOCK";
      case VK_FORMAT_BC1_RGB_SRGB_BLOCK :
         return "VK_FORMAT_BC1_RGB_SRGB_BLOCK";
      case VK_FORMAT_BC1_RGBA_UNORM_BLOCK :
         return "VK_FORMAT_BC1_RGBA_UNORM_BLOCK";
      case VK_FORMAT_BC1_RGBA_SRGB_BLOCK :
         return "VK_FORMAT_BC1_RGBA_SRGB_BLOCK";
      case VK_FORMAT_BC2_UNORM_BLOCK :
         return "VK_FORMAT_BC2_UNORM_BLOCK";
      case VK_FORMAT_BC2_SRGB_BLOCK :
         return "VK_FORMAT_BC2_SRGB_BLOCK";
      case VK_FORMAT_BC3_UNORM_BLOCK :
         return "VK_FORMAT_BC3_UNORM_BLOCK";
      case VK_FORMAT_BC3_SRGB_BLOCK :
         return "VK_FORMAT_BC3_SRGB_BLOCK";
      case VK_FORMAT_BC4_UNORM_BLOCK :
         return "VK_FORMAT_BC4_UNORM_BLOCK";
      case VK_FORMAT_BC4_SNORM_BLOCK :
         return "VK_FORMAT_BC4_SNORM_BLOCK";
      case VK_FORMAT_BC5_UNORM_BLOCK :
         return "VK_FORMAT_BC5_UNORM_BLOCK";
      case VK_FORMAT_BC5_SNORM_BLOCK :
         return "VK_FORMAT_BC5_SNORM_BLOCK";
      case VK_FORMAT_BC6H_UFLOAT_BLOCK :
         return "VK_FORMAT_BC6H_UFLOAT_BLOCK";
      case VK_FORMAT_BC6H_SFLOAT_BLOCK :
         return "VK_FORMAT_BC6H_SFLOAT_BLOCK";
      case VK_FORMAT_BC7_UNORM_BLOCK :
         return "VK_FORMAT_BC7_UNORM_BLOCK";
      case VK_FORMAT_BC7_SRGB_BLOCK :
         return "VK_FORMAT_BC7_SRGB_BLOCK";
      case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK :
         return "VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK";
      case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK :
         return "VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK";
      case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK :
         return "VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK";
      case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK :
         return "VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK";
      case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK :
         return "VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK";
      case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK :
         return "VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK";
      case VK_FORMAT_EAC_R11_UNORM_BLOCK :
         return "VK_FORMAT_EAC_R11_UNORM_BLOCK";
      case VK_FORMAT_EAC_R11_SNORM_BLOCK :
         return "VK_FORMAT_EAC_R11_SNORM_BLOCK";
      case VK_FORMAT_EAC_R11G11_UNORM_BLOCK :
         return "VK_FORMAT_EAC_R11G11_UNORM_BLOCK";
      case VK_FORMAT_EAC_R11G11_SNORM_BLOCK :
         return "VK_FORMAT_EAC_R11G11_SNORM_BLOCK";
      case VK_FORMAT_ASTC_4x4_UNORM_BLOCK :
         return "VK_FORMAT_ASTC_4x4_UNORM_BLOCK";
      case VK_FORMAT_ASTC_4x4_SRGB_BLOCK :
         return "VK_FORMAT_ASTC_4x4_SRGB_BLOCK";
      case VK_FORMAT_ASTC_5x4_UNORM_BLOCK :
         return "VK_FORMAT_ASTC_5x4_UNORM_BLOCK";
      case VK_FORMAT_ASTC_5x4_SRGB_BLOCK :
         return "VK_FORMAT_ASTC_5x4_SRGB_BLOCK";
      case VK_FORMAT_ASTC_5x5_UNORM_BLOCK :
         return "VK_FORMAT_ASTC_5x5_UNORM_BLOCK";
      case VK_FORMAT_ASTC_5x5_SRGB_BLOCK :
         return "VK_FORMAT_ASTC_5x5_SRGB_BLOCK";
      case VK_FORMAT_ASTC_6x5_UNORM_BLOCK :
         return "VK_FORMAT_ASTC_6x5_UNORM_BLOCK";
      case VK_FORMAT_ASTC_6x5_SRGB_BLOCK :
         return "VK_FORMAT_ASTC_6x5_SRGB_BLOCK";
      case VK_FORMAT_ASTC_6x6_UNORM_BLOCK :
         return "VK_FORMAT_ASTC_6x6_UNORM_BLOCK";
      case VK_FORMAT_ASTC_6x6_SRGB_BLOCK :
         return "VK_FORMAT_ASTC_6x6_SRGB_BLOCK";
      case VK_FORMAT_ASTC_8x5_UNORM_BLOCK :
         return "VK_FORMAT_ASTC_8x5_UNORM_BLOCK";
      case VK_FORMAT_ASTC_8x5_SRGB_BLOCK :
         return "VK_FORMAT_ASTC_8x5_SRGB_BLOCK";
      case VK_FORMAT_ASTC_8x6_UNORM_BLOCK :
         return "VK_FORMAT_ASTC_8x6_UNORM_BLOCK";
      case VK_FORMAT_ASTC_8x6_SRGB_BLOCK :
         return "VK_FORMAT_ASTC_8x6_SRGB_BLOCK";
      case VK_FORMAT_ASTC_8x8_UNORM_BLOCK :
         return "VK_FORMAT_ASTC_8x8_UNORM_BLOCK";
      case VK_FORMAT_ASTC_8x8_SRGB_BLOCK :
         return "VK_FORMAT_ASTC_8x8_SRGB_BLOCK";
      case VK_FORMAT_ASTC_10x5_UNORM_BLOCK :
         return "VK_FORMAT_ASTC_10x5_UNORM_BLOCK";
      case VK_FORMAT_ASTC_10x5_SRGB_BLOCK :
         return "VK_FORMAT_ASTC_10x5_SRGB_BLOCK";
      case VK_FORMAT_ASTC_10x6_UNORM_BLOCK :
         return "VK_FORMAT_ASTC_10x6_UNORM_BLOCK";
      case VK_FORMAT_ASTC_10x6_SRGB_BLOCK :
         return "VK_FORMAT_ASTC_10x6_SRGB_BLOCK";
      case VK_FORMAT_ASTC_10x8_UNORM_BLOCK :
         return "VK_FORMAT_ASTC_10x8_UNORM_BLOCK";
      case VK_FORMAT_ASTC_10x8_SRGB_BLOCK :
         return "VK_FORMAT_ASTC_10x8_SRGB_BLOCK";
      case VK_FORMAT_ASTC_10x10_UNORM_BLOCK :
         return "VK_FORMAT_ASTC_10x10_UNORM_BLOCK";
      case VK_FORMAT_ASTC_10x10_SRGB_BLOCK :
         return "VK_FORMAT_ASTC_10x10_SRGB_BLOCK";
      case VK_FORMAT_ASTC_12x10_UNORM_BLOCK :
         return "VK_FORMAT_ASTC_12x10_UNORM_BLOCK";
      case VK_FORMAT_ASTC_12x10_SRGB_BLOCK :
         return "VK_FORMAT_ASTC_12x10_SRGB_BLOCK";
      case VK_FORMAT_ASTC_12x12_UNORM_BLOCK :
         return "VK_FORMAT_ASTC_12x12_UNORM_BLOCK";
      case VK_FORMAT_ASTC_12x12_SRGB_BLOCK :
         return "VK_FORMAT_ASTC_12x12_SRGB_BLOCK";
      case VK_FORMAT_G8B8G8R8_422_UNORM :
         return "VK_FORMAT_G8B8G8R8_422_UNORM";
      case VK_FORMAT_B8G8R8G8_422_UNORM :
         return "VK_FORMAT_B8G8R8G8_422_UNORM";
      case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM :
         return "VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM";
      case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM :
         return "VK_FORMAT_G8_B8R8_2PLANE_420_UNORM";
      case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM :
         return "VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM";
      case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM :
         return "VK_FORMAT_G8_B8R8_2PLANE_422_UNORM";
      case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM :
         return "VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM";
      case VK_FORMAT_R10X6_UNORM_PACK16 :
         return "VK_FORMAT_R10X6_UNORM_PACK16";
      case VK_FORMAT_R10X6G10X6_UNORM_2PACK16 :
         return "VK_FORMAT_R10X6G10X6_UNORM_2PACK16";
      case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16 :
         return "VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16";
      case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16 :
         return "VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16";
      case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16 :
         return "VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16";
      case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16 :
         return "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16";
      case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16 :
         return "VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16";
      case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16 :
         return "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16";
      case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16 :
         return "VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16";
      case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16 :
         return "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16";
      case VK_FORMAT_R12X4_UNORM_PACK16 :
         return "VK_FORMAT_R12X4_UNORM_PACK16";
      case VK_FORMAT_R12X4G12X4_UNORM_2PACK16 :
         return "VK_FORMAT_R12X4G12X4_UNORM_2PACK16";
      case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16 :
         return "VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16";
      case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16 :
         return "VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16";
      case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16 :
         return "VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16";
      case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16 :
         return "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16";
      case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16 :
         return "VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16";
      case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16 :
         return "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16";
      case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16 :
         return "VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16";
      case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16 :
         return "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16";
      case VK_FORMAT_G16B16G16R16_422_UNORM :
         return "VK_FORMAT_G16B16G16R16_422_UNORM";
      case VK_FORMAT_B16G16R16G16_422_UNORM :
         return "VK_FORMAT_B16G16R16G16_422_UNORM";
      case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM :
         return "VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM";
      case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM :
         return "VK_FORMAT_G16_B16R16_2PLANE_420_UNORM";
      case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM :
         return "VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM";
      case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM :
         return "VK_FORMAT_G16_B16R16_2PLANE_422_UNORM";
      case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM :
         return "VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM";
      default:
         return "UNKNOWN";
   }
}

