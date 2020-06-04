#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <stdint.h>
#include <string.h>

#include <time.h>

#define PLAIN_BUF_SIZE 1879904

/*
 link:
 https://wiki.openssl.org/index.php/EVP_Symmetric_Encryption_and_Decryption
 compile: gcc -o openssl_aec openssl_aec.c -lcrypto
 */

void handleErrors(void) {
        ERR_print_errors_fp(stderr);
        abort();
}

int encrypt(uint8_t *plain, int plain_len, unsigned char *key,
            unsigned char *iv, unsigned char *cipher) {
        EVP_CIPHER_CTX *ctx;

        int len;

        int cipher_len;

        /* Create and initialise the context */
        if (!(ctx = EVP_CIPHER_CTX_new()))
                handleErrors();

        /* Initialise the encryption operation. IMPORTANT - ensure you use a key
         * and IV size appropriate for your cipher
         * In this example we are using 256 bit AES (i.e. a 256 bit key). The
         * IV size for *most* modes is the same as the block size. For AES this
         * is 128 bits */
        if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv))
                handleErrors();

        /* Provide the message to be encrypted, and obtain the encrypted output.
         * EVP_EncryptUpdate can be called multiple times if necessary
         */
        if (1 != EVP_EncryptUpdate(ctx, cipher, &len, plain, plain_len))
                handleErrors();
        cipher_len = len;

        /* Finalise the encryption. Further ciphertext bytes may be written at
         * this stage.
         */
        if (1 != EVP_EncryptFinal_ex(ctx, cipher + len, &len))
                handleErrors();
        cipher_len += len;

        /* Clean up */
        EVP_CIPHER_CTX_free(ctx);

        return cipher_len;
}

int gcm_encrypt(unsigned char *plaintext, int plaintext_len, unsigned char *aad,
                int aad_len, unsigned char *key, unsigned char *iv, int iv_len,
                unsigned char *ciphertext, unsigned char *tag) {
        EVP_CIPHER_CTX *ctx;

        int len;

        int ciphertext_len;

        /* Create and initialise the context */
        if (!(ctx = EVP_CIPHER_CTX_new()))
                handleErrors();

        printf("1\n");

        /* Initialise the encryption operation. */
        if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL))
                handleErrors();
        printf("2\n");
        /*
         * Set IV length if default 12 bytes (96 bits) is not appropriate
         */
        if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv_len, NULL))
                handleErrors();
        printf("3\n");
        /* Initialise key and IV */
        if (1 != EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv))
                handleErrors();
        printf("4\n");
        /*
         * Provide any AAD data. This can be called zero or more times as
         * required
         */
        if (1 != EVP_EncryptUpdate(ctx, NULL, &len, aad, aad_len))
                handleErrors();
        printf("5\n");
        /*
         * Provide the message to be encrypted, and obtain the encrypted output.
         * EVP_EncryptUpdate can be called multiple times if necessary
         */
        if (1 !=
            EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len))
                handleErrors();
        ciphertext_len = len;

        printf("6\n");

        /*
         * Finalise the encryption. Normally ciphertext bytes may be written at
         * this stage, but this does not occur in GCM mode
         */
        if (1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len))
                handleErrors();
        ciphertext_len += len;

        printf("7\n");

        /* Get the tag */
        if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag))
                handleErrors();

        printf("8\n");

        /* Clean up */
        EVP_CIPHER_CTX_free(ctx);

        return ciphertext_len;
}

int decrypt(uint8_t *cipher, int cipher_len, unsigned char *key,
            unsigned char *iv, uint8_t *decrypted) {
        EVP_CIPHER_CTX *ctx;

        int len;

        int decrypted_len;

        printf("cipher len: %d\n", cipher_len);

        /* Create and initialise the context */
        if (!(ctx = EVP_CIPHER_CTX_new()))
                handleErrors();

        /* Initialise the decryption operation. IMPORTANT - ensure you use a key
         * and IV size appropriate for your cipher
         * In this example we are using 256 bit AES (i.e. a 256 bit key). The
         * IV size for *most* modes is the same as the block size. For AES this
         * is 128 bits */
        if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv))
                handleErrors();

        printf("cipher block size: %d\n", EVP_CIPHER_CTX_block_size(ctx));

        /* Provide the message to be decrypted, and obtain the plaintext output.
         * EVP_DecryptUpdate can be called multiple times if necessary
         */
        if (1 != EVP_DecryptUpdate(ctx, decrypted, &len, cipher, cipher_len))
                handleErrors();
        decrypted_len = len;

        /* Finalise the decryption. Further plaintext bytes may be written at
         * this stage.
         */
        if (1 != EVP_DecryptFinal_ex(ctx, decrypted + len, &len))
                handleErrors();
        decrypted_len += len;

        /* Clean up */
        EVP_CIPHER_CTX_free(ctx);
        return decrypted_len;
}

