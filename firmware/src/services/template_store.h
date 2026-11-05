/**
 * @file template_store.h
 * @brief Mailvorlagen in /mail.tpl (Format #MV2 aus dem Altcode)
 * @details In der Datei stehen nur angepasste Abschnitte; fehlt einer, gilt
 *          der Standard aus dem Flash. "Zurücksetzen" entfernt den Abschnitt.
 */
#pragma once

#include "core/config/schema.h"
#include "core/mail/scheduler.h"
#include "core/mail/template.h"
#include "hal/fs.h"

namespace ps {

enum class TemplateKind : uint8_t { Boot = 0, Warning, Alive, Style };

class TemplateStore {
public:
  static constexpr const char* kPath = "/mail.tpl";
  using LineFn = bool (*)(const char* line, void* ctx); ///< false bricht ab

  explicit TemplateStore(hal::Fs& fs) : fs_(fs) {}

  static bool kindFromName(const char* name, TemplateKind& out);
  static MailVorlage::Abschnitt subjectOf(TemplateKind k);
  static MailVorlage::Abschnitt bodyOf(TemplateKind k);
  static TemplateKind kindOf(Mail::Kind k);

  /// Rohzeilen eines Abschnitts (Datei, sonst Standard)
  void read(MailVorlage::Abschnitt a, LineFn fn, void* ctx);
  bool isCustom(TemplateKind k);

  /// Prüfen und speichern. Style: subject wird ignoriert, body ist das CSS.
  Err save(TemplateKind k, const char* subject, const char* body, schema::FieldErrors& errs);
  Err reset(TemplateKind k);
  static Err validate(TemplateKind k, const char* subject, const char* body,
                      schema::FieldErrors& errs);

  /// {"kind":…,"subject":…,"body":…,"custom":…}
  void writeJson(JsonWriter& w, TemplateKind k);
  /// Alle angepassten Abschnitte als {"boot.subject":…,…} (Export)
  void writeCustomJson(JsonWriter& w);

private:
  struct Replace {
    MailVorlage::Abschnitt section;
    const char* text; ///< nullptr: entfernen
    bool singleLine;
  };
  bool rewrite(const Replace* r, uint8_t n);
  bool readFromFile(MailVorlage::Abschnitt a, LineFn fn, void* ctx);

  hal::Fs& fs_;
};

} // namespace ps
