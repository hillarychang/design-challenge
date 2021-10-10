// Hardware Imports
#include "inc/hw_memmap.h" // Peripheral Base Addresses
#include "inc/lm3s6965.h" // Peripheral Bit Masks and Registers
#include "inc/hw_types.h" // Boolean type
#include "inc/hw_ints.h" // Interrupt numbers

// Driver API Imports
#include "driverlib/flash.h" // FLASH API
#include "driverlib/sysctl.h" // System control API (clock/reset)
#include "driverlib/interrupt.h" // Interrupt API

// Application Imports
#include "uart.h"
#include "beaverssl.h"

// Forward Declarations
void load_initial_firmware(void);
void load_firmware(void);
void boot_firmware(void);
long program_flash(uint32_t, unsigned char*, unsigned int);

// TODO: Write this in bl build
unsigned char signaturehash[32] = {} /* Hash Here */;
unsigned char keys[3][17] = {
    /* Write Here */ {},
    /* Write Here */ {},
    /* Write Here */ {}
};


// Firmware Constants
#define METADATA_BASE 0xFC00  // base address of version and firmware size in Flash
#define FW_BASE 0x10000  // base address of firmware in Flash


// FLASH Constants
#define FLASH_PAGESIZE 1024
#define FLASH_WRITESIZE 4


// Protocol Constants
#define OK    ((unsigned char)0x00)
#define ERROR ((unsigned char)0x01)
#define UPDATE ((unsigned char)'U')
#define BOOT ((unsigned char)'B')


// Firmware v2 is embedded in bootloader
extern int _binary_firmware_bin_start;
extern int _binary_firmware_bin_size;


// Device metadata
uint16_t *fw_version_address = (uint16_t *) (METADATA_BASE );
uint16_t *fw_size_address = (uint16_t *) (METADATA_BASE + 2);
uint8_t *fw_release_message_address;



int main(void) {

  // Initialize UART channels
  // 0: Reset
  // 1: Host Connection
  // 2: Debug
  uart_init(UART0);
  uart_init(UART1);
  uart_init(UART2);

  // Enable UART0 interrupt
  IntEnable(INT_UART0);
  IntMasterEnable();

  load_initial_firmware();

  uart_write_str(UART2, "Welcome to the BWSI Vehicle Update Service!\n");
  uart_write_str(UART2, "Send \"U\" to update, and \"B\" to run the firmware.\n");
  uart_write_str(UART2, "Writing 0x20 to UART0 will reset the device.\n");

  int resp;
  while (1){
    uint32_t instruction = uart_read(UART1, BLOCKING, &resp);
    if (instruction == UPDATE){
      uart_write_str(UART1, "U");
      load_firmware();
    } else if (instruction == BOOT){
      uart_write_str(UART1, "B");
      boot_firmware();
    }
  }
}


/*
 * Load initial firmware into flash
 */
void load_initial_firmware(void) {
    if (*((uint32_t*)(METADATA_BASE)) != 0xFFFFFFFF){
    /*
     * Default Flash startup state in QEMU is all zeros since it is
     * secretly a RAM region for emulation purposes. Only load initial
     * firmware when metadata page is all zeros. Do this by checking
     * 4 bytes at the half-way point, since the metadata page is filled
     * with 0xFF after an erase in this function (program_flash()).
     */
    return;
  }

  int size = (int)&_binary_firmware_bin_size;
  int *data = (int *)&_binary_firmware_bin_start;
    
  uint16_t version = 2;
  uint32_t metadata = (((uint16_t) size & 0xFFFF) << 16) | (version & 0xFFFF);
  program_flash(METADATA_BASE, (uint8_t*)(&metadata), 4);
  fw_release_message_address = (uint8_t *) "This is the initial release message.";
    
  int i = 0;
  for (; i < size / FLASH_PAGESIZE; i++){
       program_flash(FW_BASE + (i * FLASH_PAGESIZE), ((unsigned char *) data) + (i * FLASH_PAGESIZE), FLASH_PAGESIZE);
  }
  program_flash(FW_BASE + (i * FLASH_PAGESIZE), ((unsigned char *) data) + (i * FLASH_PAGESIZE), size % FLASH_PAGESIZE);
}


/*
 * Load the firmware into flash.
 */
