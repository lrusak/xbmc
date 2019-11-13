/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "VulkanDebug.h"

#include "utils/log.h"

using namespace KODI::RENDERING::VULKAN;

namespace
{
VkBool32 debugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                     VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                     VkDebugUtilsMessengerCallbackDataEXT const* callbackData,
                                     void* /*pUserData*/)
{
  std::string message;
  message.append("\nMessageIDName: " + std::string(callbackData->pMessageIdName));
  message.append("\nMessageIDNumber: " + std::to_string(callbackData->messageIdNumber));
  message.append("\nMessage: " + std::string(callbackData->pMessage));

  if (callbackData->queueLabelCount > 0)
  {
    message.append("\n\tQueue Labels:");
    for (uint8_t i = 0; i < callbackData->queueLabelCount; i++)
      message.append("\n\t\tlableName = <" + std::string(callbackData->pQueueLabels[i].pLabelName) +
                     ">");
  }

  if (callbackData->cmdBufLabelCount > 0)
  {
    message.append("\n\tCommandBuffer Labels:");
    for (uint8_t i = 0; i < callbackData->cmdBufLabelCount; i++)
      message.append("\n\t\tlableName = <" +
                     std::string(callbackData->pCmdBufLabels[i].pLabelName) + ">");
  }

  if (callbackData->objectCount > 0)
  {
    message.append("\n\tObjects:");
    for (uint8_t i = 0; i < callbackData->objectCount; i++)
    {
      message.append("\n\t\tObject: " + std::to_string(i));
      message.append("\n\t\t\tobjectType: " + vk::to_string(static_cast<vk::ObjectType>(
                                                  callbackData->pObjects[i].objectType)));
      message.append("\n\t\t\tobjectHandle: " +
                     std::to_string(callbackData->pObjects[i].objectHandle));

      if (callbackData->pObjects[i].pObjectName)
        message.append("\n\t\t\tobjectName: <" +
                       std::string(callbackData->pObjects[i].pObjectName) + ">");
    }
  }

  CLog::Log(LOGDEBUG, "VULKAN: [{}] {}{}",
            vk::to_string(static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity)),
            vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageTypes)), message);

  return VK_TRUE;
}
} // namespace

CVulkanDebug::CVulkanDebug(vk::UniqueInstance& instance)
{
  m_loader = vk::DispatchLoaderDynamic(instance.get(), vkGetInstanceProcAddr);

  vk::DebugUtilsMessageSeverityFlagsEXT messageSeverityFlags(
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning);

  vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
      vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
      vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
      vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance);

  vk::DebugUtilsMessengerCreateInfoEXT createInfo;
  createInfo.setMessageSeverity(messageSeverityFlags)
      .setMessageType(messageTypeFlags)
      .setPfnUserCallback(&debugUtilsMessengerCallback);

  m_debug = instance->createDebugUtilsMessengerEXTUnique(createInfo, nullptr, m_loader);
}
