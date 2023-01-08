#ifndef SDK_PVRERRORCODE_H
#define SDK_PVRERRORCODE_H
#include "pvr_Definition.h"

typedef enum platformResult {
  pvrPlatformResult_Success = 0,
  pvrPlatformResult_Failed = 1,

  /// <summary>
  ///     The current app id is connected
  /// </summary>
  pvrPlatformResult_IsConnected = 3,
  /// <summary>
  ///     Not initialized, or an unknown error occurred,
  /// please call pvr_PlatformInit() to reinitialize
  /// </summary>
  pvrPlatformResult_NotInitialize = 100,
  /// <summary>
  ///     pvr is in preparation
  /// </summary>
  pvrPlatformResult_NotReady,
  /// <summary>
  ///     For some unknown reason, our connection to the service
  /// failed, please reconsider the initialization or check if
  /// the service is up
  /// </summary>
  pvrPlatformResult_ServiceNotInstalled,
  pvrPlatformResult_ServiceLaunchFailed,
  pvrPlatformResult_ConnectServiceFailed,
  /// <summary>
  ///     connection to pimax failed
  /// </summary>
  pvrPlatformResult_ConnectPimaxFailed,

  /// <summary>
  ///    The connection to the Pimax server failed
  /// </summary>
  pvrPlatformResult_ConnectPimaxServerFailed,

  /// <summary>
  ///     unknown mistake
  /// </summary>
  pvrPlatformResult_Unknown = 99999,
} pvrPlatformResult;

#endif  // SDK_PVRERRORCODE_H