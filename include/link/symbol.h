#ifndef RGBDS_LINK_SYMBOL_H
#define RGBDS_LINK_SYMBOL_H

#include <stdint.h>

#include "types.h"

void sym_Init(void);
void sym_CreateSymbol(char *tzName, int32_t nValue, int32_t nBank,
			char *tzObjFileName, char *tzFileName, ULONG nFileLine);
int32_t sym_GetValue(char *tzName);
int32_t sym_GetBank(char *tzName);

#endif
