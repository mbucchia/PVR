#ifndef SDK_PVRSDKSTATE_H
#define SDK_PVRSDKSTATE_H
#include "pvr_Definition.h"
#include "pvr_ErrCode.h"

/// <summary>
/// This is a synchronous function 
/// 
/// Initialize the platform sdk, other functions can be called after 
/// initialization, please call as soon as possible 
/// </summary>
/// <param name="appid">
/// the app id of the current app 
/// </param>
/// <returns>
///   Whether the message was received successfully.
///   See the pvr_ErrCode.h file for details
/// </returns>
PVR_SDK pvrPlatformResult pvr_PlatformInit(pvrID appid);


/// <summary>
/// This is a synchronous function 
/// 
/// This function is called when the application is about to end and 
/// the platform sdk has been called 
/// </summary>
/// <returns>
///   Whether the message was received successfully.
///   See the pvr_ErrCode.h file for details
/// </returns>
PVR_SDK pvrPlatformResult pvr_PlatformShutdown();
#endif //SDK_PVRSDKSTATE_H