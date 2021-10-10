#!/usr/bin/env python
"""
Firmware Updater Tool

A frame consists of two sections:
1. Two bytes for the length of the data section
2. A data section of length defined in the length section

[ 0x02 ]  [ variable ]
--------------------
| Length | Data... |
--------------------

In our case, the data is from one line of the Intel Hex formated .hex file

We write a frame to the bootloader, then wait for it to respond with an
OK message so we can write the next frame. The OK message in this case is
just a zero
"""





import argparse
import struct
import time

from serial import Serial

RESP_OK = b'\x00'
FRAME_SIZE = 16

debug = False


def decode(k):
    """
    Converts the python bytestring to a string containing decimals
    Return:
        Decoded string
    """
    s = []
    for i in k:
        s.append(str(i))
    return ' '.join(s)

def encode(m):
	"""
	Converts a string of space separated decimal ints to a bytestring
	Return:
		The bytestring
	"""
	k = b""
	l = [int(i) for i in m.split(" ")]
	for i in l:
		k += bytes([i])
	return k
def main(ser, infile, debug):
	# read in what do send
    fp = open(infile, 'r')
    firmware_lines = [encode(i) for i in fp.readlines()]
    count = 0 
	
	# Handshake
    ser.write(b'U')
    resp = b'asdf'
    while resp != b'U':
        resp = ser.read()
	
    for line in firmware_lines:
		# Send one line of data to bootloader
#         ser.write(line)
        for i in range(0,len(line),4):
            ser.write(line[i:i+4])
            time.sleep(0.02)
        resp = ser.read()
        # Wait for an OK from the bootloader.
        if resp != RESP_OK:
            raise RuntimeError("ERROR: Bootloader responded with {}".format(repr(resp)))
        
        
        count += 1
        

    
    ser.write(struct.pack('>Hh', 0x0000,0x0000))  #send zero bytes 

    
    return ser


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Firmware Update Tool')

    parser.add_argument("--port", help="Serial port to send update over.",
                        required=True)
    parser.add_argument("--firmware", help="Path to firmware image to load.",
                        required=True)
    parser.add_argument("--debug", help="Enable debugging messages.",
                        action='store_true')
    args = parser.parse_args()

    print('Opening serial port...')
    ser = Serial(args.port, baudrate=115200)
    main(ser=ser, infile=args.firmware, debug=args.debug)

