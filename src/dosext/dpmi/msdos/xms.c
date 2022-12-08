/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * Purpose: XMS realmode call interface
 *
 * Author: Stas Sergeev
 *
 */
#include "dosemu_debug.h"
#include "msdoshlp.h"
#include "dpmi_api.h"
#include "hlpmisc.h"
#include "msdos_priv.h"
#include "xms_msdos.h"

static struct dos_helper_s helper;

static void msdos_pre_xms(const cpuctx_t *scp,
	__dpmi_regs *rmreg, unsigned short rm_seg, int *r_mask)
{
    int rm_mask = *r_mask;
    x_printf("in msdos_pre_xms for function %02X\n", _HI_(ax));
    switch (_HI_(ax)) {
    case 0x0b:
	RMPRESERVE1(esi);
	SET_RMREG(ds, rm_seg);
	SET_RMLWORD(si, 0);
	MEMCPY_2DOS(SEGOFF2LINEAR(rm_seg, 0),
			SEL_ADR_CLNT(_ds_, _esi_, msdos_is_32()), 0x10);
	break;
    case 0x89:
	X_RMREG(edx) = _edx_;
	break;
    case 0x8F:
	X_RMREG(ebx) = _ebx_;
	break;
    }
    *r_mask = rm_mask;
}

static void msdos_post_xms(cpuctx_t *scp,
	const __dpmi_regs *rmreg, int *r_mask)
{
    int rm_mask = *r_mask;
    x_printf("in msdos_post_xms for function %02X\n", _HI_(ax));
    switch (_HI_(ax)) {
    case 0x0b:
	RMPRESERVE1(esi);
	break;
    case 0x88:
	_eax = X_RMREG(eax);
	_ecx = X_RMREG(ecx);
	_edx = X_RMREG(edx);
	break;
    case 0x8E:
	_edx = X_RMREG(edx);
	break;
    }
    *r_mask = rm_mask;
}

static void xms_call(const cpuctx_t *scp,
	__dpmi_regs *rmreg, unsigned short rm_seg)
{
    int rmask = (1 << cs_INDEX) |
	(1 << eip_INDEX) | (1 << ss_INDEX) | (1 << esp_INDEX);
    msdos_pre_xms(scp, rmreg, rm_seg, &rmask);
    pm_to_rm_regs(scp, rmreg, ~rmask);
}

static void xms_ret(cpuctx_t *scp, const __dpmi_regs *rmreg)
{
    int rmask = 0;
    msdos_post_xms(scp, rmreg, &rmask);
    rm_to_pm_regs(scp, rmreg, ~rmask);
    D_printf("MSDOS: XMS call return\n");
}

static void xmshlp_thr(void *arg)
{
    cpuctx_t *scp = arg;
    cpuctx_t sa = *scp;
    __dpmi_regs rmreg = {};
    unsigned short rm_seg = helper.rm_seg(scp, 0, helper.rm_arg);
    int is_32 = msdos_is_32();

    xms_call(scp, &rmreg, rm_seg);
    do_call_to(scp, is_32, get_xms_call(), &rmreg);
    *scp = sa;
    xms_ret(scp, &rmreg);
}

struct pmaddr_s get_xms_handler(void)
{
    return doshlp_get_entry(helper.entry);
}

void xmshlp_init(void)
{
    doshlp_setup_retf(&helper, "msdos xms thr", xmshlp_thr, scratch_seg, NULL);
}
