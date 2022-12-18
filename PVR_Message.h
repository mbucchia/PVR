#ifndef SDK_PVRMESSAGE_H
#define SDK_PVRMESSAGE_H
#include "pvr_Definition.h"
#include "pvr_ErrCode.h"
#include "pvr_messageType.h"

struct pvrMessage;
typedef pvrMessage* pvrMessageHandle;

/// <summary>
///   Returns the next message in the message loop.
///   The returned messages are out of order, 
///   as each message takes longer or shorter to process.
/// </summary>
/// <returns>
///   pointer to the message handle, which can be 
///   used to get the result 
/// </returns>
PVR_SDK pvrMessageHandle pvr_PollMessage();


/// <summary>
///   Get the message type of the current message handle 
/// </summary>
/// <param name="msg">
///   The current message handle
/// </param>
/// <returns>
///   return message type
///   See pvr_messageType.h file for details
/// </returns>
PVR_SDK pvrMessageType pvr_Message_GetType(/*in*/pvrMessageHandle msg);


/// <summary>
///     If the result returned by the message is a simple result, then 
///   the following function can be used to get the result. For example: pvr_GetCurrentLanguage().
/// 
///     To determine whether it is a simple result, please pay attention to 
///   the annotation of each function that gets the message.Complex results 
///   usually have dedicated getter functions.For example : pvr_GetLoggedInUser().
/// </summary>
/// <param name="msg">
///     The current message handle
/// </param>
/// <returns>
///     some type
/// </returns>
PVR_SDK short pvr_Message_GetInt16(/*in*/pvrMessageHandle msg);
PVR_SDK unsigned short pvr_Message_GetUint16(/*in*/pvrMessageHandle msg);

PVR_SDK int pvr_Message_GetInt32(/*in*/pvrMessageHandle msg);
PVR_SDK unsigned int pvr_Message_GetUint32(/*in*/pvrMessageHandle msg);

PVR_SDK long long pvr_Message_GetInt64(/*in*/pvrMessageHandle msg);
PVR_SDK unsigned long long pvr_Message_GetUint64(/*in*/pvrMessageHandle msg);

PVR_SDK bool pvr_Message_GetBool(/*in*/pvrMessageHandle msg);

PVR_SDK double pvr_Message_GetDouble(/*in*/pvrMessageHandle msg);

PVR_SDK const char * pvr_Message_GetString(/*in*/pvrMessageHandle msg);
#endif //SDK_PVRMESSAGE_H