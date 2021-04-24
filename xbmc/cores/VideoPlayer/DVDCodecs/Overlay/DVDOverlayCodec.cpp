/*
 *  Copyright (C) 2012-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */
#include "DVDOverlayCodec.h"

#include "cores/VideoPlayer/Interface/DemuxPacket.h"
#include "cores/VideoPlayer/Interface/TimingConstants.h"

void CDVDOverlayCodec::GetAbsoluteTimes(double &starttime, double &stoptime, DemuxPacket *pkt, bool &replace, double offset/* = 0.0*/)
{
  if (!pkt)
    return;

  double duration = 0.0;
  double pts = starttime;

  // we assume pts from packet is better than what
  // decoder gives us, only take duration
  // from decoder if available
  if(stoptime > starttime)
    duration = stoptime - starttime;
  else if (pkt->packet->duration != DVD_NOPTS_VALUE)
    duration = pkt->packet->duration;

  if (pkt->packet->pts != DVD_NOPTS_VALUE)
    pts = pkt->packet->pts;
  else if (pkt->packet->dts != DVD_NOPTS_VALUE)
    pts = pkt->packet->dts;

  starttime = pts + offset;
  if(duration)
  {
    stoptime = pts + duration + offset;
  }
  else
  {
    stoptime = 0;
    replace = true;
  }
}
