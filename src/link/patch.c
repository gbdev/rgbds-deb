#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern/err.h"
#include "link/assign.h"
#include "link/mylink.h"
#include "link/symbol.h"
#include "link/main.h"

struct sSection *pCurrentSection;
int32_t rpnstack[256];
int32_t rpnp;
int32_t nPC;

void
rpnpush(int32_t i)
{
	rpnstack[rpnp++] = i;
}

int32_t
rpnpop(void)
{
	return (rpnstack[--rpnp]);
}

int32_t
getsymvalue(int32_t symid)
{
	switch (pCurrentSection->tSymbols[symid]->Type) {
		case SYM_IMPORT:
		return (sym_GetValue(pCurrentSection->tSymbols[symid]->pzName));
		break;
	case SYM_EXPORT:
	case SYM_LOCAL:
		{
			if (strcmp
			    (pCurrentSection->tSymbols[symid]->pzName,
				"@") == 0) {
				return (nPC);
			} else
				return (pCurrentSection->tSymbols[symid]->
				    nOffset +
				    pCurrentSection->tSymbols[symid]->
				    pSection->nOrg);
		}
	default:
		break;
	}
	errx(1, "*INTERNAL* UNKNOWN SYMBOL TYPE");
}

int32_t
getsymbank(int32_t symid)
{
	int32_t nBank;

	switch (pCurrentSection->tSymbols[symid]->Type) {
	case SYM_IMPORT:
		nBank = sym_GetBank(pCurrentSection->tSymbols[symid]->pzName);
		break;
	case SYM_EXPORT:
	case SYM_LOCAL:
		nBank = pCurrentSection->tSymbols[symid]->pSection->nBank;
		break;
	default:
		errx(1, "*INTERNAL* UNKNOWN SYMBOL TYPE");
	}

	if (nBank == BANK_WRAM0 || nBank == BANK_ROM0 || nBank == BANK_OAM ||
			nBank == BANK_HRAM) {
		return 0;
	} else if (nBank >= BANK_WRAMX && nBank < (BANK_WRAMX + BANK_COUNT_WRAMX)) {
		return nBank - BANK_WRAMX + 1;
	} else if (nBank >= BANK_VRAM && nBank < (BANK_VRAM + BANK_COUNT_VRAM)) {
		return nBank - BANK_VRAM;
	} else if (nBank >= BANK_SRAM && nBank < (BANK_SRAM + BANK_COUNT_SRAM)) {
		return nBank - BANK_SRAM;
	}

	return nBank;
}

