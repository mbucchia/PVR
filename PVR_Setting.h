#ifndef SDK_PVRSETTING_H
#define SDK_PVRSETTING_H
#include "pvr_Definition.h"
#include "pvr_ErrCode.h"

/// <summary>
///   This is an async function that gets the result via pvr_PollMessage
/// 
///   After receiving the pvrMessageType of pvrMessage_GetCurrentLanguage, 
///   Get the current language from pvr_Message_GetString ( pvrMessageHandle msg ) ,
///   which returns the BCP47 language tag (const char*). 
/// 
///   First call ::pvr_Message_IsError() to check if an error occurred.
/// </summary>
/// <returns>
///   Whether the message was received successfully.
///   See the pvr_ErrCode.h file for details
/// </returns>
PVR_SDK pvrPlatformResult pvr_GetCurrentLanguage(); 

#endif //SDK_PVSETTING_H