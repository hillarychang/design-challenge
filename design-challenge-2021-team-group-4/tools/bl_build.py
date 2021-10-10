"""
Bootloader Build Tool
This tool is responsible for building the bootloader from source and copying
the build outputs into the host tools directory for programming.
"""
import argparse
import os
import pathlib
import shutil
import subprocess

import secrets
from Crypto.Hash import SHA256

FILE_DIR = pathlib.Path(__file__).parent.absolute()


def copy_initial_firmware(binary_path):
    """
    Copy the initial firmware binary to the bootloader build directory
    Return:
        None
    """
    # Change into directory containing tools
    os.chdir(FILE_DIR)
    bootloader = FILE_DIR / '..' / 'bootloader'
    shutil.copy(binary_path, bootloader / 'src' / 'firmware.bin')

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

def keydecode(k):
    """
    Converts the python bytestring to a c-style char array with decimal elements
    Return:
        A string that can be pasted within curly braces in c to form a valid char[]
    """
    s = []
    for i in k:
        s.append(str(i))
    return ', '.join(s)

def make_bootloader():
    """
    Build the bootloader from source.
    Return:
        True if successful, False otherwise.
    """
    #Creating signature 
    signature = secrets.token_bytes(256)
    f = open("secret_output.txt", "w")
    f.write(decode(signature))
    f.write("\n")
    #Creating hash
    h = SHA256.new()
    h.update(signature)
    
    #creating keys and writing to secret output txt
    keys = [b"" for _ in range(200)]
    for i in range(200):
        keys[i] = secrets.token_bytes(16)
    for i in range(200):
        f.write(decode(keys[i]))
        f.write("\n")
    f.close()

    # opening bootloader and copying the before (no keys inside original bootloader.c) and creating the after (hash of signature and 200 keys inside bootloader.c)
    bc = open("../bootloader/src/bootloader.c", "r")
    after = []
    before = []
    x = 0
    for l in bc.readlines():
        before.append(l)
        if "Write Here" in l:
            # Add key
            index = l.find('{}')
            index = index + 1
            final = l[:index] 
            final = final + keydecode(keys[x]) 
            final = final + l[index:]
            after.append(final)
            x += 1
        elif "Hash Here" in l:
			# Add hash
            index = l.find('{}')
            index += 1
            final = l[:index] 
            final = final + keydecode(h.digest()) 
            final = final + l[index:]
            after.append(final)
        else:
            after.append(l)
    bc.close()

    # rewriting bootloader.c to the after that was created in the step above
    bc = open("../bootloader/src/bootloader.c", "w")
    for i in after:
        bc.write(i)
    bc.close()

    # making bootloader
    # Change into directory containing bootloader.
    bootloader = FILE_DIR / '..' 
    bootloader = bootloader / 'bootloader'
    os.chdir(bootloader)

    subprocess.call('make clean', shell=True)
    status = subprocess.call('make')

    #opening bootloader.c and removing all the keys/reverting it
    bc = open("../bootloader/src/bootloader.c", "w")
    for i in before:
        bc.write(i)
    bc.close()

    # Return True if make returned 0, otherwise return False.
    return (status == 0)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Bootloader Build Tool')
    parser.add_argument("--initial-firmware",
                        help="Path to the the firmware binary.",
                        default=None)
    args = parser.parse_args()
    if args.initial_firmware is None:
        binary_path = FILE_DIR / '..' / 'firmware' / 'firmware' / 'gcc' / 'main.bin'
    else:
        binary_path = os.path.abspath(pathlib.Path(args.initial_firmware))

    if not os.path.isfile(binary_path):
        raise FileNotFoundError(
            "ERROR: {} does not exist or is not a file. You may have to call \"make\" in the firmware directory."
            .format(binary_path))

    copy_initial_firmware(binary_path)
    make_bootloader()
