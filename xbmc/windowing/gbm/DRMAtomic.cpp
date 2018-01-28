/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DRMAtomic.h"

#include "ServiceBroker.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIWindowManager.h"
#include "settings/Settings.h"
#include "utils/log.h"

#include <errno.h>
#include <string.h>

#include <drm_fourcc.h>
#include <drm_mode.h>
#include <unistd.h>

using namespace KODI::WINDOWING::GBM;

int32_t getScaling_factor(uint32_t destWidth, uint32_t destHeight,uint32_t srcWidth, uint32_t srcHeight ){
	uint32_t fW, fH;
	fW=destWidth/srcWidth;
	fH=destHeight/srcHeight;
	if(fW != fH)
		return (fW<fH)?fW:fH;
	return fW;
}
void CDRMAtomic::DrmAtomicCommit(int fb_id, int flags, bool rendered, bool videoLayer)
{
  uint32_t blob_id;

  bool modeset = (flags & DRM_MODE_ATOMIC_ALLOW_MODESET);
  if (modeset)
  {
    if (!AddProperty(m_connector, "CRTC_ID", m_crtc->crtc->crtc_id))
    {
      return;
    }

    if (drmModeCreatePropertyBlob(m_fd, m_mode, sizeof(*m_mode), &blob_id) != 0)
    {
      return;
    }

    if (!AddProperty(m_crtc, "MODE_ID", blob_id))
    {
      return;
    }

    if (!AddProperty(m_crtc, "ACTIVE", m_active ? 1 : 0))
    {
      return;
    }
  }

  if (rendered)
  {
    AddProperty(m_gui_plane, "FB_ID", fb_id);
    AddProperty(m_gui_plane, "CRTC_ID", m_crtc->crtc->crtc_id);

    if (modeset)
    {
      drmModeFBPtr r;
      r=drmModeGetFB(m_fd,fb_id);
      if(r){
       CLog::Log(LOGINFO, "SAMEER --> FB Details:\n");
       CLog::Log(LOGINFO, "SAMEER:[%s:%d]  FB - [ID-%d] [W-%d x H-%d] [Pitch-%d][BPP- %d] [Depth-%d]\n", __func__, __LINE__,r->fb_id,r->width,r->height,r->pitch,r->bpp,r->depth);
      }
      CLog::Log(LOGINFO, "SAMEER: [%s:%d] WxH --> %d x %d\n",__func__, __LINE__, m_width,m_height);

      AddProperty(m_gui_plane, "SRC_X", 0);
      AddProperty(m_gui_plane, "SRC_Y", 0);
      AddProperty(m_gui_plane, "SRC_W", m_width << 16);
      AddProperty(m_gui_plane, "SRC_H", m_height << 16);
      AddProperty(m_gui_plane, "CRTC_X", 0);
      AddProperty(m_gui_plane, "CRTC_Y", 0);
	uint32_t scale_factor = getScaling_factor(m_mode->hdisplay, m_mode->vdisplay, m_width, m_height);
	CLog::Log(LOGINFO, "SAMEER --> [%s:%d] IS-Factor is :%ld\n", __func__, __LINE__,scale_factor );
	AddProperty(m_gui_plane, "CRTC_W", (scale_factor*m_width));
	AddProperty(m_gui_plane, "CRTC_H", (scale_factor*m_height));
	uint32_t property_id = this->GetPropertyId(m_gui_plane, "SCALING_FILTER");
	CLog::Log(LOGINFO, "SAMEER --> [%s:%d] Property_ID:%ld - SCALING_FILTER\n", __func__, __LINE__,property_id );
	AddProperty(m_gui_plane, "SCALING_FILTER", property_id ? 1 : 0);
	property_id = this->GetPropertyId(m_video_plane, "SCALING_FILTER");
	CLog::Log(LOGINFO, "SAMEER --> [%s:%d] Property_ID:%ld - SCALING_FILTER\n", __func__, __LINE__,property_id );
	AddProperty(m_video_plane, "SCALING_FILTER", property_id ? 1 : 0);
    }

    if (videoLayer)
    {
      /*
        114 alpha:
          flags: range
          values: 0 65535
          value: 65535
        115 pixel blend mode:
          flags: enum
          enums: None=2 Pre-multiplied=0 Coverage=1
          value: 0
      */
      if (SupportsProperty(m_gui_plane, "alpha"))
      {
        AddProperty(m_gui_plane, "alpha", 0x7e7e);
      }

      if (SupportsProperty(m_gui_plane, "pixel blend mode"))
      {
        AddProperty(m_gui_plane, "pixel blend mode", 2);
      }
    }
    else
    {
      if (SupportsProperty(m_gui_plane, "alpha"))
      {
        AddProperty(m_gui_plane, "alpha", 0xFFFF);
      }
    }
  }
  else if (videoLayer && !CServiceBroker::GetGUI()->GetWindowManager().HasVisibleControls())
  {
    // disable gui plane when video layer is active and gui has no visible controls
    AddProperty(m_gui_plane, "FB_ID", 0);
    AddProperty(m_gui_plane, "CRTC_ID", 0);
  }

  int ret = 0;
  if (modeset)
  {
    ret = drmModeAtomicCommit(m_fd, m_req, flags | DRM_MODE_ATOMIC_TEST_ONLY, nullptr);
    if (ret < 0)
    {
      CLog::Log(LOGERROR, "CDRMAtomic::%s - test commit failed: %s", __FUNCTION__, strerror(errno));
    }
  }

  if (ret == 0)
  {
    ret = drmModeAtomicCommit(m_fd, m_req, flags, nullptr);
    if (ret < 0)
    {
      CLog::Log(LOGERROR, "CDRMAtomic::%s - atomic commit failed: %s", __FUNCTION__, strerror(errno));
    }
  }

  if (modeset)
  {
    if (drmModeDestroyPropertyBlob(m_fd, blob_id) != 0)
      CLog::Log(LOGERROR, "CDRMAtomic::%s - failed to destroy property blob: %s", __FUNCTION__, strerror(errno));
  }

  drmModeAtomicFree(m_req);
  m_req = drmModeAtomicAlloc();
}

