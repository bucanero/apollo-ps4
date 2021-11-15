#define OFFZIP_WBITS_ZLIB		15
#define OFFZIP_WBITS_DEFLATE	-15


int offzip_util(const char *input, const char *output_dir, const char *basename, int wbits);
int packzip_util(const char *input, const char *output, uint32_t offset, int wbits);

// Diablo 3 save data encryption
void diablo_decrypt_data(uint8_t* data, uint32_t size);
void diablo_encrypt_data(uint8_t* data, uint32_t size);

// Blowfish ECB save data encryption
void blowfish_ecb_encrypt(uint8_t* data, uint32_t len, uint8_t* key, uint32_t key_len);
void blowfish_ecb_decrypt(uint8_t* data, uint32_t len, uint8_t* key, uint32_t key_len);

// AES ECB save data encryption
void aes_ecb_decrypt(uint8_t* data, uint32_t len, uint8_t* key, uint32_t key_len);
void aes_ecb_encrypt(uint8_t* data, uint32_t len, uint8_t* key, uint32_t key_len);

// DES ECB save data encryption
void des_ecb_decrypt(uint8_t* data, uint32_t len, uint8_t* key, uint32_t key_len);
void des_ecb_encrypt(uint8_t* data, uint32_t len, uint8_t* key, uint32_t key_len);

// 3-DES CBC save data encryption
void des3_cbc_decrypt(uint8_t* data, uint32_t len, const uint8_t* key, uint32_t key_len, uint8_t* iv, uint32_t iv_len);
void des3_cbc_encrypt(uint8_t* data, uint32_t len, const uint8_t* key, uint32_t key_len, uint8_t* iv, uint32_t iv_len);

// NFS Undercover save data encryption
void nfsu_decrypt_data(uint8_t* data, uint32_t size);
void nfsu_encrypt_data(uint8_t* data, uint32_t size);

// Silent Hill 3 save data encryption
void sh3_decrypt_data(uint8_t* data, uint32_t size);
void sh3_encrypt_data(uint8_t* data, uint32_t size);

// Dynasty Warriors 8 Xtreme Legends save data encryption
void dw8xl_encode_data(uint8_t* data, uint32_t len);

// Metal Gear Solid 2/3 HD save data encryption
void mgs_Decrypt(uint8_t* data, int size, const char* key, int keylen);
void mgs_Encrypt(uint8_t* data, int size, const char* key, int keylen);
void mgs_DecodeBase64(uint8_t* data, uint32_t size);
void mgs_EncodeBase64(uint8_t* data, uint32_t size);

// Metal Gear Solid Peace Walker save data encryption
void mgspw_Encrypt(uint32_t* data, uint32_t len);
void mgspw_Decrypt(uint32_t* data, uint32_t len);

// Final Fantasy XIII (1/2/3) save data encryption
void ff13_decrypt_data(uint32_t game, uint8_t* data, uint32_t len, const uint8_t* key, uint32_t key_len);
void ff13_encrypt_data(uint32_t game, uint8_t* data, uint32_t len, const uint8_t* key, uint32_t key_len);
uint32_t ff13_checksum(const uint8_t* bytes, uint32_t len);
