/*
 *  Copyright (C) 2010 Team Boxee
 *      http://www.boxee.tv
 *
 *  Copyright (C) 2010-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

class CURL;

namespace XFILE
{

class IFileDirectory;

class CDiscImageDirectory
{
public:
  static IFileDirectory* GetDiscImageDirectory(const CURL& url);
};

}

