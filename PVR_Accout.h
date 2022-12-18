#ifndef SDK_PVRACCOUT_H
#define SDK_PVRACCOUT_H
#include "pvr_Definition.h"
#include "pvr_Message.h"

typedef enum checkEntitlementResult {
  /// Entitlement succeeded
  pvrEntitlement_Success = 0,
  /// Following are failed or unknow results, you should exit app in most case.
  pvrEntitlement_NotLogin = 1001,
  pvrEntitlement_Refund = 1002,
  pvrEntitlement_NotPurchased = 1003,

  /// Pimax Client is not installed
  pvrEntitlement_PimaxIsNotInstalled = 3001,
  /// Pimax Client is offline
  pvrEntitlement_NoInternet = 3002,
  /// Pimax could not verify the realtime purchase state right now due to
  /// network exception, it's your decision to exit app or allow the user use
  /// your app in offline mode.
  pvrEntitlement_NoInternetPurchased = 3003,
  /// Pimax Client is offline, and the app is not purchased AFAWK.
  pvrEntitlement_NoInternetNotPurchased = 3004,

  /// Other unknown errors
  pvrEntitlement_Unknown = 99999,
  //...
} pvrCheckEntitlementResult;

/// <summary>
///   This is an async function that gets the result via pvr_PollMessage()
///
///   After receiving the pvrMessageType of
///   pvrMessageType::pvrMessage_CheckEntitlement, get it from
///   pvr_CheckEntitlement_GetResult(pvrMessageHandle msg) whether it is
///   successful or wrong.
///
///   First call ::pvr_Message_IsError() to check if an error occurred.
/// </summary>
/// <returns>
///   Whether the message was received successfully.
///   See the pvr_ErrCode.h file for details
/// </returns>
PVR_SDK pvrPlatformResult pvr_CheckEntitlement();
/// <summary>
///   Return the result of pvr_CheckEntitlement()
///   Note: The result can only be obtained if the type of the message handle is
///   pvrMessageType::pvrMessage_CheckEntitlement
/// </summary>
/// <param name="msg">The current message handle, which can be obtained using
///   pvr_PollMessage()
/// </param>
/// <returns>
///   CheckEntitlement Result.
///   See the pvr_ErrCode.h file for details
/// </returns>
PVR_SDK pvrCheckEntitlementResult
pvr_CheckEntitlement_GetResult(pvrMessageHandle msg);

/// <summary>
///   This is an async function that gets the result via pvr_PollMessage
///
///   After receiving the pvrMessageType of
///   pvrMessageType::pvrMessage_GetAccessToken, Get Token from
///   pvr_Message_GetString(pvrMessageHandle msg)].
///
///   First call ::pvr_Message_IsError() to check if an error occurred.
/// </summary>
/// <returns>
///   Whether the message was received successfully.
///   See the pvr_ErrCode.h file for details
/// </returns>
PVR_SDK pvrPlatformResult pvr_GetAccessToken();

/// <summary>
///   This is an async function that gets the result via pvr_PollMessage
///
///   After receiving the pvrMessageType of
///   pvrMessageType::pvrMessage_GetLoggedInUser, Get UserId from
///   pvr_UserInfo_GetUserId(pvrMessageHandle msg)]]] Get UserName from
///   pvr_UserInfo_GetUserName(pvrMessageHandle msg)
///
///   First call ::pvr_Message_IsError() to check if an error occurred.
/// </summary>
/// <returns>
///   Whether the message was received successfully.
///   See the pvr_ErrCode.h file for details
/// </returns>
PVR_SDK pvrPlatformResult pvr_GetLoggedInUser();
/// <summary>
///   Returns the user id of pvr_GetLoggedInUser()
///   Note: only message handles are of type
///   pvrMessageType::pvrMessage_GetLoggedInUser
/// </summary>
/// <param name="msg">
///   The current message handle, which can be obtained using
///   pvr_PollMessage()
/// </param>
/// <returns>
///   User Id
/// </returns>
PVR_SDK unsigned long long pvr_UserInfo_GetUserId(pvrMessageHandle msg);
/// <summary>
///   Returns the user name of pvr_GetLoggedInUser()
///   Note: only message handles are of type
///   pvrMessageType::pvrMessage_GetLoggedInUser
/// </summary>
/// <param name="msg">
///   The current message handle, which can be obtained using
///   pvr_PollMessage()
/// </param>
/// <returns>
///   User Name
/// </returns>
PVR_SDK char* pvr_UserInfo_GetUserName(pvrMessageHandle msg);

typedef enum RuntimeError {
  pvrRuntimeError_Refund = 4001,
  /// The current user's app sessions exceeded the upper limit, You MUST process
  /// this message and exit the app.
  pvrRuntimeError_TooMuchDevices = 5001,

  /// Some other unknow error happened, You are advised to save user data and
  /// exit the app.
  pvrRuntimeError_Unknown = 99999,
} pvrRuntimeError;

/// <summary>
///   Return the specific runtime error, please call it
///   when pvrMessageType = pvrMessage_Notify_RuntimeError
///   to get the specific error.
/// </summary>
/// <param name="msg">
///   The current message handle, which can be obtained using
///   pvr_PollMessage()
/// </param>
/// <returns>
///   pvrRuntimeError
/// </returns>
PVR_SDK pvrRuntimeError pvr_RuntimeError_GetError(pvrMessageHandle msg);

#endif  // SDK_PVRACCOUT_H