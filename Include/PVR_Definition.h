#ifndef SDK_PVRDEFINE_H
#define SDK_PVRDEFINE_H

#ifdef PLATFORMSDK_EXPORTS	
#define PVR_SDK __declspec(dllexport)  
#else
#define PVR_SDK __declspec(dllimport)  
#endif
#include <stdint.h>

typedef uint64_t pvrID;

#endif //SDK_PVRDEFINE_H