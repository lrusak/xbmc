#pragma once

/*
 *      Copyright (C) 2005-2014 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "linux/V4L2.h"

#include "DVDVideoCodec.h"
#include "DVDStreamInfo.h"
#include "utils/log.h"
#include "utils/BitstreamConverter.h"

class CV4L2Codec
{
public:
  CV4L2Codec();
  virtual ~CV4L2Codec();

  bool OpenDecoder();
  void CloseDecoder();

  bool AddData(uint8_t *pData, size_t size, double dts, double pts);
  CDVDVideoCodec::VCReturn GetPicture(VideoPicture* pVideoPicture);

  const char* GetOutputName() { return m_name.c_str(); };
  bool SetupOutputFormat(CDVDStreamInfo &hints);

  bool SetupOutputBuffers();
  bool SetupCaptureBuffers();
  int GetOutputBuffersCount()  { return m_OutputBuffers.g_buffers();  };
  int GetCaptureBuffersCount() { return m_CaptureBuffers.g_buffers(); };

  cv4l_queue GetOutputBuffers();
  cv4l_queue GetCaptureBuffers();
  cv4l_fd * GetFd();

  bool IsOutputBufferEmpty(int index);
  bool IsCaptureBufferEmpty(int index);

  bool QueueOutputBuffer(int index, uint8_t* pData, int size, double pts);
  bool DequeueOutputBuffer(timeval *timestamp);
  bool QueueCaptureBuffer(int index);
  CDVDVideoCodec::VCReturn DequeueCaptureBuffer(timeval *timestamp);

private:
  cv4l_fd *m_fd;
  std::string m_name;
  cv4l_queue m_OutputBuffers;
  cv4l_queue m_CaptureBuffers;

  int m_iDequeuedToPresentBufferNumber;
  int m_OutputType;
  int m_CaptureType;
};