void CDRMAtomic::FlipPage(struct gbm_bo *bo, bool rendered, bool videoLayer)
{
  struct drm_fb *drm_fb = nullptr;

  if (rendered)
  {
    if (videoLayer)
      m_gui_plane->SetFormat(CDRMUtils::FourCCWithAlpha(m_gui_plane->GetFormat()));
    else
      m_gui_plane->SetFormat(CDRMUtils::FourCCWithoutAlpha(m_gui_plane->GetFormat()));

    drm_fb = CDRMUtils::DrmFbGetFromBo(bo);
    if (!drm_fb)
    {
      CLog::Log(LOGERROR, "CDRMAtomic::%s - Failed to get a new FBO", __FUNCTION__);
      return;
    }
  }

  uint32_t flags = 0;

  if (m_need_modeset)
  {
    flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
    m_need_modeset = false;
    CLog::Log(LOGDEBUG, "CDRMAtomic::%s - Execute modeset at next commit", __FUNCTION__);
  }

  DrmAtomicCommit(!drm_fb ? 0 : drm_fb->fb_id, flags, rendered, videoLayer);
}

bool CDRMAtomic::InitDrm()
{
  if (!CDRMUtils::OpenDrm(true))
  {
    return false;
  }

  auto ret = drmSetClientCap(m_fd, DRM_CLIENT_CAP_ATOMIC, 1);
  if (ret)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - no atomic modesetting support: %s", __FUNCTION__, strerror(errno));
    return false;
  }

  m_req = drmModeAtomicAlloc();

  if (!CDRMUtils::InitDrm())
  {
    return false;
  }

  if (!CDRMAtomic::ResetPlanes())
  {
    CLog::Log(LOGDEBUG, "CDRMAtomic::%s - failed to reset planes", __FUNCTION__);
  }

  CLog::Log(LOGDEBUG, "CDRMAtomic::%s - initialized atomic DRM", __FUNCTION__);
  return true;
}

void CDRMAtomic::DestroyDrm()
{
  CDRMUtils::DestroyDrm();

  drmModeAtomicFree(m_req);
  m_req = nullptr;
}

bool CDRMAtomic::SetVideoMode(const RESOLUTION_INFO& res, struct gbm_bo *bo)
{
  m_need_modeset = true;

  return true;
}

bool CDRMAtomic::SetActive(bool active)
{
  m_need_modeset = true;
  m_active = active;

  return true;
}

bool CDRMAtomic::AddProperty(struct drm_object *object, const char *name, uint64_t value)
{
  uint32_t property_id = this->GetPropertyId(object, name);
  if (!property_id)
    return false;

  if (drmModeAtomicAddProperty(m_req, object->id, property_id, value) < 0)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - could not add property %s", __FUNCTION__, name);
    return false;
  }

  return true;
}

bool CDRMAtomic::ResetPlanes()
{
  drmModePlaneResPtr plane_resources = drmModeGetPlaneResources(m_fd);
  if (!plane_resources)
  {
    CLog::Log(LOGERROR, "CDRMAtomic::%s - drmModeGetPlaneResources failed: %s", __FUNCTION__, strerror(errno));
    return false;
  }

  for (uint32_t i = 0; i < plane_resources->count_planes; i++)
  {
    drmModePlanePtr plane = drmModeGetPlane(m_fd, plane_resources->planes[i]);
    if (!plane)
      continue;

    drm_object object;

    if (!CDRMUtils::GetProperties(m_fd, plane->plane_id, DRM_MODE_OBJECT_PLANE, &object))
    {
      CLog::Log(LOGERROR, "CDRMAtomic::%s - could not get plane %u properties: %s", __FUNCTION__, plane->plane_id, strerror(errno));\
      drmModeFreePlane(plane);
      continue;
    }

    AddProperty(&object, "FB_ID", 0);
    AddProperty(&object, "CRTC_ID", 0);

    CDRMUtils::FreeProperties(&object);
    drmModeFreePlane(plane);
  }

  drmModeFreePlaneResources(plane_resources);

  return true;
}
