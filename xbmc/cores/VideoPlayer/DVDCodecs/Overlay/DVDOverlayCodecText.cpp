/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DVDOverlayCodecText.h"

#include "DVDCodecs/DVDCodecs.h"
#include "DVDOverlayText.h"
#include "DVDStreamInfo.h"
#include "cores/VideoPlayer/DVDSubtitles/DVDSubtitleTagSami.h"
#include "cores/VideoPlayer/Interface/DemuxPacket.h"
#include "utils/log.h"

#include "system.h"

CDVDOverlayCodecText::CDVDOverlayCodecText() : CDVDOverlayCodec("Text Subtitle Decoder")
{
}

bool CDVDOverlayCodecText::Open(CDVDStreamInfo &hints, CDVDCodecOptions &options)
{
  if (hints.codec == AV_CODEC_ID_TEXT || hints.codec == AV_CODEC_ID_SUBRIP)
    return true;

  return false;
}

int CDVDOverlayCodecText::Decode(DemuxPacket *pPacket)
{
  if(!pPacket)
    return OC_ERROR;

  uint8_t *data = pPacket->pData;
  int      size = pPacket->iSize;

  m_pOverlay = std::make_shared<CDVDOverlayText>();
  CDVDOverlayCodec::GetAbsoluteTimes(m_pOverlay->iPTSStartTime, m_pOverlay->iPTSStopTime, pPacket, m_pOverlay->replace);

  char *start, *end, *p;
  start = (char*)data;
  end   = (char*)data + size;
  p     = (char*)data;

  CDVDSubtitleTagSami TagConv;
  bool Taginit = TagConv.Init();

  while(p<end)
  {
    if(*p == '{')
    {
      if(p>start)
      {
        if(Taginit)
          TagConv.ConvertLine(m_pOverlay.get(), start, p - start);
        else
          m_pOverlay->AddElement(new CDVDOverlayText::CElementText(start, p-start));
      }
      start = p+1;

      while(*p != '}' && p<end)
        p++;

      char* override = (char*)malloc(p-start + 1);
      memcpy(override, start, p-start);
      override[p-start] = '\0';
      CLog::Log(LOGINFO, "{} - Skipped formatting tag {}", __FUNCTION__, override);
      free(override);

      start = p+1;
    }
    p++;
  }
  if(p>start)
  {
    if(Taginit)
    {
      TagConv.ConvertLine(m_pOverlay.get(), start, p - start);
      TagConv.CloseTag(m_pOverlay.get());
    }
    else
      m_pOverlay->AddElement(new CDVDOverlayText::CElementText(start, p-start));
  }
  return OC_OVERLAY;
}

void CDVDOverlayCodecText::Reset()
{
}

void CDVDOverlayCodecText::Flush()
{
}

std::shared_ptr<CDVDOverlay> CDVDOverlayCodecText::GetOverlay()
{
  if (m_pOverlay)
  {
    std::shared_ptr<CDVDOverlay> overlay = m_pOverlay;
    m_pOverlay = nullptr;
    return overlay;
  }

  return nullptr;
}