int32_t
calcrpn(struct sPatch * pPatch)
{
	int32_t t, size;
	uint8_t *rpn;

	rpnp = 0;

	size = pPatch->nRPNSize;
	rpn = pPatch->pRPN;
	pPatch->oRelocPatch = 0;

	while (size > 0) {
		size -= 1;
		switch (*rpn++) {
		case RPN_ADD:
			rpnpush(rpnpop() + rpnpop());
			break;
		case RPN_SUB:
			t = rpnpop();
			rpnpush(rpnpop() - t);
			break;
		case RPN_MUL:
			rpnpush(rpnpop() * rpnpop());
			break;
		case RPN_DIV:
			t = rpnpop();
			rpnpush(rpnpop() / t);
			break;
		case RPN_MOD:
			t = rpnpop();
			rpnpush(rpnpop() % t);
			break;
		case RPN_UNSUB:
			rpnpush(-rpnpop());
			break;
		case RPN_OR:
			rpnpush(rpnpop() | rpnpop());
			break;
		case RPN_AND:
			rpnpush(rpnpop() & rpnpop());
			break;
		case RPN_XOR:
			rpnpush(rpnpop() ^ rpnpop());
			break;
		case RPN_UNNOT:
			rpnpush(~rpnpop());
			break;
		case RPN_LOGAND:
			rpnpush(rpnpop() && rpnpop());
			break;
		case RPN_LOGOR:
			rpnpush(rpnpop() || rpnpop());
			break;
		case RPN_LOGUNNOT:
			rpnpush(!rpnpop());
			break;
		case RPN_LOGEQ:
			rpnpush(rpnpop() == rpnpop());
			break;
		case RPN_LOGNE:
			rpnpush(rpnpop() != rpnpop());
			break;
		case RPN_LOGGT:
			t = rpnpop();
			rpnpush(rpnpop() > t);
			break;
		case RPN_LOGLT:
			t = rpnpop();
			rpnpush(rpnpop() < t);
			break;
		case RPN_LOGGE:
			t = rpnpop();
			rpnpush(rpnpop() >= t);
			break;
		case RPN_LOGLE:
			t = rpnpop();
			rpnpush(rpnpop() <= t);
			break;
		case RPN_SHL:
			t = rpnpop();
			rpnpush(rpnpop() << t);
			break;
		case RPN_SHR:
			t = rpnpop();
			rpnpush(rpnpop() >> t);
			break;
		case RPN_HRAM:
			t = rpnpop();
			rpnpush(t & 0xFF);
			if (t < 0 || (t > 0xFF && t < 0xFF00) || t > 0xFFFF) {
				errx(1,
				    "%s(%ld) : Value must be in the HRAM area",
				    pPatch->pzFilename, pPatch->nLineNo);
			}
			break;
		case RPN_CONST:
			/* constant */
			t = (*rpn++);
			t |= (*rpn++) << 8;
			t |= (*rpn++) << 16;
			t |= (*rpn++) << 24;
			rpnpush(t);
			size -= 4;
			break;
		case RPN_SYM:
			/* symbol */
			t = (*rpn++);
			t |= (*rpn++) << 8;
			t |= (*rpn++) << 16;
			t |= (*rpn++) << 24;
			rpnpush(getsymvalue(t));
			pPatch->oRelocPatch |= (getsymbank(t) != -1);
			size -= 4;
			break;
		case RPN_BANK:
			/* symbol */
			t = (*rpn++);
			t |= (*rpn++) << 8;
			t |= (*rpn++) << 16;
			t |= (*rpn++) << 24;
			rpnpush(getsymbank(t));
			size -= 4;
			break;
		}
	}
	return (rpnpop());
}

void
Patch(void)
{
	struct sSection *pSect;

	pSect = pSections;
	while (pSect) {
		struct sPatch *pPatch;

		pCurrentSection = pSect;
		pPatch = pSect->pPatches;
		while (pPatch) {
			int32_t t;

			nPC = pSect->nOrg + pPatch->nOffset;
			t = calcrpn(pPatch);
			switch (pPatch->Type) {
			case PATCH_BYTE:
				if (t >= -128 && t <= 255) {
					t &= 0xFF;
					pSect->pData[pPatch->nOffset] =
					    (uint8_t) t;
				} else {
					errx(1,
					    "%s(%ld) : Value must be 8-bit",
					    pPatch->pzFilename,
					    pPatch->nLineNo);
				}
				break;
			case PATCH_WORD_L:
				if (t >= -32768 && t <= 65535) {
					t &= 0xFFFF;
					pSect->pData[pPatch->nOffset] =
						t & 0xFF;
					pSect->pData[pPatch->nOffset + 1] =
						(t >> 8) & 0xFF;
				} else {
					errx(1,
					    "%s(%ld) : Value must be 16-bit",
					    pPatch->pzFilename,
					    pPatch->nLineNo);
				}
				break;
			case PATCH_LONG_L:
				pSect->pData[pPatch->nOffset + 0] = t & 0xFF;
				pSect->pData[pPatch->nOffset + 1] =
				    (t >> 8) & 0xFF;
				pSect->pData[pPatch->nOffset + 2] =
				    (t >> 16) & 0xFF;
				pSect->pData[pPatch->nOffset + 3] =
				    (t >> 24) & 0xFF;
				break;
			}

			pPatch = pPatch->pNext;
		}

		pSect = pSect->pNext;
	}
}
