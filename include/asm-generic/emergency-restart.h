#ifndef _ASM_GENERIC_EMERGENCY_RESTART_H
#define _ASM_GENERIC_EMERGENCY_RESTART_H

static inline void machine_emergency_restart(void)
{
#ifdef CONFIG_MACH_QSD8X50_S1
		machine_restart("oem-0"); //silent reset
#else
		machine_restart(NULL);
#endif
}

#endif /* _ASM_GENERIC_EMERGENCY_RESTART_H */
