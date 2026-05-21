#ifndef DOBBY_H
#define DOBBY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Dobby inline hook fonksiyon prototipi
int DobbyHook(void *target_address, void *replace_address, void **origin_address);

#ifdef __cplusplus
}
#endif

#endif // DOBBY_H
