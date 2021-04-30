/*
 *  Copyright (C) 2020 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "addons/kodi-dev-kit/include/kodi/addon-instance/peripheral/PeripheralUtils.h"
#include "peripherals/PeripheralTypes.h"
#include "threads/CriticalSection.h"

#import "platform/darwin/ios-common/peripherals/Input_Gamecontroller.h"
#include "platform/darwin/ios-common/peripherals/PeripheralBusDarwinEmbedded.h"

#include <string>
#include <vector>

#import <Foundation/Foundation.h>

@interface CBPeripheralBusDarwinEmbeddedManager : NSObject

@property(nonatomic, strong) Input_IOSGamecontroller* input_GC;

- (instancetype)initWithName:(PERIPHERALS::CPeripheralBusDarwinEmbedded*)parentClass;
- (PERIPHERALS::PeripheralScanResults)GetInputDevices;
- (void)DeviceAdded:static_cast<int>(deviceID);
- (void)DeviceRemoved:static_cast<int>(deviceID);
- (void)SetDigitalEvent:(kodi::addon::PeripheralEvent)event;
- (void)SetAxisEvent:(kodi::addon::PeripheralEvent)event;
- (std::vector<kodi::addon::PeripheralEvent>)GetButtonEvents;
- (std::vector<kodi::addon::PeripheralEvent>)GetAxisEvents;
- static_cast<int>(GetControllerAxisCount:(int)deviceId)
    withControllerType:(GCCONTROLLER_TYPE)controllerType;
- static_cast<int>(GetControllerButtonCount:(int)deviceId)
    withControllerType:(GCCONTROLLER_TYPE)controllerType;
- (GCCONTROLLER_TYPE)GetControllerType:static_cast<int>(deviceID);
- (std::string)GetDeviceLocation:static_cast<int>(deviceId);
- (void)displayMessage:(NSString*)message controllerID:static_cast<int>(controllerID);
@end
