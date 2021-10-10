# Bootloader.c
## Main
1. [Initialize UARTS](#main-step-1)
2. [Load initial firmware](#main-step-2)
3. [Wait for instructions (Boot or Update)](#main-step-3)
### [If Booting (Boot_firmware)](#Boot_Firmware)
### [If Updating (Load_firmware)](#Load_Firmware)

## Main Step 1
Initialize the UARTs (reset, host, and debug) using uart_init. Also enable UART0 interrupt.
## Main Step 2
1. Check to see if metadata page is all zeros by checking 4 bytes at the half-way point, since the metadata page is filled with 0xFF later in this function. 
2. Calculate and store the size of initial firmware (version 2) and the actual firmware itself. Set the version number to 2.
3. Set the metadata using the previously defined size and version. Program metadata to flash.
4. Store the initial release message ("This is the initial release message")
5. Program the firmware to flashs

## Main Step 3
Wait until 'U' (update) or 'B' (boot) is received through UART1 (host). If a 'U' is received, [begin updating](#load_Firmware). If a 'B' is received, [boot the firmware](#boot_firmware).

## Boot_Firmware
Write the release message through a series of uart_write's and then boot the firmware using:

```
__asm("LDR R0,=0x10001\n\t"
                  "BX R0\n\t");
```
## Load_Firmware
This function serves to load in the firmware from the FW_update tool. In chronological order, it:
1. Performs the authentication check
    1. Read in the encrypted signature (aes)
    2. Read in the key number used
    3. Read in the iv used
    4. Decrypt the signature and compute the sha256 hash of it (store this in sh)
    5. Compare the computed hash (sh) and the preprogrammed hash (signaturehash)
    6. If they are the same, continue to the next step, if not, end.
3. Receives the metadata
    1. Receives the version, firm_size, and message_size as little endian shorts
    2. Put these variables all into the long long "metadata"
    3. Program the metadata into flash and compute the release message address
5. Read in frames, perform integrity checks, and write to flash
    1. Create data array which will store all the firmware until all the integrity checks are performed
    2. For each frame (while frame length isn't 0)
         1. Read in the key number used, the size of the frame, the encrypted data, the hash, and the iv
         2. Decrypt the data using the correct key and iv
         3. Compute the hash and compare it to the received hash
         4. If the hashes are different, end. If not, add the decrypted data to the data array (using the variable idx to keep track of where to put it in the data array)
    3. If the data array index reaches FLASH_PAGESIZE (1024), program the data array to flash and set index back to 0.
