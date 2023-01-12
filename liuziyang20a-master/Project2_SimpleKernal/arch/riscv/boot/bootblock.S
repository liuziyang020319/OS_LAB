#include <asm/biosdef.h>
//# include <common.h>

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ os_size_loc, 0x502001fc

// kernel address (move kernel to here, and jmp here to start kernel)
.equ kernel, 0x50201000

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000

// APP info offset
.equ app_offset, 0x502001f0

// APP info NUM_BLOCK
.equ app_num_sector, 0x502001e8 

// APP info NUM_ID
.equ app_num_ID, 0x502001e0

// APP info, save in bss
.equ app_info, 0x52000000

.text
.global main

main:
	// fence on all memory and I/O
	fence

	// TODO: [p1-task1] call BIOS to print string "It's bootblock!"
	
	lui  a5, 	 %hi(msg) //compute address of msg
	addi a1, a5, %lo(msg)

	li a0, BIOS_PUTSTR
	call bios_func_entry

	// TODO: [p1-task2] call BIOS to read kernel in SD card

	li a0, BIOS_SDREAD
	
	lui  a5, 	 %hi(kernel) //memaddr
	addi a1, a5, %lo(kernel)

	lui a5, %hi(os_size_loc)
	addi a5, a5, %lo(os_size_loc)
	lh a2, 0(a5) //num of block to be read
	
	li a3, 1 //block id

	call bios_func_entry

	// TODO: [p1-task4] load task-related arguments and pass them to kernel
	
	//NUM_SECTOR SECTOR_ID SECTOR_offset
	li a0, BIOS_SDREAD

	lui  a1, 	 %hi(app_info) //appINFO_STORE_addr
	addi a1, a1, %lo(app_info)

	lui  a5, 	 %hi(app_num_sector) //app num sector
	addi a5, a5, %lo(app_num_sector)	
	lw a2, 0(a5) 

	lui  a5, 	 %hi(app_num_ID) //app num sector
	addi a5, a5, %lo(app_num_ID)	
	lw a3, 0(a5)

	call bios_func_entry

	// TODO: [p1-task2] jump to kernel to start UCAS-OS

	lui  a5, 	 %hi(kernel) //PC_addr
	addi a5, a5, %lo(kernel)
	jr a5   # jump to a5(kernal)

// while(1) --> stop here
stop:
	j stop
	nop

.data

msg: .string "It's a bootloader...\n\r"

