#ifndef SDK_PVRMESSAGETYPE_H
#define SDK_PVRMESSAGETYPE_H

#include "pvr_Definition.h"

typedef enum messageType {
  /// horribly wrong.
  pvrMessage_Unknown = 0,
  // user
  /// To request this type of message, please call ->pvr_CheckEntitlement()
  pvrMessage_CheckEntitlement = 0x00000100,
  /// To request this type of message, please call ->pvr_GetAccessToken()
  pvrMessage_GetAccessToken = 0x00000101,
  /// To request this type of message, please call ->pvr_GetLoggedInUser()
  pvrMessage_GetLoggedInUser = 0x00000102,

  // setting
  /// To request this type of message, please call ->pvr_GetCurrentLanguage()
  pvrMessage_GetCurrentLanguage = 0x00000200,

  pvrMessage_ShutDown = 0x00020000,

  // notify
  /// The message notification is generally sent by the service to the SDK actively

  /// This message is triggered when pimax is logged out
  pvrMessage_Notify_Logout = 0x00050001,
  /// Error messages triggered during running, such as another device logging 
  /// in at another place, etc. For details, please check pvrRuntimeError in PVR_Account.h
  /// Get detailed errors through pvr_RuntimeError_GetError(msg).
  pvrMessage_Notify_RuntimeError = 0x00050002,
} pvrMessageType;

#endif  // SDK_PVRMESSAGETYPE_H