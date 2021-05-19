#ifndef MAR_VULKANTOOLS_H
#define MAR_VULKANTOOLS_H

#include <unordered_set>
#include <unordered_map>

#include <vulkan/vulkan.h>

class VulkanTools
{
public:
   static bool init(const VkInstance& instance);
   static bool make_extension_map(std::unordered_set<std::string> extension_map);
   static VkFormat find_supported_format(const VkPhysicalDevice& physicalDevice, const std::vector<VkFormat>& candidates,
                         VkImageTiling tiling, VkFormatFeatureFlags features);
   static bool check_extensions(std::vector<const char *>& instance_extensions, std::string& missing_exts);
   static void record_barrier(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout,
                  VkPipelineStageFlags srcStages, VkPipelineStageFlags destStages);
   static std::string physical_device_typename(const VkPhysicalDeviceType& type);
   static std::string result_string(VkResult result);
   static VkImageMemoryBarrier new_image_barrier()
   {
      return VkImageMemoryBarrier { .sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED
                                  };
   }
   static void set_image_layout(VkCommandBuffer buffer, VkImage image, VkImageAspectFlags aspectMask,
                                VkImageLayout old_image_layout, VkImageLayout new_image_layout,
                                VkAccessFlagBits srcAccessMask, VkPipelineStageFlags src_stages,
                                VkPipelineStageFlags dest_stages);
   static uint32_t get_surface_formats(VkInstance const instance, VkPhysicalDevice const physical_device,
                                       VkSurfaceKHR const surface, std::vector<VkSurfaceFormatKHR>& formats,
                                       std::unordered_map<VkFormat, size_t>& surface_format_indices,
                                       std::stringstream* out);

   static std::string surface_format_string(const VkFormat format);

   static PFN_vkAcquireNextImageKHR fpAcquireNextImageKHR;
   static PFN_vkQueuePresentKHR fpQueuePresentKHR;
   static PFN_vkWaitForFences fpWaitForFences;
   static PFN_vkResetFences fpResetFences;
   static PFN_vkQueueSubmit fpQueueSubmit;
   static PFN_vkCmdPipelineBarrier fpCmdPipelineBarrier;
   static PFN_vkCmdCopyBufferToImage fpCmdCopyBufferToImage;
   static PFN_vkBeginCommandBuffer fpBeginCommandBuffer;
   static PFN_vkEndCommandBuffer fpEndCommandBuffer;
   static PFN_vkQueueWaitIdle fpQueueWaitIdle;
};


#endif //MAR_VULKANTOOLS_H
