/*
 *  Copyright (C) 2012-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "TimingConstants.h"
#include "addons/kodi-dev-kit/include/kodi/c-api/addon-instance/inputstream/demux_packet.h"

#define DMX_SPECIALID_STREAMINFO DEMUX_SPECIALID_STREAMINFO
#define DMX_SPECIALID_STREAMCHANGE DEMUX_SPECIALID_STREAMCHANGE

#include <stdexcept>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

  struct DemuxPacket : DEMUX_PACKET
  {
    DemuxPacket()
    {
      packet = av_packet_alloc();
      if (!packet)
        throw std::runtime_error("av_packet_alloc: oom?");

      packet->data = nullptr;
      packet->size = 0;
      packet->stream_index = -1;
      demuxerId = -1;
      iGroupId = -1;
      packet->side_data = nullptr;
      packet->side_data_elems = 0;
      packet->pts = DVD_NOPTS_VALUE;
      packet->dts = DVD_NOPTS_VALUE;
      packet->duration = 0;
      dispTime = 0;
      recoveryPoint = false;

      cryptoInfo = nullptr;
    }

    ~DemuxPacket() { av_packet_free(&packet); }
  };

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
