/*
 *      Copyright (C) 2014-2015 Team XBMC
 *      http://xbmc.org
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

#include "AddonJoystickButtonMap.h"
#include "PeripheralAddonTranslator.h"
#include "input/joysticks/JoystickUtils.h"
#include "peripherals/Peripherals.h"
#include "peripherals/devices/Peripheral.h"
#include "utils/log.h"

using namespace JOYSTICK;
using namespace PERIPHERALS;

CAddonJoystickButtonMap::CAddonJoystickButtonMap(CPeripheral* device, const std::string& strControllerId)
  : m_device(device),
    m_addon(g_peripherals.GetAddon(device)),
    m_strControllerId(strControllerId)
{
  if (m_addon)
    m_addon->RegisterButtonMap(device, this);
  else
    CLog::Log(LOGDEBUG, "Failed to locate add-on for device \"%s\"", device->DeviceName().c_str());
}

CAddonJoystickButtonMap::~CAddonJoystickButtonMap(void)
{
  if (m_addon)
    m_addon->UnregisterButtonMap(this);
}

bool CAddonJoystickButtonMap::Load(void)
{
  if (m_addon)
  {
    m_features.clear();
    m_driverMap.clear();

    bool bSuccess = m_addon->GetFeatures(m_device, m_strControllerId, m_features);

    // GetFeatures() was changed to always return false if no features were
    // retrieved. Check here, just in case, in case its contract is changed or
    // violated in the future.
    if (bSuccess && m_features.empty())
      bSuccess = false;

    if (bSuccess)
    {
      CLog::Log(LOGDEBUG, "Loaded button map with %lu features for controller %s",
                m_features.size(), m_strControllerId.c_str());

      m_driverMap = CreateLookupTable(m_features);
    }
    else
    {
      CLog::Log(LOGDEBUG, "Failed to load button map for device \"%s\"", m_device->DeviceName().c_str());
    }

    return true;
  }
  return false;
}

bool CAddonJoystickButtonMap::GetFeature(const CDriverPrimitive& primitive, FeatureName& feature)
{
  DriverMap::const_iterator it = m_driverMap.find(primitive);
  if (it != m_driverMap.end())
  {
    feature = it->second;
    return true;
  }

  return false;
}

bool CAddonJoystickButtonMap::GetScalar(const FeatureName& feature, CDriverPrimitive& primitive)
{
  bool retVal(false);

  FeatureMap::const_iterator it = m_features.find(feature);
  if (it != m_features.end())
  {
    const ADDON::JoystickFeature& addonFeature = it->second;

    if (addonFeature.Type() == JOYSTICK_FEATURE_TYPE_SCALAR)
    {
      primitive = CPeripheralAddonTranslator::TranslatePrimitive(addonFeature.Primitive());
      retVal = true;
    }
  }

  return retVal;
}

bool CAddonJoystickButtonMap::AddScalar(const FeatureName& feature, const CDriverPrimitive& primitive)
{
  if (!primitive.IsValid())
  {
    FeatureMap::iterator it = m_features.find(feature);
    if (it != m_features.end())
      m_features.erase(it);
  }
  else
  {
    UnmapPrimitive(primitive);

    ADDON::JoystickFeature scalar(feature, JOYSTICK_FEATURE_TYPE_SCALAR);
    scalar.SetPrimitive(CPeripheralAddonTranslator::TranslatePrimitive(primitive));

    m_features[feature] = scalar;
  }

  m_driverMap = CreateLookupTable(m_features);

  return m_addon->MapFeatures(m_device, m_strControllerId, m_features);
}

bool CAddonJoystickButtonMap::GetAnalogStick(const FeatureName& feature,
                                             CDriverPrimitive& up,
                                             CDriverPrimitive& down,
                                             CDriverPrimitive& right,
                                             CDriverPrimitive& left)
{
  bool retVal(false);

  FeatureMap::const_iterator it = m_features.find(feature);
  if (it != m_features.end())
  {
    const ADDON::JoystickFeature& addonFeature = it->second;

    if (addonFeature.Type() == JOYSTICK_FEATURE_TYPE_ANALOG_STICK)
    {
      up     = CPeripheralAddonTranslator::TranslatePrimitive(addonFeature.Up());
      down   = CPeripheralAddonTranslator::TranslatePrimitive(addonFeature.Down());
      right  = CPeripheralAddonTranslator::TranslatePrimitive(addonFeature.Right());
      left   = CPeripheralAddonTranslator::TranslatePrimitive(addonFeature.Left());
      retVal = true;
    }
  }

  return retVal;
}

bool CAddonJoystickButtonMap::AddAnalogStick(const FeatureName& feature,
                                             const CDriverPrimitive& up,
                                             const CDriverPrimitive& down,
                                             const CDriverPrimitive& right,
                                             const CDriverPrimitive& left)
{
  if (!up.IsValid() && !down.IsValid() && !right.IsValid() && !left.IsValid())
  {
    FeatureMap::iterator it = m_features.find(feature);
    if (it != m_features.end())
      m_features.erase(it);
  }
  else
  {
    ADDON::JoystickFeature analogStick(feature, JOYSTICK_FEATURE_TYPE_ANALOG_STICK);

    if (up.IsValid())
    {
      UnmapPrimitive(up);
      analogStick.SetUp(CPeripheralAddonTranslator::TranslatePrimitive(up));
    }
    if (down.IsValid())
    {
      UnmapPrimitive(down);
      analogStick.SetDown(CPeripheralAddonTranslator::TranslatePrimitive(down));
    }
    if (right.IsValid())
    {
      UnmapPrimitive(right);
      analogStick.SetRight(CPeripheralAddonTranslator::TranslatePrimitive(right));
    }
    if (left.IsValid())
    {
      UnmapPrimitive(left);
      analogStick.SetLeft(CPeripheralAddonTranslator::TranslatePrimitive(left));
    }

    m_features[feature] = analogStick;
  }

  m_driverMap = CreateLookupTable(m_features);

  return m_addon->MapFeatures(m_device, m_strControllerId, m_features);
}

bool CAddonJoystickButtonMap::GetAccelerometer(const FeatureName& feature,
                                               CDriverPrimitive& positiveX,
                                               CDriverPrimitive& positiveY,
                                               CDriverPrimitive& positiveZ)
{
  bool retVal(false);

  FeatureMap::const_iterator it = m_features.find(feature);
  if (it != m_features.end())
  {
    const ADDON::JoystickFeature& addonFeature = it->second;

    if (addonFeature.Type() == JOYSTICK_FEATURE_TYPE_ACCELEROMETER)
    {
      positiveX = CPeripheralAddonTranslator::TranslatePrimitive(addonFeature.PositiveX());
      positiveY = CPeripheralAddonTranslator::TranslatePrimitive(addonFeature.PositiveY());
      positiveZ = CPeripheralAddonTranslator::TranslatePrimitive(addonFeature.PositiveZ());
      retVal    = true;
    }
  }

  return retVal;
}

bool CAddonJoystickButtonMap::AddAccelerometer(const FeatureName& feature,
                                               const CDriverPrimitive& positiveX,
                                               const CDriverPrimitive& positiveY,
                                               const CDriverPrimitive& positiveZ)
{
  if (positiveX.IsValid() && positiveY.IsValid() && positiveZ.IsValid())
  {
    FeatureMap::iterator it = m_features.find(feature);
    if (it != m_features.end())
      m_features.erase(it);
  }
  else
  {
    ADDON::JoystickFeature accelerometer(feature, JOYSTICK_FEATURE_TYPE_ACCELEROMETER);

    if (positiveX.IsValid())
    {
      UnmapPrimitive(positiveX);
      accelerometer.SetPositiveX(CPeripheralAddonTranslator::TranslatePrimitive(positiveX));
    }
    if (positiveY.IsValid())
    {
      UnmapPrimitive(positiveY);
      accelerometer.SetPositiveY(CPeripheralAddonTranslator::TranslatePrimitive(positiveY));
    }
    if (positiveZ.IsValid())
    {
      UnmapPrimitive(positiveZ);
      accelerometer.SetPositiveZ(CPeripheralAddonTranslator::TranslatePrimitive(positiveZ));
    }

    // TODO: Unmap complementary semiaxes

    m_features[feature] = accelerometer;
  }

  m_driverMap = CreateLookupTable(m_features);

  return m_addon->MapFeatures(m_device, m_strControllerId, m_features);
}

CAddonJoystickButtonMap::DriverMap CAddonJoystickButtonMap::CreateLookupTable(const FeatureMap& features)
{
  DriverMap driverMap;

  for (FeatureMap::const_iterator it = features.begin(); it != features.end(); ++it)
  {
    const ADDON::JoystickFeature& feature = it->second;

    switch (feature.Type())
    {
      case JOYSTICK_FEATURE_TYPE_SCALAR:
      {
        driverMap[CPeripheralAddonTranslator::TranslatePrimitive(feature.Primitive())] = it->first;
        break;
      }

      case JOYSTICK_FEATURE_TYPE_ANALOG_STICK:
      {
        driverMap[CPeripheralAddonTranslator::TranslatePrimitive(feature.Up())] = it->first;
        driverMap[CPeripheralAddonTranslator::TranslatePrimitive(feature.Down())] = it->first;
        driverMap[CPeripheralAddonTranslator::TranslatePrimitive(feature.Right())] = it->first;
        driverMap[CPeripheralAddonTranslator::TranslatePrimitive(feature.Left())] = it->first;
        break;
      }

      case JOYSTICK_FEATURE_TYPE_ACCELEROMETER:
      {
        CDriverPrimitive x_axis(CPeripheralAddonTranslator::TranslatePrimitive(feature.PositiveX()));
        CDriverPrimitive y_axis(CPeripheralAddonTranslator::TranslatePrimitive(feature.PositiveY()));
        CDriverPrimitive z_axis(CPeripheralAddonTranslator::TranslatePrimitive(feature.PositiveZ()));

        driverMap[x_axis] = it->first;
        driverMap[y_axis] = it->first;
        driverMap[z_axis] = it->first;

        CDriverPrimitive x_axis_opposite(x_axis.Index(), x_axis.SemiAxisDirection() * -1);
        CDriverPrimitive y_axis_opposite(y_axis.Index(), y_axis.SemiAxisDirection() * -1);
        CDriverPrimitive z_axis_opposite(z_axis.Index(), z_axis.SemiAxisDirection() * -1);

        driverMap[x_axis_opposite] = it->first;
        driverMap[y_axis_opposite] = it->first;
        driverMap[z_axis_opposite] = it->first;
        break;
      }
        
      default:
        break;
    }
  }
  
  return driverMap;
}

bool CAddonJoystickButtonMap::UnmapPrimitive(const CDriverPrimitive& primitive)
{
  bool bModified = false;

  DriverMap::iterator it = m_driverMap.find(primitive);
  if (it != m_driverMap.end())
  {
    const FeatureName& featureName = it->second;
    FeatureMap::iterator itFeature = m_features.find(featureName);
    if (itFeature != m_features.end())
    {
      ADDON::JoystickFeature& addonFeature = itFeature->second;
      ResetPrimitive(addonFeature, CPeripheralAddonTranslator::TranslatePrimitive(primitive));
      if (addonFeature.Type() == JOYSTICK_FEATURE_TYPE_UNKNOWN)
        m_features.erase(itFeature);
      bModified = true;
    }
  }

  return bModified;
}

bool CAddonJoystickButtonMap::ResetPrimitive(ADDON::JoystickFeature& feature, const ADDON::DriverPrimitive& primitive)
{
  bool bModified = false;

  switch (feature.Type())
  {
    case JOYSTICK_FEATURE_TYPE_SCALAR:
    {
      if (primitive == feature.Primitive())
      {
        CLog::Log(LOGDEBUG, "Removing \"%s\" from button map due to conflict", feature.Name().c_str());
        feature.SetType(JOYSTICK_FEATURE_TYPE_UNKNOWN);
        bModified = true;
      }
      break;
    }
    case JOYSTICK_FEATURE_TYPE_ANALOG_STICK:
    {
      if (primitive == feature.Up())
      {
        feature.SetUp(ADDON::DriverPrimitive());
        bModified = true;
      }
      else if (primitive == feature.Down())
      {
        feature.SetDown(ADDON::DriverPrimitive());
        bModified = true;
      }
      else if (primitive == feature.Right())
      {
        feature.SetRight(ADDON::DriverPrimitive());
        bModified = true;
      }
      else if (primitive == feature.Left())
      {
        feature.SetLeft(ADDON::DriverPrimitive());
        bModified = true;
      }

      if (bModified)
      {
        if (feature.Up().Type() == JOYSTICK_DRIVER_PRIMITIVE_TYPE_UNKNOWN &&
            feature.Down().Type() == JOYSTICK_DRIVER_PRIMITIVE_TYPE_UNKNOWN &&
            feature.Right().Type() == JOYSTICK_DRIVER_PRIMITIVE_TYPE_UNKNOWN &&
            feature.Left().Type() == JOYSTICK_DRIVER_PRIMITIVE_TYPE_UNKNOWN)
        {
          CLog::Log(LOGDEBUG, "Removing \"%s\" from button map due to conflict", feature.Name().c_str());
          feature.SetType(JOYSTICK_FEATURE_TYPE_UNKNOWN);
        }
      }
      break;
    }
    case JOYSTICK_FEATURE_TYPE_ACCELEROMETER:
    {
      if (primitive == feature.PositiveX() ||
          primitive == CPeripheralAddonTranslator::Opposite(feature.PositiveX()))
      {
        feature.SetPositiveX(ADDON::DriverPrimitive());
        bModified = true;
      }
      else if (primitive == feature.PositiveY() ||
               primitive == CPeripheralAddonTranslator::Opposite(feature.PositiveY()))
      {
        feature.SetPositiveY(ADDON::DriverPrimitive());
        bModified = true;
      }
      else if (primitive == feature.PositiveZ() ||
               primitive == CPeripheralAddonTranslator::Opposite(feature.PositiveZ()))
      {
        feature.SetPositiveZ(ADDON::DriverPrimitive());
        bModified = true;
      }

      if (bModified)
      {
        if (feature.PositiveX().Type() == JOYSTICK_DRIVER_PRIMITIVE_TYPE_UNKNOWN &&
            feature.PositiveY().Type() == JOYSTICK_DRIVER_PRIMITIVE_TYPE_UNKNOWN &&
            feature.PositiveZ().Type() == JOYSTICK_DRIVER_PRIMITIVE_TYPE_UNKNOWN)
        {
          CLog::Log(LOGDEBUG, "Removing \"%s\" from button map due to conflict", feature.Name().c_str());
          feature.SetType(JOYSTICK_FEATURE_TYPE_UNKNOWN);
        }
      }
      break;
    }
    default:
      break;
  }
  return bModified;
}
