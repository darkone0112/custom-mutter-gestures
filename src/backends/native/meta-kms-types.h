/*
 * Copyright (C) 2019 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdint.h>

typedef struct _MetaKms MetaKms;
typedef struct _MetaKmsDevice MetaKmsDevice;

typedef struct _MetaKmsPlane MetaKmsPlane;
typedef struct _MetaKmsCrtc MetaKmsCrtc;
typedef struct _MetaKmsConnector MetaKmsConnector;

typedef struct _MetaKmsUpdate MetaKmsUpdate;
typedef struct _MetaKmsPlaneAssignment MetaKmsPlaneAssignment;
typedef struct _MetaKmsModeSet MetaKmsModeSet;

typedef struct _MetaKmsMode MetaKmsMode;

typedef struct _MetaKmsFeedback MetaKmsFeedback;

typedef struct _MetaKmsPageFlipListenerVtable MetaKmsPageFlipListenerVtable;

typedef struct _MetaKmsImpl MetaKmsImpl;
typedef struct _MetaKmsImplDevice MetaKmsImplDevice;

typedef struct _MetaKmsCursorManager MetaKmsCursorManager;

/* 16:16 fixed point */
typedef int32_t MetaFixed16;

typedef struct _MetaFixed16Rectangle
{
  MetaFixed16 x;
  MetaFixed16 y;
  MetaFixed16 width;
  MetaFixed16 height;
} MetaFixed16Rectangle;

typedef enum _MetaKmsDeviceFlag
{
  META_KMS_DEVICE_FLAG_NONE = 0,
  META_KMS_DEVICE_FLAG_BOOT_VGA = 1 << 0,
  META_KMS_DEVICE_FLAG_PLATFORM_DEVICE = 1 << 1,
  META_KMS_DEVICE_FLAG_DISABLE_MODIFIERS = 1 << 2,
  META_KMS_DEVICE_FLAG_PREFERRED_PRIMARY = 1 << 3,
  META_KMS_DEVICE_FLAG_NO_MODE_SETTING = 1 << 4,
  META_KMS_DEVICE_FLAG_HAS_ADDFB2 = 1 << 5,
  META_KMS_DEVICE_FLAG_FORCE_LEGACY = 1 << 6,
  META_KMS_DEVICE_FLAG_DISABLE_CLIENT_MODIFIERS = 1 << 7,
  META_KMS_DEVICE_FLAG_DISABLE_VRR = 1 << 8,
  META_KMS_DEVICE_FLAG_SUPPORTS_COLOR_MODES = 1 << 9,
} MetaKmsDeviceFlag;

typedef enum _MetaKmsResourceChanges
{
  META_KMS_RESOURCE_CHANGE_NONE = 0,
  META_KMS_RESOURCE_CHANGE_GAMMA = 1 << 0,
  META_KMS_RESOURCE_CHANGE_NO_DEVICES = 1 << 1,
  META_KMS_RESOURCE_CHANGE_PRIVACY_SCREEN = 1 << 2,
  META_KMS_RESOURCE_CHANGE_FULL = -1,
} MetaKmsResourceChanges;

typedef enum _MetaKmsUpdateFlag
{
  META_KMS_UPDATE_FLAG_NONE = 0,
  META_KMS_UPDATE_FLAG_TEST_ONLY = 1 << 0,
  META_KMS_UPDATE_FLAG_MODE_SET = 1 << 1,
} MetaKmsUpdateFlag;

typedef enum _MetaKmsPlaneType MetaKmsPlaneType;

typedef enum _MetaKmsPropType
{
  META_KMS_PROP_TYPE_RAW = 0,
  META_KMS_PROP_TYPE_FIXED_16,
} MetaKmsPropType;
