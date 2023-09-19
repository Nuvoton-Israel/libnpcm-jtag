# libnpcm-jtag

Provide APIs to access CPLD via JTAG interface.

# loadsvf

Download CPLD firmware(in SVF format) to device through JTAG interface on NPCM7xx/NPCM8xx BMC.

## Usage

```bash
loadsvf -d <jtag_device> -s <svf_file>
        [-l <log_level> -m <transfer_mode> -f <frequency> -g]
```

**-d jtag_device:**  
specify the jtag device node <ex: /dev/jtag_drv>

**-s svf_file:**  
specify the svf file path  

**-l loglevel:**  
display the log whose level is large or equal to the specified loglevel
LOG LEVEL:  
1: Debug  
2: Info  
3: Error  

**-m transfer mode:(for Poleg only)**  
1: PSPI mode (default if not specified)  
0: GPIO mode  

**-f frequency:**  
force running at specific frequency in Mhz for PSPI mode.  
the accepted frequency is 1 ~ 50  

**-g:**  
execute svf command line by line  


# jtag_rw

Send instruction, read/write data to CPLD target via JTAG.

## Usage

```bash
Usage: jtag_rw [option(s)]
  -d <device>           jtag device
  -c <command>          send 8-bit cmd byte
  -w <data>             write data
  -l <data bit length>  data bit length
  -t <tcks>             runtest idle
  -r                    print received data
```

## Example

Read id code (send command 0xe0, read 32 bits data)
```bash
jtag_rw -d /dev/jtag0 -c 0xe0 -l 32 -r
Recv:
43 c0 2b 61
```

Enable the flash (send command 0xc6, write 8 bits data(0x8), runtest 2 tcks)
```bash
jtag_rw -d /dev/jtag0 -c 0xc6 -w 8 -l 8 -t 2
```

Write user code (send command 0xc0, write 32 bits usercode, send command 0xc2, runtest 2 tcks)
```bash
jtag_rw -d /dev/jtag0 -c 0xc0 -w 0,0,0x0a,0x01 -l 32
jtag_rw -d /dev/jtag0 -c 0xc2 -t 2
```

Read user code
```bash
jtag_rw -d /dev/jtag0 -c 0xc0 -l 32 -r
Recv:
00 00 0a 01
```

