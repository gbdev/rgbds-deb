#ifndef RGBDS_ASM_OUTPUT_H
#define RGBDS_ASM_OUTPUT_H

#include <stdint.h>

#include "asm/rpn.h"
#include "types.h"

struct Section {
	char *pzName;
	uint8_t nType;
	ULONG nPC;
	ULONG nOrg;
	ULONG nBank;
	ULONG nAlign;
	struct Section *pNext;
	struct Patch *pPatches;
	struct Charmap *charmap;
	uint8_t *tData;
};

void out_PrepPass2(void);
void out_SetFileName(char *s);
void out_NewSection(char *pzName, ULONG secttype);
void out_NewAbsSection(char *pzName, ULONG secttype, int32_t org, int32_t bank);
void out_NewAlignedSection(char *pzName, ULONG secttype, int32_t alignment, int32_t bank);
void out_AbsByte(int b);
void out_AbsByteGroup(char *s, int length);
void out_RelByte(struct Expression * expr);
void out_RelWord(struct Expression * expr);
void out_PCRelByte(struct Expression * expr);
void out_WriteObject(void);
void out_Skip(int skip);
void out_BinaryFile(char *s);
void out_BinaryFileSlice(char *s, int32_t start_pos, int32_t length);
void out_String(char *s);
void out_AbsLong(int32_t b);
void out_RelLong(struct Expression * expr);
void out_PushSection(void);
void out_PopSection(void);

#endif
