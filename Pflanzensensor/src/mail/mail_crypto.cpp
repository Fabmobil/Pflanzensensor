/**
 * @file mail_crypto.cpp
 * @brief Implementierung siehe mail_crypto.h
 */

#include "mail_crypto.h"

#include <bearssl/bearssl_aead.h>
#include <bearssl/bearssl_block.h>
#include <bearssl/bearssl_hash.h>
#include <osapi.h>

#include <cstring>
#include <memory>

#include "../logger/logger.h"
#include "../utils/base64.h"

namespace MailCrypto {

SealedEnvelope seal(const uint8_t secretKey[KEY_LEN], const String& plaintext,
                    const String& deviceId, uint32_t unixTimestamp) {
  SealedEnvelope result;

  uint8_t nonce[NONCE_LEN];
  if (os_get_random(nonce, NONCE_LEN) != 0) {
    LOG_ERROR(F("MailCrypto"), F("Hardware-RNG für Nonce fehlgeschlagen"));
    return result;
  }

  // AAD = device_id + Dezimaldarstellung von ts, ohne Trennzeichen - muss
  // byteidentisch mit der PHP-Gegenseite ($device_id . $ts) sein.
  String aad = deviceId + String(unixTimestamp);

  // In-place-Verschlüsselung: Klartext in einen mutable Puffer kopieren.
  size_t len = plaintext.length();
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[len]);
  memcpy(buffer.get(), plaintext.c_str(), len);

  br_aes_ct_ctr_keys blockCtx;
  br_aes_ct_ctr_init(&blockCtx, secretKey, KEY_LEN);

  br_gcm_context gcmCtx;
  br_gcm_init(&gcmCtx, &blockCtx.vtable, br_ghash_ctmul32);
  br_gcm_reset(&gcmCtx, nonce, NONCE_LEN);
  br_gcm_aad_inject(&gcmCtx, aad.c_str(), aad.length());
  br_gcm_flip(&gcmCtx);
  br_gcm_run(&gcmCtx, 1 /* encrypt */, buffer.get(), len);

  uint8_t tag[TAG_LEN];
  br_gcm_get_tag(&gcmCtx, tag);

  result.nonceBase64 = Base64::encode(nonce, NONCE_LEN);
  result.ciphertextBase64 = Base64::encode(buffer.get(), len);
  result.tagBase64 = Base64::encode(tag, TAG_LEN);
  result.success = true;
  return result;
}

} // namespace MailCrypto
