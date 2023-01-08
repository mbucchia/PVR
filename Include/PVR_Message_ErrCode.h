#ifndef SDK_PVRMESSAGEERRORCODE_H
#define SDK_PVRMESSAGEERRORCODE_H
#include "pvr_Definition.h"
#include "pvr_Message.h"

struct pvrError;
using pvrErrorHandle = pvrError*;

/// <summary>
///   Returns whether this message gets an error 
/// </summary>
/// <param name="msg">
///   The current message handle
/// </param>
/// <returns>
///  is it wrong 
/// </returns>
PVR_SDK bool pvr_Message_IsError(/*in*/pvrMessageHandle msg);
/// <summary>
///   Get the current error message handle 
/// </summary>
/// <param name="msg">
///   The current message handle
/// </param>
/// <returns>
///   error message handle 
/// </returns>
PVR_SDK pvrErrorHandle pvr_Message_GetError(/*in*/pvrMessageHandle msg);

/// <summary>
///   When an error occurs, this function can be 
///called to return the error info 
/// </summary>
/// <param name="errhandle">
///  When an error occurs (pvr_Message_IsError() is true), 
///the error handle can be obtained through pvr_Message_GetError(pvrMessageHandle msg) 
/// </param>
/// <returns>
///   error info
/// </returns>
PVR_SDK char* pvr_Message_GetErrorInfo(pvrErrorHandle errhandle);
#endif //SDK_PVRMESSAGEERRORCODE_H