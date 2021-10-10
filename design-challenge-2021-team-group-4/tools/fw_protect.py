"""
Firmware Bundle-and-Protect Tool

"""
from Crypto.Cipher import AES
from Crypto.Random import get_random_bytes
from Crypto.Util.Padding import pad
import argparse
import time
import random
import struct
import hashlib


def chunk(message):
    cs = []
    while len(message) > 0:
        size = 128
        if size > len(message):
            size = len(message)
        c = b""
        for i in range(size):
            print(i , len(c))
            print(message[i])
            print(chr(message[i]).encode())
            assert(i==len(c))
            c += chr(message[i]).encode()
            
        message = message[size:]
        cs.append(c)
        print(size)
    return cs

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

def protect_firmware(infile, outfile, version, message):
    # Load firmware binary from infile
    with open(infile, 'rb') as fp:
        firmware = fp.read()

    # Append null-terminated message to end of firmware
    firmware_and_message = firmware + message.encode()
    lines = [encode(i.strip()) for i in open("secret_output.txt", "r").readlines()] #read data from secret_output.txt
    signature = lines[0] 	#split data into signature
    keys = lines[1:] 		#split data into keys
    random.seed(time.time())
    kn = random.randint(0, 2)
    k = keys[kn]
    aes = AES.new(k, AES.MODE_CBC)
    auth = aes.encrypt(signature) + struct.pack(
        "<h", kn) + aes.IV     
    # Pack version and size into three little-endian shorts
    metadata = struct.pack('<HHH', version, len(firmware), len(message))
	# Init data
    data = [auth, metadata]
    bl = open("../bootloader/src/bootloader.c","r").readlines()
    for i in range(0,len(firmware_and_message),128):
        c = firmware_and_message[i:i+128]
		# encrypting the chunks, and putting the data in order
        kn = random.randint(0, 2) 
        k = keys[kn]
        a = AES.new(k, AES.MODE_CBC, iv=get_random_bytes(16))
        en = b""
        if len(c) %16 == 0:
            en = a.encrypt(c)
        else:
#             en = a.encrypt(pad(c, AES.block_size))
            c = c + b'\x00' * (16 - len(c)%16)
            en = a.encrypt(c)
        hash = hashlib.sha256(c).digest() #hash of chunk
        l = len(en)
        d = b""
        d += struct.pack("<hh", kn, l)
        d += en
        d += hash
        d += a.IV
        data.append(d)

    # Write firmware blob to outfile
    with open(outfile, 'w') as of:
        convdata = [decode(i) + '\n' for i in data]
        of.writelines(convdata)
        


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Firmware Update Tool')
    parser.add_argument("--infile",
                        help="Path to the firmware image to protect.",
                        required=True)
    parser.add_argument("--outfile",
                        help="Filename for the output firmware.",
                        required=True)
    parser.add_argument("--version",
                        help="Version number of this firmware.",
                        required=True)
    parser.add_argument("--message",
                        help="Release message for this firmware.",
                        required=True)
    args = parser.parse_args()

    protect_firmware(infile=args.infile,
                     outfile=args.outfile,
                     version=int(args.version),
                     message=args.message)