int gcm_decrypt(unsigned char *ciphertext, int ciphertext_len,
                unsigned char *aad, int aad_len, unsigned char *tag,
                unsigned char *key, unsigned char *iv, int iv_len,
                unsigned char *plaintext) {
        EVP_CIPHER_CTX *ctx;
        int len;
        int plaintext_len;
        int ret;

        /* Create and initialise the context */
        if (!(ctx = EVP_CIPHER_CTX_new()))
                handleErrors();

        /* Initialise the decryption operation. */
        if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL))
                handleErrors();

        /* Set IV length. Not necessary if this is 12 bytes (96 bits) */
        if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv_len, NULL))
                handleErrors();

        /* Initialise key and IV */
        if (!EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv))
                handleErrors();

        /*
         * Provide any AAD data. This can be called zero or more times as
         * required
         */
        if (!EVP_DecryptUpdate(ctx, NULL, &len, aad, aad_len))
                handleErrors();

        /*
         * Provide the message to be decrypted, and obtain the plaintext output.
         * EVP_DecryptUpdate can be called multiple times if necessary
         */
        if (!EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext,
                               ciphertext_len))
                handleErrors();
        plaintext_len = len;

        /* Set expected tag value. Works in OpenSSL 1.0.1d and later */
        if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag))
                handleErrors();

        /*
         * Finalise the decryption. A positive return value indicates success,
         * anything else is a failure - the plaintext is not trustworthy.
         */
        ret = EVP_DecryptFinal_ex(ctx, plaintext + len, &len);

        /* Clean up */
        EVP_CIPHER_CTX_free(ctx);

        if (ret > 0) {
                /* Success */
                plaintext_len += len;
                return plaintext_len;
        } else {
                /* Verify failed */
                return -1;
        }
}

#if 0
void test_gcm(void) {
        uint8_t plain[PLAIN_BUF_SIZE];
        int i;
        uint64_t start, end;

        /* A 256 bit key */
        unsigned char *key =
            (unsigned char *)"01234567890123456789012345678901";
        /* A 128 bit IV */
        unsigned char *iv = (unsigned char *)"0123456789012345";

        uint8_t cipher[PLAIN_BUF_SIZE + 16];
        /* Buffer for ciphertext. Ensure the buffer is long enough for the
         * ciphertext which may be longer than the plaintext, dependant on the
         * algorithm and mode

            JMI: with 1 byte value, the cipher len should be 16 bits
         */
        /* Buffer for the decrypted text */
        uint8_t
            decrypted[PLAIN_BUF_SIZE + 17]; /* inl  + cipher_block_size (16) */

        int decrypted_len, cipher_len;

        char tag;

        printf("GCM test\n");
        /* Message to be encrypted */
        for (i = 0; i < PLAIN_BUF_SIZE; i++)
                plain[i] = (uint8_t)i;

        /* Encrypt the plaintext */
        cipher_len = gcm_encrypt(plain, PLAIN_BUF_SIZE, NULL, 0, key, iv,
                                 strlen(iv), cipher, &tag);

        printf("Encryption DONE!\n");

        /* Decrypt the ciphertext */
        decrypted_len = gcm_decrypt(cipher, cipher_len, NULL, 0, &tag, key, iv,
                                    strlen(iv), decrypted);

        printf("Test %s\n", memcmp(plain, decrypted, PLAIN_BUF_SIZE) == 0
                                ? "Success!"
                                : "Failed!");
        /* Show the decrypted text */
        printf("Decrypted text is: (len: %d)\n", decrypted_len);
}

