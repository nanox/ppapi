// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_DEV_PPB_CURSOR_CONTROL_DEV_H_
#define PPAPI_C_DEV_PPB_CURSOR_CONTROL_DEV_H_

#include "ppapi/c/dev/pp_cursor_type_dev.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_resource.h"

#define PPB_CURSOR_CONTROL_DEV_INTERFACE "PPB_CursorControl(Dev);0.1"

struct PPB_CursorControl_Dev {
  // Set a cursor.  If "type" is PP_CURSOR_TYPE_CUSTOM, then "custom_image"
  // must be an ImageData resource containing the cursor and "hot_spot" must
  // contain the offset within that image that refers to the cursor's position.
  bool (*SetCursor)(PP_Instance instance,
                    enum PP_CursorType_Dev type,
                    PP_Resource custom_image,
                    const struct PP_Point* hot_spot);

  // This method causes the cursor to be moved to the center of the
  // instance and be locked, preventing the user from moving it.
  // The cursor is implicitly hidden from the user while locked.
  // Cursor lock may only be requested in response to a
  // PP_InputEvent_MouseDown, and then only if the event was generated via
  // user gesture.
  //
  // While the cursor is locked, any movement of the mouse will
  // generate a PP_InputEvent_Type_MouseMove, whose x and y values
  // indicate the position the cursor would have been moved to had
  // the cursor not been locked, and had the screen been infinite in size.
  //
  // The browser may revoke cursor lock for reasons including but not
  // limited to the user pressing the ESC key, the user activating
  // another program via a reserved keystroke (e.g., ALT+TAB), or
  // some other system event.
  //
  // Returns true if the cursor could be locked.
  bool (*LockCursor)(PP_Instance);

  // Causes the cursor to be unlocked, allowing it to track user
  // movement again.
  bool (*UnlockCursor)(PP_Instance);

  // Returns true if the cursor is locked.
  bool (*HasCursorLock)(PP_Instance);

  // Returns true if the cursor can be locked.
  bool (*CanLockCursor)(PP_Instance);
};

#endif  //  PPAPI_C_DEV_PPB_CURSOR_CONTROL_DEV_H_

