/*
*
*	Custom PS3 Save Decrypters - (c) 2020 by Bucanero - www.bucanero.com.ar
*
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <polarssl/aes.h>
#include <polarssl/des.h>
#include <polarssl/md5.h>
#include <polarssl/blowfish.h>

#include "types.h"
#include "keys.h"


void blowfish_ecb_decrypt(u8* data, u32 len, u8* key, u32 key_len)
{
	blowfish_context ctx;

	LOG("Decrypting Blowfish ECB data (%d bytes)", len);

	blowfish_init(&ctx);
	blowfish_setkey(&ctx, key, key_len * 8);
	len = len / BLOWFISH_BLOCKSIZE;

	while (len--)
	{
		blowfish_crypt_ecb(&ctx, BLOWFISH_DECRYPT, data, data);
		data += BLOWFISH_BLOCKSIZE;
	}

	return;
}

void blowfish_ecb_encrypt(u8* data, u32 len, u8* key, u32 key_len)
{
	blowfish_context ctx;

	LOG("Encrypting Blowfish ECB data (%d bytes)", len);

	blowfish_init(&ctx);
	blowfish_setkey(&ctx, key, key_len * 8);
	len = len / BLOWFISH_BLOCKSIZE;

	while (len--)
	{
		blowfish_crypt_ecb(&ctx, BLOWFISH_ENCRYPT, data, data);
		data += BLOWFISH_BLOCKSIZE;
	}

	return;
}

void diablo_decrypt_data(u8* data, u32 size)
{
	u32 xor_key1 = DIABLO3_KEY1;
	u32 xor_key2 = DIABLO3_KEY2;
	u32 tmp;

	LOG("[*] Total Decrypted Size 0x%X (%d bytes)", size, size);

	for (int i = 0; i < size; i++)
	{
		data[i] ^= (xor_key1 & 0xFF);
		tmp = data[i] ^ xor_key1;
		xor_key1 = xor_key1 >> 8 | xor_key2 << 0x18;
		xor_key2 = xor_key2 >> 8 | tmp << 0x18;
	}

	LOG("[*] Decrypted File Successfully!");
	return;
}

void diablo_encrypt_data(u8* data, u32 size)
{
	u32 xor_key1 = DIABLO3_KEY1;
	u32 xor_key2 = DIABLO3_KEY2;
	u32 tmp;

	LOG("[*] Total Encrypted Size 0x%X (%d bytes)", size, size);

	for (int i = 0; i < size; i++)
	{
		tmp = data[i] ^ xor_key1;
		data[i] ^= (xor_key1 & 0xFF);
		xor_key1 = xor_key1 >> 8 | xor_key2 << 0x18;
		xor_key2 = xor_key2 >> 8 | tmp << 0x18;
	}

	LOG("[*] Encrypted File Successfully!");
	return;
}

void aes_ecb_decrypt(u8* data, u32 len, u8* key, u32 key_len)
{
	aes_context ctx;

	LOG("Decrypting AES ECB data (%d bytes)", len);

	aes_init(&ctx);
	aes_setkey_dec(&ctx, key, key_len * 8);
	len = len / AES_BLOCK_SIZE;

	while (len--)
	{
		aes_crypt_ecb(&ctx, AES_DECRYPT, data, data);
		data += AES_BLOCK_SIZE;
	}

	return;
}

void aes_ecb_encrypt(u8* data, u32 len, u8* key, u32 key_len)
{
	aes_context ctx;

	LOG("Encrypting AES ECB data (%d bytes)", len);

	aes_init(&ctx);
	aes_setkey_enc(&ctx, key, key_len * 8);
	len = len / AES_BLOCK_SIZE;

	while (len--)
	{
		aes_crypt_ecb(&ctx, AES_ENCRYPT, data, data);
		data += AES_BLOCK_SIZE;
	}

	return;
}

void des_ecb_decrypt(u8* data, u32 len, u8* key, u32 key_len)
{
	des_context ctx;

	if (key_len != DES_KEY_SIZE)
		return;

	LOG("Decrypting DES ECB data (%d bytes)", len);

	des_init(&ctx);
	des_setkey_dec(&ctx, key);
	len = len / DES_BLOCK_SIZE;

	while (len--)
	{
		des_crypt_ecb(&ctx, data, data);
		data += DES_BLOCK_SIZE;
	}

	return;
}

void des_ecb_encrypt(u8* data, u32 len, u8* key, u32 key_len)
{
	des_context ctx;

	if (key_len != DES_KEY_SIZE)
		return;

	LOG("Encrypting DES ECB data (%d bytes)", len);

	des_init(&ctx);
	des_setkey_enc(&ctx, key);
	len = len / DES_BLOCK_SIZE;

	while (len--)
	{
		des_crypt_ecb(&ctx, data, data);
		data += DES_BLOCK_SIZE;
	}

	return;
}

void des3_cbc_decrypt(u8* data, u32 len, const u8* key, u32 key_len, u8* iv, u32 iv_len)
{
	des3_context ctx;

	if (key_len != DES_KEY_SIZE*3 || iv_len != DES_KEY_SIZE)
		return;

	LOG("Decrypting 3-DES CBC data (%d bytes)", len);

	des3_init(&ctx);
	des3_set3key_dec(&ctx, key);
	des3_crypt_cbc(&ctx, DES_DECRYPT, len, iv, data, data);

	return;
}

void des3_cbc_encrypt(u8* data, u32 len, const u8* key, u32 key_len, u8* iv, u32 iv_len)
{
	des3_context ctx;

	if (key_len != DES_KEY_SIZE*3 || iv_len != DES_KEY_SIZE)
		return;

	LOG("Encrypting 3-DES CBC data (%d bytes)", len);

	des3_init(&ctx);
	des3_set3key_enc(&ctx, key);
	des3_crypt_cbc(&ctx, DES_ENCRYPT, len, iv, data, data);

	return;
}

void xor_block(const u8* in, u8* out)
{
	for (int i = 0; i < XOR_BLOCK_SIZE; i++)
		out[i] ^= in[i];
}

void nfsu_decrypt_data(u8* data, u32 size)
{
	u8 xor_key[XOR_BLOCK_SIZE];
	u8 tmp[XOR_BLOCK_SIZE];

	LOG("[*] Total Decrypted Size Is 0x%X (%d bytes)", size, size);

	// init xor key
	memcpy(xor_key, NFS_XOR_KEY, XOR_BLOCK_SIZE);
	xor_block(data, xor_key);
	md5(xor_key, XOR_BLOCK_SIZE, xor_key);

	size /= XOR_BLOCK_SIZE;
	
	while (size--)
	{
		data += XOR_BLOCK_SIZE;

		md5(data, XOR_BLOCK_SIZE, tmp);
		xor_block(xor_key, data);
		memcpy(xor_key, tmp, XOR_BLOCK_SIZE);
	}

	return;
}

void nfsu_encrypt_data(u8* data, u32 size)
{
	u8 xor_key[XOR_BLOCK_SIZE];

	LOG("[*] Total Encrypted Size Is 0x%X (%d bytes)", size, size);

	// init xor key
	memcpy(xor_key, NFS_XOR_KEY, XOR_BLOCK_SIZE);
	xor_block(data, xor_key);
	md5(xor_key, XOR_BLOCK_SIZE, xor_key);

	size /= XOR_BLOCK_SIZE;

	while (size--)
	{
		data += XOR_BLOCK_SIZE;

		xor_block(xor_key, data);
		md5(data, XOR_BLOCK_SIZE, xor_key);
	}

	return;
}

void sh3_decrypt_data(u8* data, u32 size)
{
	u32 out;
	u64 input, key2 = SH3_KEY2;

	LOG("[*] Total Decrypted Size Is 0x%X (%d bytes)", size, size);

	size /= 4;

	while (size--)
	{
		input = *(u32*) data;
		out = (u32)((input ^ (u64)(key2 - SH3_KEY1)) & 0xFFFFFFFF);
		memcpy(data, &out, sizeof(u32));

		key2 = (input << 5 | input >> 27) + (u64)SH3_KEY2;
		data += 4;
	}

	return;
}

void sh3_encrypt_data(u8* data, u32 size)
{
	u32 out;
	u64 input, key2 = SH3_KEY2;

	LOG("[*] Total Encrypted Size Is 0x%X (%d bytes)", size, size);

	size /= 4;

	while (size--)
	{
		input = *(u32*) data;
		out = (u32)((input ^ (u64)(key2 - SH3_KEY1)) & 0xFFFFFFFF);
		memcpy(data, &out, sizeof(u32));

		key2 = (u64)(out << 5 | out >> 27) + (u64)SH3_KEY2;
		data += 4;
	}

	return;
}

void ff13_init_key(u8* key_table, u32 ff_game, const u64* kdata)
{
	u32 init[2];
	u64 ff_key = FFXIII_KEY;

	if (ff_game > 1)
	{
		ff_key = (ff_game == 2) ? FFXIII_2_KEY : FFXIII_3_KEY;
		ff_key ^= (kdata[0] ^ kdata[1]) | 1L;
	}

	ff_key = ((ff_key & 0xFF00000000000000) >> 16) | ((ff_key & 0x0000FF0000000000) << 16) | (ff_key & 0x00FF00FFFFFFFFFF);
	ff_key = ((ff_key & 0x00000000FF00FF00) >>  8) | ((ff_key & 0x0000000000FF00FF) <<  8) | (ff_key & 0xFFFFFFFF00000000);
	ff_key = ES64(ff_key);

	memcpy(key_table, &ff_key, sizeof(uint64_t));

	key_table[0] += 0x45;
	
	for (int j = 1; j < 8; j++)
	{
		init[0] = key_table[j-1] + key_table[j];
		init[1] = ((key_table[j-1] << 2) & 0x3FC);
		key_table[j] = ((((init[0] + 0xD4) ^ init[1]) & 0xFF) ^ 0x45);
	}

	for (int j = 0; j < 31; j++)
	{
		ff_key = ES64(*(u64*)(key_table + j*8));
		ff_key = ff_key + (u64)((ff_key << 2) & 0xFFFFFFFFFFFFFFFC);
		ff_key = ES64(ff_key);

		memcpy(key_table + (j+1)*8, &ff_key, sizeof(uint64_t));
	}
}

u32 ff13_checksum(const u8* bytes, u32 len)
{
	u32 ff_csum = 0;
	len /= 4;

	while (len--)
	{
		ff_csum += bytes[0];
		bytes += 4;
	}

	return (ff_csum);
}

void ff13_decrypt_data(u32 type, u8* MemBlock, u32 size, const u8* key, u32 key_len)
{
	u8 KeyBlocksArray[32][8];
	u32 *csum, ff_csum;

	if (type != 1 && key_len != 16)
		return;

	LOG("[*] Total Decrypted Size Is 0x%X (%d bytes)", size, size);

	memset(KeyBlocksArray, 0, sizeof(KeyBlocksArray));
	ff13_init_key(&KeyBlocksArray[0][0], type, (u64*) key);

	///DECODING THE ENCODED INFORMATION NOW IN MemBlock
	uint32_t ByteCounter = 0, BlockCounter = 0, KeyBlockCtr = 0;
	uint32_t Gear1 = 0, Gear2 = 0;
	uint32_t TBlockA = 0, TBlockB = 0, KBlockA = 0, KBlockB = 0;
	uint8_t CarryFlag1 = 0, CarryFlag2 = 0;
	uint8_t IntermediateCarrier = 0, OldMemblockValue = 0;

	union u {
		uint64_t Cog64B;
		uint32_t Cog32BArray[2];
	} o;

	///OUTERMOST LOOP OF THE DECODER
	for (; ByteCounter < size; BlockCounter++, KeyBlockCtr++, ByteCounter += 8)
	{
		if(KeyBlockCtr > 31)
			KeyBlockCtr = 0;

		o.Cog64B = (uint64_t)ByteCounter << 0x14;
		Gear1 = o.Cog32BArray[1] | (ByteCounter << 0x0A) | ByteCounter; ///Will this work badly when Gear1 becomes higher than 7FFFFFFF?

		CarryFlag1 = (Gear1 > ~FFXIII_CONST) ? 1 : 0;

		Gear1 = Gear1 + FFXIII_CONST;
		Gear2 = (BlockCounter*2 | o.Cog32BArray[0]) + CarryFlag1;

		///THE INNER LOOP OF THE DECODER
		for(int i = 0, BlockwiseByteCounter = 0; BlockwiseByteCounter < 8;)
		{
			if(i == 0 && BlockwiseByteCounter == 0)
			{
				OldMemblockValue = MemBlock[ByteCounter];
				MemBlock[ByteCounter] = 0x45 ^ BlockCounter ^ MemBlock[ByteCounter];
				i++;
			}
			else if(i == 0 && BlockwiseByteCounter < 8)
			{
				IntermediateCarrier = MemBlock[ByteCounter] ^ OldMemblockValue;
				OldMemblockValue = MemBlock[ByteCounter];
				MemBlock[ByteCounter] = IntermediateCarrier;
				i++;
			}
			else if(i < 9 && BlockwiseByteCounter < 8)
			{
				MemBlock[ByteCounter] = 0x78 + MemBlock[ByteCounter] - KeyBlocksArray[KeyBlockCtr][i-1];
				i++;
			}
			else if(i == 9)
			{
				i = 0;
				ByteCounter++;
				BlockwiseByteCounter++;
			}
		}
		///EXITING THE INNER LOOP OF THE DECOER

		///RESUMING THE OUTER LOOP
		ByteCounter -=8;

		TBlockA = ES32(*(u32*) &MemBlock[ByteCounter]);
		TBlockB = ES32(*(u32*) &MemBlock[ByteCounter+4]);

		KBlockA = ES32(*(u32*) &KeyBlocksArray[KeyBlockCtr][0]);
		KBlockB = ES32(*(u32*) &KeyBlocksArray[KeyBlockCtr][4]);

		CarryFlag2 = (TBlockA < KBlockA) ? 1 : 0;

		TBlockA = ES32(KBlockA ^ Gear1 ^ (TBlockA - KBlockA));
		TBlockB = ES32(KBlockB ^ Gear2 ^ (TBlockB - KBlockB - CarryFlag2));

		memcpy(&MemBlock[ByteCounter],   &TBlockB, sizeof(uint32_t));
		memcpy(&MemBlock[ByteCounter+4], &TBlockA, sizeof(uint32_t));
	}
	///EXITING THE OUTER LOOP. FILE HAS NOW BEEN FULLY DECODED.

	ff_csum = ES32(ff13_checksum(MemBlock, ByteCounter - 8));
	csum = (u32*)(MemBlock + ByteCounter - 4);

	if (*csum == ff_csum)
		LOG("[*] Decrypted File Successfully!");
	else
		LOG("[!] Decrypted data did not pass file integrity check. (Expected: %08X Got: %08X)", *csum, ff_csum);

	return;
}

void ff13_encrypt_data(u32 type, u8* MemBlock, u32 size, const u8* key, u32 key_len)
{
	u8 KeyBlocksArray[32][8];

	if (type != 1 && key_len != 16)
		return;

	LOG("[*] Total Encrypted Size Is 0x%X (%d bytes)", size, size);

	memset(KeyBlocksArray, 0, sizeof(KeyBlocksArray));
	ff13_init_key(&KeyBlocksArray[0][0], type, (u64*) key);

	///ENCODE the file, now that all the changes have been made and the checksum has been updated.
	uint32_t ByteCounter = 0, BlockCounter = 0, KeyBlockCtr = 0;
	uint32_t Gear1 = 0, Gear2 = 0;
	uint32_t TBlockA = 0, TBlockB = 0, KBlockA = 0, KBlockB = 0;
	uint8_t CarryFlag1 = 0, CarryFlag2 = 0;
	uint8_t OldMemblockValue = 0;

	union u {
		uint64_t Cog64B;
		uint32_t Cog32BArray[2];
	} o;

	///OUTERMOST LOOP OF THE ENCODER
	for (; ByteCounter < size; BlockCounter++, KeyBlockCtr++)
	{
		if(KeyBlockCtr > 31)
			KeyBlockCtr = 0;

		o.Cog64B = (uint64_t)ByteCounter << 0x14;

		Gear1 = o.Cog32BArray[1] | (ByteCounter << 0x0A) | ByteCounter; ///Will this work badly when Gear1 becomes higher than 7FFFFFFF?

		CarryFlag1 = (Gear1 > ~FFXIII_CONST) ? 1 : 0;

		Gear1 = Gear1 + FFXIII_CONST;
		Gear2 = (BlockCounter*2 | o.Cog32BArray[0]) + CarryFlag1;

		KBlockA = ES32(*(u32*) &KeyBlocksArray[KeyBlockCtr][0]);
		KBlockB = ES32(*(u32*) &KeyBlocksArray[KeyBlockCtr][4]);

		TBlockB = KBlockB ^ Gear2 ^ ES32(*(u32*) &MemBlock[ByteCounter]);
		TBlockA = KBlockA ^ Gear1 ^ ES32(*(u32*) &MemBlock[ByteCounter+4]);

		///Reverse of TBlockA < KBlockA from the Decoder.
		CarryFlag2 = (TBlockA > ~KBlockA) ? 1 : 0;

		TBlockB = ES32(TBlockB + KBlockB + CarryFlag2);       ///Reversed from subtraction to addition.
		TBlockA = ES32(TBlockA + KBlockA);                    ///Reversed from subtraction to addition.

		memcpy(&MemBlock[ByteCounter],   &TBlockA, sizeof(uint32_t));
		memcpy(&MemBlock[ByteCounter+4], &TBlockB, sizeof(uint32_t));

		///INNER LOOP OF ENCODER
		for(int i = 8, BlockwiseByteCounter = 0; BlockwiseByteCounter < 8;)
		{
			if(i != 0 && BlockwiseByteCounter < 8)
			{
				MemBlock[ByteCounter] = MemBlock[ByteCounter] + KeyBlocksArray[KeyBlockCtr][i-1] - 0x78;
				i--;
			}
			else if(i == 0 && BlockwiseByteCounter==0)
			{
				MemBlock[ByteCounter] = 0x45 ^ BlockCounter ^ MemBlock[ByteCounter];
				OldMemblockValue = MemBlock[ByteCounter];
				BlockwiseByteCounter++;
				ByteCounter++;
				i = 8;
			}
			else if(i == 0 && BlockwiseByteCounter < 8)
			{
				MemBlock[ByteCounter] = MemBlock[ByteCounter] ^ OldMemblockValue;
				OldMemblockValue = MemBlock[ByteCounter];
				i = 8;
				BlockwiseByteCounter++;
				ByteCounter++;
			}
		}
		///EXITING INNER LOOP OF ENCODER
	}
	///EXITING OUTER LOOP
	///ENCODING FINISHED

	LOG("[*] Encrypted File Successfully!");
	return;
}

void mgs_Decrypt(u8* data, int size, const char* key, int keylen)
{
    LOG("[*] Total Decrypted Size Is 0x%X (%d bytes)", size, size);

    for (int i = size - 1; i > 0; i--)
        data[i] -= (key[i % keylen] + data[i-1]);
    
    data[0] -= key[0];
    return;
}

void mgs_Encrypt(u8* data, int size, const char* key, int keylen)
{
    LOG("[*] Total Encrypted Size Is 0x%X (%d bytes)", size, size);

    data[0] += key[0];

    for (int i = 1; i < size; i++)
        data[i] += (key[i % keylen] + data[i-1]);

    return;
}

void mgs_EncodeBase64(u8* data, u32 size)
{
    int i, j, k;
    u8 chars[64];
    u8 tmpArray[28];

    LOG("[*] Total Encoded Size Is 0x%X (%d bytes)", size, size);

    if (size != 32)
        return;

    u8 type = data[31];
    memcpy(chars, (type == 2) ? MGS2_ALPHABET : MGS3_ALPHABET, sizeof(chars));

    data[31] = 0;
    data[20] = 0;
    for (j = 0; j < 20; j++)
        data[20] ^= data[j];

    for (i = 0, j = 0, k = 0; j < 28; j++)
    {
        if (k == 0 || k == 1)
            tmpArray[j] = (u8)(data[i] >> (2-k));

        else if (k == 2)
            tmpArray[j] = (u8)(data[i] & 63);

        else if (k <= 7)
            tmpArray[j] = (u8)(data[i + 1] >> (10-k)) | ((data[i] & ((1 << (8-k)) - 1)) << (k-2));

        k += 6;
        if (k >= 8)
        {
            k -= 8;
            i++;
        }
    }

    data[0] = (type == 2) ? 68 : 0x5F;
    for (i = 0, j = 0; i < 28; i++, j += (i % 4) ? 0 : 28)
        data[i + 1] = (u8)chars[(tmpArray[i] + j +  7 * (i % 4)) & 63];

    return;
}

void mgs_DecodeBase64(u8* data, u32 size)
{
    int i, j, k, m;
    u8 chars[64];
    u8 b64_table[0x100];
    u8 tmpArray[0x20];

    LOG("[*] Total Decoded Size Is 0x%X (%d bytes)", size, size);

    if (size != 32)
        return;

    u8 type = (data[0] == 0x5F) + 2;
    memset(tmpArray, 0, sizeof(tmpArray));
    memset(b64_table, 0xFF, sizeof(b64_table));
    memcpy(chars, (type == 2) ? MGS2_ALPHABET : MGS3_ALPHABET, sizeof(chars));

    for (k = 0; k < 64; k++)
        b64_table[(u8)chars[k]] = (u8)k;

    for (j = 0, m = 0; j < 196; j += 7, m++)
    {
        k = b64_table[data[1 + m]];

        if (k == 0xff)
            return;

        data[m] = (u8)((k - j) & 63);
    }

    for (j = 0, k = 0, m = 0; m < 21; m++)
    {
        if (j <= 5)
            tmpArray[m] = (u8)(data[k] & 63 >> (j & 31)) << ((j + 2) & 31);

        i = ~j + 7;
        j = 0;
        k++;

        if (i < 2)
        {
            tmpArray[m] |= (u8)(data[k] << ((~i + 3) & 31));
            i += 6;
            k++;
        }
        i -= 2;

        if (i == 0)
        {
            tmpArray[m] |= (u8)(data[k]);
            k++;
            j = 0;
        }
        else if (i <= 5)
        {
            j = 6 - i;
            tmpArray[m] |= (u8)(data[k] >> (i & 31));
        }
    }
    memcpy(data, tmpArray, sizeof(tmpArray));
    data[31] = type;

    return;
}

u32 mgspw_Checksum(const u8* data, int size)
{
    u32 csum = -1;

    while (size--)
        csum = mgspw_Table[(u8)(*data++ ^ csum)] ^ csum >> 8 ^ mgspw_Table[0];

    return ~csum;
}

void mgspw_DeEncryptBlock(u32* data, int size, u32* pwSalts)
{
	for (int i = 0; i < size; i++)
	{
		data[i] ^= pwSalts[0];
		pwSalts[0] = pwSalts[0] * 0x2e90edd + pwSalts[1];
	}
}

void mgspw_SetSalts(u32* pwSalts, const u32 *data)
{
    u32 offset = (data[1] | 0xAD47DE8F) ^ data[0];

    pwSalts[0] = (data[offset + 2] ^ 0x1327de73) ^ (data[offset + 3] ^ 0x2d71d26c);
    pwSalts[1] = pwSalts[0] * (data[offset + 7] ^ 0xBC4DEFA2);
    pwSalts[0] = (pwSalts[0] ^ 0x6576) << 16 | pwSalts[0];
}

void mgspw_SwapBlock(u32* data, int len)
{
    for (int i = 0; i < len; i++)
    	data[i] = ES32(data[i]);
}

void mgspw_Decrypt(u32* data, u32 size)
{
    u32 salts[2] = {0, 0};

    LOG("[*] Total Decrypted Size Is 0x%X (%d bytes)", size, size);

    if (size < 0x35998)
        return;

    mgspw_SwapBlock(data, 0xd676);
    mgspw_SetSalts(salts, data);
    mgspw_DeEncryptBlock(data + 16, 0xD666, salts);

    mgspw_SetSalts(salts, data + 0xD676);
    mgspw_DeEncryptBlock(data + 0xD686, 0x3C34, salts);
    mgspw_SwapBlock(data + 17, 0xd665);

    if (mgspw_Checksum((u8*)data + 68, 0x1af24) != data[14])
        LOG("[!] Checksum error (%x)", 68);

    if (mgspw_Checksum((u8*)data + 0x1af68, 0x1c00) != data[15])
        LOG("[!] Checksum error (%x)", 0x1af68);

    if (mgspw_Checksum((u8*)data + 0x1cb68, 0x18e68) != data[12])
        LOG("[!] Checksum error (%x)", 0x1cb68);

    if (mgspw_Checksum((u8*)data + 0x35a18, 0xf0d0) != data[0xD683])
        LOG("[!] Checksum error (%x)", 0x35a18);

    LOG("[*] Decrypted File Successfully!");
    return;
}

void mgspw_Encrypt(u32* data, u32 size)
{
    u32 salts[2] = {0, 0};

    LOG("[*] Total Encrypted Size Is 0x%X (%d bytes)", size, size);

    if (size < 0x35998)
        return;

    data[0xD683] = mgspw_Checksum((u8*)data + 0x35a18, 0xf0d0);
    data[12] = mgspw_Checksum((u8*)data + 0x1cb68, 0x18e68);
    data[15] = mgspw_Checksum((u8*)data + 0x1af68, 0x1c00);
    data[14] = mgspw_Checksum((u8*)data + 68, 0x1af24);

    LOG("[*] New Checksums: %08X %08X %08X %08X", data[12], data[14], data[15], data[0xD683]);

    mgspw_SwapBlock(data + 17, 0xd665);
    mgspw_SetSalts(salts, data + 0xD676);
    mgspw_DeEncryptBlock(data + 0xD686, 0x3C34, salts);

    mgspw_SetSalts(salts, data);
    mgspw_DeEncryptBlock(data + 16, 0xD666, salts);
    mgspw_SwapBlock(data, 0xD676);

    LOG("[*] Encrypted File Successfully!");
    return;
}

void dw8xl_encode_data(u8* data, u32 size)
{
	u32 xor_key = DW8XL_KEY1;

	LOG("[*] Total Encoded Size Is 0x%X (%d bytes)", size, size);

    for(int i = 0; i <= size; i++)
    {
        xor_key = (xor_key * DW8XL_KEY2) + 0x3039;
        data[i] ^= ((xor_key >> 16) & 0xff);
    }

	LOG("[*] Encoded File Successfully!");
	return;
}