#endif

void test_cbc(void) {
        uint8_t plain[PLAIN_BUF_SIZE];
        int i;
        struct timespec start, end;

        /* A 256 bit key */
        unsigned char *key =
            (unsigned char *)"01234567890123456789012345678901";
        /* A 128 bit IV */
        unsigned char *iv = (unsigned char *)"0123456789012345";

        uint8_t cipher[PLAIN_BUF_SIZE + 16];
        /* Buffer for ciphertext. Ensure the buffer is long enough for the
         * ciphertext which may be longer than the plaintext, dependant on the
         * algorithm and mode

            JMI: with 1 byte value, the cipher len should be 16 bits
         */
        /* Buffer for the decrypted text */
        uint8_t
            decrypted[PLAIN_BUF_SIZE + 17]; /* inl  + cipher_block_size (16) */

        int decrypted_len, cipher_len;

        printf("CBC test\n");
        /* Message to be encrypted */
        for (i = 0; i < PLAIN_BUF_SIZE; i++)
                plain[i] = (uint8_t)i;

        /* Encrypt the plaintext */
        clock_gettime(CLOCK_MONOTONIC, &start);
        cipher_len = encrypt(plain, PLAIN_BUF_SIZE, key, iv, cipher);
        clock_gettime(CLOCK_MONOTONIC, &end);

        printf("Encryption DONE!\n");

        uint64_t tot_time = (end.tv_sec * 1000000000 + end.tv_nsec) -
                            (start.tv_sec * 1000000000 + start.tv_nsec);
        printf("Encryption time: %llu\n", tot_time);

        /* Decrypt the ciphertext */

        clock_gettime(CLOCK_MONOTONIC, &start);

        decrypted_len = decrypt(cipher, cipher_len, key, iv, decrypted);
        clock_gettime(CLOCK_MONOTONIC, &end);

        if (!memcmp(plain, decrypted, PLAIN_BUF_SIZE)) {
                printf("Decryption OK (decrypt len: %dB!\n", decrypted_len);
                uint64_t tot_time = (end.tv_sec * 1000000000 + end.tv_nsec) -
                                    (start.tv_sec * 1000000000 + start.tv_nsec);
                printf("Decryption time: %llu\n", tot_time);
        } else {
                printf("Decryption Failed!\n");
        }
}

#include <stdint.h>
#include <tomcrypt.h>

/* TomCrypt test -
   compilation: gcc tomcrypt_tst.c -o tom_tst -ltomcrypt
*/

int test_tomcrypt_gcm(void) {
        int i;
        int res;
        uint8_t tag[16];
        unsigned long taglen;

        struct timespec start, end;

        uint8_t plain[PLAIN_BUF_SIZE];

        static uint8_t aes_key[] = {
            0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15,
            0x88, 0x09, 0xcf, 0x4f, 0x3c, 0x3b, 0x4e, 0x12, 0x61, 0x82, 0xea,
            0x2d, 0x6a, 0xac, 0x7f, 0x15, 0x88, 0x90, 0xfc, 0xf4, 0xc3};

        static uint8_t asf_iv[] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x0,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x0};

        printf("\nTest TOMCRYPT GCM\n");

        for (i = 0; i < PLAIN_BUF_SIZE; i++)
                plain[i] = (uint8_t)i;

        /* register cipher */
        register_cipher(&aes_desc);

        clock_gettime(CLOCK_MONOTONIC, &start);
        res = gcm_memory(find_cipher("aes"), aes_key, sizeof(aes_key), /* Key */
                         asf_iv, sizeof(asf_iv),                       /* IV */
                         NULL, 0,               /* AAD - not used */
                         plain, PLAIN_BUF_SIZE, /* Plain buffer */
                         plain,                 /* encrypted buffer */
                         tag, &taglen,          /* TAG */
                         GCM_ENCRYPT);          /* Direction */
        clock_gettime(CLOCK_MONOTONIC, &end);

        if (res == CRYPT_OK) {
                printf("Encryption OK!\n");
                uint64_t tot_time = (end.tv_sec * 1000000000 + end.tv_nsec) -
                                    (start.tv_sec * 1000000000 + start.tv_nsec);
                printf("Encryption time: %llu\n", tot_time);
        } else {
                printf("Encryption failed, error: %s\n", error_to_string(res));
        }

        clock_gettime(CLOCK_MONOTONIC, &start);
        res = gcm_memory(find_cipher("aes"), aes_key, sizeof(aes_key), /* Key */
                         asf_iv, sizeof(asf_iv),                       /* IV */
                         NULL, 0,               /* AAD - not used */
                         plain, PLAIN_BUF_SIZE, /* Plain buffer */
                         plain,                 /* encrypted buffer */
                         tag, &taglen,          /* TAG */
                         GCM_DECRYPT);          /* Direction */
        clock_gettime(CLOCK_MONOTONIC, &end);

        if (res == CRYPT_OK) {
                printf("Decryption OK!\n");
                uint64_t tot_time = (end.tv_sec * 1000000000 + end.tv_nsec) -
                                    (start.tv_sec * 1000000000 + start.tv_nsec);
                printf("Decryption time: %llu\n", tot_time);
        } else {
                printf("Decryption failed, error: %s\n", error_to_string(res));
        }

        return 0;
}

