/*
 *  Copyright (C) 2006-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <assert.h>
#include <atomic>
#include <memory>
#include <vector>

enum DVDOverlayType
{
  DVDOVERLAY_TYPE_NONE    = -1,
  DVDOVERLAY_TYPE_SPU     = 1,
  DVDOVERLAY_TYPE_TEXT    = 2,
  DVDOVERLAY_TYPE_IMAGE   = 3,
  DVDOVERLAY_TYPE_SSA     = 4,
  DVDOVERLAY_TYPE_GROUP   = 5,
};

class CDVDOverlay
{
public:
  explicit CDVDOverlay(DVDOverlayType type)
  {
    m_type = type;

    iPTSStartTime = 0LL;
    iPTSStopTime = 0LL;
    bForced = false;
    replace = false;
    m_textureid = 0;
  }

  virtual ~CDVDOverlay()
  {
  }

  bool IsOverlayType(DVDOverlayType type) { return (m_type == type); }

  double iPTSStartTime;
  double iPTSStopTime;
  bool bForced; // display, no matter what
  bool replace; // replace by next nomatter what stoptime it has
  unsigned long m_textureid;
protected:
  DVDOverlayType m_type;
};

typedef std::vector<std::shared_ptr<CDVDOverlay>> VecOverlays;
typedef std::vector<std::shared_ptr<CDVDOverlay>>::iterator VecOverlaysIter;

class CDVDOverlayGroup : public CDVDOverlay
{

public:
  ~CDVDOverlayGroup() override = default;

  CDVDOverlayGroup()
    : CDVDOverlay(DVDOVERLAY_TYPE_GROUP)
  {
  }

  CDVDOverlayGroup(CDVDOverlayGroup& src) : CDVDOverlay(src) { m_overlays = src.m_overlays; }

  VecOverlays m_overlays;
};
