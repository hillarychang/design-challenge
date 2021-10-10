# FW Protect
## Overview
1. [Read in Secrets](#step-1)
2. [Encrypt Signature](#step-2)
3. [Create Metadata](#step-3)
4. [Chunk the data](#step-4)
5. [Encrypt the chunks](#step-5)
6. [Write to File](#step-6)

## Step 1
Read in the data from secret_output.txt, and split it up by lines into the signature and the keys.

## Step 2
Choose a random number from 0-2 to decide which key to use for the signature. When a suitable key is found, encrypt the signature using that key, then save the number of the key
used and the IV, so that the C code will be able to decrypt it.

## Step 3
The metadata is in the format of 3 little endian shorts, in the following order 
1. Version number
2. Size of the Firmware
3. Size of the Release Message

## Step 4
Chunk the data using the chunk function, which takes a maximum size of 128 bytes from the message at a time, and keep the chunked data

## Step 5
Calculate the hash of the chunk, and save it. Using the same key choosing principle as [Step 2](#step-2), choose a key and encrypt it.
Save the key index, the length of the data, the encrypted data, the hash, and the iv as one unit

## Step 6
Take all the units, including the signature, the metadata, and the chunks, and write it to the outfile


