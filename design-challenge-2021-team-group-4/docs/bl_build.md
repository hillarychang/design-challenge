# BL Build
## Overview
1. [Generating signature and keys](#step-1)
2. [Writing to secret file](#step-2)
3. [Adding secrets to bootloader.c](#step-3)
4. [Compiling](#step-4)
5. [Remove secrets from bootloader.c](#step-5)

## Step 1
Using the python [secrets](https://docs.python.org/3/library/secrets.html) module, generate a 256 byte signature that will be used for verification,
and 200 separate 16 byte keys used for AES encryption. Also generate the SHA256 hash of the signature.

## Step 2
Separating each entry with a new line, write the signature, then each of the 3 keys, to the secret file for firmware protect.

## Step 3
Open bootloader.c as a file, and search for the appropriate places to put the hash of the signature (marked by `Hash Here`), and
each of the 3 keys (marked by `Write Here`)

## Step 4
Now compile the bootloader by changing directories to the bootloader directory, and invoking the following commands
```console
make clean
make
```

## Step 5
Finally, remove all confidential information added in [step 3](#step-3). Do this by finding the markers for the hash and keys, and
erasing within the braces.


