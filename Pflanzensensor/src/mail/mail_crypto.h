/**
 * @file mail_crypto.h
 * @brief AES-256-GCM-Verschlüsselung der Mail-Nutzlast, auf Basis der rohen
 *        BearSSL-Primitiven
 * @details Der ESP8266 hat zu wenig RAM für einen vollständigen
 *          TLS-Handshake (X.509-Parsing, Schlüsselaustausch, 16 KB+16 KB
 *          Record-Puffer - bereits praktisch erprobt und verworfen, siehe
 *          configs/config_example.h, USE_MAIL). Diese Datei nutzt
 *          stattdessen direkt die rohen AES-GCM-Bausteine von BearSSL
 *          (`<bearssl/bearssl_aead.h>`, `<bearssl/bearssl_block.h>`) -
 *          NICHT über `WiFiClientSecure`/`BearSSLHelpers`, das den
 *          kompletten TLS-Sitzungs- und Zertifikatscode mitziehen würde.
 *          Der `br_gcm_context` samt AES-Rundenschlüsseln ist wenige
 *          hundert Byte groß, es wird nur eine einzelne
 *          Blockchiffre-Operation pro Nachricht gebraucht, kein
 *          Session-Puffer, kein Schlüsselaustausch.
 *
 *          Damit ist das hier ein selbstgebautes Protokoll auf Basis
 *          auditierter Krypto-Bausteine, nicht selbstgebaute Kryptografie -
 *          AES und GCM selbst sind BearSSLs vorhandene, in
 *          tools/sdk/lib/libbearssl.a bereits vorkompilierte
 *          Implementierung.
 *
 *          Nicht nativ testbar über `pio test -e native` (libbearssl.a ist
 *          als vorkompilierte Xtensa-Bibliothek dort nicht verlinkt).
 *          Stattdessen wurde die exakte Aufrufreihenfolge unten
 *          (br_aes_ct_ctr_init -> br_gcm_init -> br_gcm_reset ->
 *          br_gcm_aad_inject -> br_gcm_flip -> br_gcm_run ->
 *          br_gcm_get_tag) einmalig gegen den portablen BearSSL-C-Quellcode
 *          (derselbe Code, nur für den Host statt Xtensa kompiliert) UND
 *          gegen PHPs openssl_encrypt('aes-256-gcm', ...) geprüft - beide
 *          liefern für denselben Testvektor byteidentisches Ergebnis:
 *
 *            key       = 00 01 02 ... 1f (32 Byte, aufsteigend)
 *            nonce     = 00 01 02 ... 0b (12 Byte, aufsteigend)
 *            device_id = "testdevice01"
 *            ts        = 1700000000
 *            aad       = device_id + String(ts) = "testdevice011700000000"
 *            plaintext = {"to":"test@example.com","subject":"Test","body":"Hallo Welt"}
 *            ciphertext (hex) = 3c20a274e7dfe06fe832e3cbd4911900f3bae21a93
 *              14325e144596f07f0365d1753294defba461ec56885d8fe7e3511ad47b
 *              28ec36baccfa68f2466d3a9e
 *            tag (hex) = f9fca4b671b4deef4e697e6101d007d1
 *
 *          Dieser Vektor ist auch die Fixture für CryptoTest im
 *          Mailbot-Repo (siehe Pflanzensensor-mailbot/tests/CryptoTest.php)
 *          - stellt sicher, dass beide Seiten Byte für Byte kompatibel
 *          verschlüsseln, ohne dass dafür echte Hardware nötig ist. Ein
 *          Test mit echtem Gerät bleibt trotzdem sinnvoll (RNG, NTP-Zeit,
 *          echte Netzwerkübertragung), ist aber keine Voraussetzung mehr
 *          dafür, dem Protokoll selbst zu vertrauen.
 */

#ifndef MAIL_CRYPTO_H
#define MAIL_CRYPTO_H

#include <Arduino.h>

namespace MailCrypto {

constexpr size_t KEY_LEN = 32;   // AES-256
constexpr size_t NONCE_LEN = 12; // GCM-Standardnonce
constexpr size_t TAG_LEN = 16;   // GCM-Auth-Tag

/**
 * @brief Ergebnis einer Versiegelung, Felder bereits Base64-kodiert für die
 *        direkte Einbettung in den JSON-Umschlag (siehe mail_client.cpp)
 */
struct SealedEnvelope {
  bool success = false;
  String nonceBase64;
  String ciphertextBase64;
  String tagBase64;
};

/**
 * @brief Verschlüsselt plaintext mit AES-256-GCM
 * @param secretKey 32-Byte-Schlüssel (identisch mit dem beim Mailbot
 *        hinterlegten Geräteschlüssel)
 * @param plaintext Klartext-JSON-Nutzlast ({"to":...,"subject":...,"body":...})
 * @param deviceId Öffentliche Geräte-ID (Teil der AAD)
 * @param unixTimestamp Aktuelle Unixzeit (Teil der AAD, dient dem
 *        Mailbot als Replay-Schutz)
 * @return SealedEnvelope mit success=false bei RNG-Fehler; die AAD wird aus
 *         `deviceId + String(unixTimestamp)` gebildet (Dezimaldarstellung
 *         ohne führende Nullen/Trennzeichen) - MUSS mit der PHP-Gegenseite
 *         exakt übereinstimmen (dort: `$device_id . $ts`).
 */
SealedEnvelope seal(const uint8_t secretKey[KEY_LEN], const String& plaintext,
                    const String& deviceId, uint32_t unixTimestamp);

} // namespace MailCrypto

#endif // MAIL_CRYPTO_H