void load_firmware(void)
{
     // maybe useful variables
    int read = 0;
    uint32_t rcv = 0;

    unsigned char ehash[32];
    unsigned char en[128];
    uint32_t version = 0;
    uint32_t firm_size = 0;
    unsigned char hash[32];
    uint32_t message_size = 0;
    // Authentication check
    unsigned char signature[256];
    unsigned char data[2000];
    for(int i = 0;i<274;i++){
        data[i] = uart_read(UART1, BLOCKING, &read);
    }
    
    for( int i=0 ; i < 256 ; i++) {
        signature[i] = data[i];
    }
    
    int kn;
     kn = data[256];
    kn |= data[257] << 8;
    
    unsigned char iv[16];
    for( int i = 0; i < 16; i++) {
        iv[i] = data[i + 258];
    }
    
    aes_decrypt((char *)keys[kn], iv, signature, 256);
    unsigned char sh[32];
    sha_hash((unsigned char*)signature, 256, sh);
    int authentic_sender = 1;
    for(int i = 0; i < 32; i++) {
        if(sh[i] != signaturehash[i]) {
            authentic_sender = 0;
        }
    }
    if(authentic_sender) {
        uart_write(UART1, OK);
    } else {
        uart_write(UART1, ERROR);
        return;
    }
    // RECEIVING METADATA STUFF
    // receive version
    rcv = uart_read(UART1, BLOCKING, &read);
    version = (uint32_t)rcv;
    rcv = uart_read(UART1, BLOCKING, &read);
    version |= (uint32_t)rcv << 8;

    // receive firmware size
    rcv = uart_read(UART1, BLOCKING, &read);
    firm_size = (uint32_t)rcv;
    rcv = uart_read(UART1, BLOCKING, &read);
    firm_size |= (uint32_t)rcv << 8;

    // receive message size
    rcv = uart_read(UART1, BLOCKING, &read);
    message_size = (uint32_t)rcv;
    rcv = uart_read(UART1, BLOCKING, &read);
    message_size |= (uint32_t)rcv << 8;
    uint16_t old_version = *fw_version_address;
    if(version != 0 && version < old_version) {
        uart_write(UART1, ERROR); // Reject the metadata.
        SysCtlReset();                    // Reset device
        return;
    } else if(version == 0) {
        // If debug firmware, don't change version
        version = old_version;
    }
    long long metadata = (message_size & 0xffff) << 32 | (firm_size & 0xffff) << 16 | (version & 0xffff);
    program_flash(METADATA_BASE, (uint8_t*)(&metadata), 6);
    uart_write(UART1, OK);
    fw_release_message_address = (uint8_t *) (FW_BASE + firm_size); 
    // Read Frames + integrity checks
    int fsize=-1;
    int idx = 0;
    int paddr = FW_BASE;
    do{
        // read in 1 frame
        kn = (uart_read(UART1, BLOCKING, &read)  | (uart_read(UART1, BLOCKING, &read) << 8));
        fsize = (uart_read(UART1, BLOCKING, &read)  | (uart_read(UART1, BLOCKING, &read) << 8));
        if(fsize==0){
            break;
        }
        for(int i = 0;i<fsize;i++){
            en[i] = uart_read(UART1, BLOCKING, &read);
        }
        for(int i = 0;i<32;i++){
            hash[i] = uart_read(UART1, BLOCKING, &read);
        }
        for(int i = 0;i<16;i++){
            iv[i] = uart_read(UART1, BLOCKING, &read);
        }
        //decrypt frame and verify integrity
        aes_decrypt(keys[kn],iv,en,fsize);
        sha_hash(en,fsize,ehash);
        int intcheck = 1;
        for(int i = 0;i<32;i++){
            if(hash[i]!=ehash[i]){
                intcheck = 0;
            }
        }
        if(intcheck){
            uart_write(UART1, OK);
            for(int i = 0;i<fsize;i++){
                data[idx] = en[i];
                idx++;
            }
            
            if(idx  >= FLASH_PAGESIZE){
                // program data to flash
                program_flash(paddr, data, idx);
                idx = 0;
                paddr += FLASH_PAGESIZE;
            }
        }
        else{
            uart_write(UART1, ERROR);
            return;
        }
    }while(fsize!=0);
    program_flash(paddr, data, idx);
}


/*
 * Program a stream of bytes to the flash.
 * This function takes the starting address of a 1KB page, a pointer to the
 * data to write, and the number of byets to write.
 *
 * This functions performs an erase of the specified flash page before writing
 * the data.
 */
long program_flash(uint32_t page_addr, unsigned char *data, unsigned int data_len)
{
  uint32_t word = 0;
  int ret;
  int i;

  // Erase next FLASH page
  FlashErase(page_addr);

  // Clear potentially unused bytes in last word
  // If data not a multiple of 4 (word size), program up to the last word
  // Then create temporary variable to create a full last word
  if (data_len % FLASH_WRITESIZE){
    // Get number of unused bytes
    int rem = data_len % FLASH_WRITESIZE;
    int num_full_bytes = data_len - rem;
    
    // Program up to the last word
    ret = FlashProgram((unsigned long *)data, page_addr, num_full_bytes);
    if (ret != 0) {
      return ret;
    }
    
    // Create last word variable -- fill unused with 0xFF
    for (i = 0; i < rem; i++) {
      word = (word >> 8) | (data[num_full_bytes+i] << 24); // Essentially a shift register from MSB->LSB
    }
    for (i = i; i < 4; i++){
      word = (word >> 8) | 0xFF000000;
    }
    
    // Program word
    return FlashProgram(&word, page_addr+num_full_bytes, 4);
  } else{
    // Write full buffer of 4-byte words
    return FlashProgram((unsigned long *)data, page_addr, data_len);
  }
}

void boot_firmware(void)
{
  uart_write_str(UART2, (char *) fw_release_message_address);

  // Boot the firmware
    __asm(
    "LDR R0,=0x10001\n\t"
    "BX R0\n\t"
  );
}