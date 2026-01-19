/*
 *  32bitbios - jumptable for those function reachable from 16bit area
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * Copyright (C) IBM Corporation, 2006
 *
 * Author: Stefan Berger <stefanb@us.ibm.com>
 */

// #include "rombios_compat.h"

/*
 * Compatibility functions and structures for transitioning between
 * 16 bit Bochs BIOS and 32 bit BIOS code.
 */

#define ADDR_FROM_SEG_OFF(seg, off)  (void *)((((uint32_t)(seg)) << 4) + (off))

typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;

typedef uint8_t  Bit8u;
typedef uint16_t Bit16u;
typedef uint32_t Bit32u;

#define SetCF(x)   (x)->u.r8.flagsl |= 0x01
#define SetZF(x)   (x)->u.r8.flagsl |= 0x40
#define ClearCF(x) (x)->u.r8.flagsl &= 0xfe
#define ClearZF(x) (x)->u.r8.flagsl &= 0xbf
#define GetCF(x)   ((x)->u.r8.flagsl & 0x01)

#define SET_CF()     *FLAGS |= 0x0001
#define CLEAR_CF()   *FLAGS &= 0xfffe
#define GET_CF()     (*FLAGS & 0x0001)

#define SET_ZF()     *FLAGS |= 0x0040
#define CLEAR_ZF()   *FLAGS &= 0xffbf


typedef struct {
 union {
  struct {
    Bit32u edi, esi, ebp, esp;
    Bit32u ebx, edx, ecx, eax;
    } r32;
  struct {
    Bit16u di, filler1, si, filler2, bp, filler3, sp, filler4;
    Bit16u bx, filler5, dx, filler6, cx, filler7, ax, filler8;
    } r16;
  struct {
    Bit32u filler[4];
    Bit8u  bl, bh;
    Bit16u filler1;
    Bit8u  dl, dh;
    Bit16u filler2;
    Bit8u  cl, ch;
    Bit16u filler3;
    Bit8u  al, ah;
    Bit16u filler4;
    } r8;
  } u;
} __attribute__((packed)) pushad_regs_t;



static inline Bit32u read_dword(Bit16u seg, Bit16u off)
{
	uint32_t *addr = (uint32_t *)ADDR_FROM_SEG_OFF(seg,off);
	return *addr;
}

static inline Bit16u read_word(Bit16u seg, Bit16u off)
{
	uint16_t *addr = (uint16_t *)ADDR_FROM_SEG_OFF(seg,off);
	return *addr;
}

static inline Bit8u read_byte(Bit16u seg, Bit16u off)
{
	uint8_t *addr = (uint8_t *)ADDR_FROM_SEG_OFF(seg,off);
	return *addr;
}

static inline void write_dword(Bit16u seg, Bit16u off, Bit32u val)
{
	uint32_t *addr = (uint32_t *)ADDR_FROM_SEG_OFF(seg,off);
	*addr = val;
}

static inline void write_word(Bit16u seg, Bit16u off, Bit16u val)
{
	uint16_t *addr = (uint16_t *)ADDR_FROM_SEG_OFF(seg,off);
	*addr = val;
}

static inline void write_byte(Bit16u seg, Bit16u off, Bit8u val)
{
	uint8_t *addr = (uint8_t *)ADDR_FROM_SEG_OFF(seg,off);
	*addr = val;
}

#define X(idx, ret, fn, args...) ret fn(args);
#include "test2015_88.h"
#undef X

/* The bug here is that the expansion of the macro happens in the
   asm command and then the #include is redundantly output; causing
   the error.  This is a famous issue in the interplay between 
   the AST and the #include directives.  Not clear if there is
   much to do about this unless the seperate strings in this case
   can be seen in the ASt (which I think is not possible).
 */
asm (
    "    .text                       \n"
    "     movzwl %bx,%eax            \n"
    "     jmp *jumptable(,%eax,4)    \n"
    "    .data                       \n"
    "jumptable:                      \n"
#define X(idx, ret, fn, args...) " .long " #fn "\n"
#include "test2015_88.h"
#undef X
    );