int test_tomcrypt_aes_cbc(void) {
        int err;
        symmetric_CBC scbc;
        int i;

        struct timespec start, end;
        /* A 128 bit IV (same size a for openssl AES test) */
        uint8_t iv[] = {0xa1, 0x52, 0x78, 0xcc, 0xba, 0xc9, 0x00, 0x11,
                        0x22, 0x00, 0x23, 0x43, 0xef, 0xff, 0x15, 0x78};

        uint8_t aes_key[] = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
                             0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
                             0x3b, 0x4e, 0x12, 0x61, 0x82, 0xea, 0x2d, 0x6a,
                             0xac, 0x7f, 0x15, 0x88, 0x90, 0xfc, 0xf4, 0xc3};

        uint8_t plain[PLAIN_BUF_SIZE];

        printf("\nTest TOMCRYPT CBC\n");

        for (i = 0; i < PLAIN_BUF_SIZE; i++)
                plain[i] = (uint8_t)i;

        register_cipher(&aes_desc);

        /* Encryption */
        err = cbc_start(find_cipher("aes"), iv, aes_key, sizeof(aes_key), 0,
                        &scbc);
        if (err != CRYPT_OK) {
                printf("cbc_start error: %s\n", error_to_string(err));
                return -1;
        }
        clock_gettime(CLOCK_MONOTONIC, &start);
        err = cbc_encrypt(plain, plain, PLAIN_BUF_SIZE, &scbc);
        clock_gettime(CLOCK_MONOTONIC, &end);
        if (err != CRYPT_OK) {
                printf("cbc_encrypt error: %s\n", error_to_string(err));
                return -1;
        }
        uint64_t tot_time = (end.tv_sec * 1000000000 + end.tv_nsec) -
                            (start.tv_sec * 1000000000 + start.tv_nsec);
        printf("Encryption time: %llu\n", tot_time);

        /* Decryption */
        err = cbc_setiv(iv, sizeof(iv), &scbc);
        if (err != CRYPT_OK) {
                printf("ctr_setiv error: %s\n", error_to_string(err));
                return -1;
        }

        clock_gettime(CLOCK_MONOTONIC, &start);
        err = cbc_decrypt(plain, plain, PLAIN_BUF_SIZE, &scbc);
        clock_gettime(CLOCK_MONOTONIC, &end);
        if (err != CRYPT_OK) {
                printf("cbc_decrypt error: %s\n", error_to_string(err));
                return -1;
        }

        tot_time = (end.tv_sec * 1000000000 + end.tv_nsec) -
                   (start.tv_sec * 1000000000 + start.tv_nsec);
        printf("Decryption time: %llu\n", tot_time);

        /* terminate the stream */
        err = cbc_done(&scbc);
        if (err != CRYPT_OK) {
                printf("cbc_done error: %s\n", error_to_string(err));
                return -1;
        }

	return 0;
}

int main(void) {
        test_cbc();
        test_tomcrypt_gcm();
        test_tomcrypt_aes_cbc();

	// Not working for now...
        // test_gcm();

        return 0;
}
