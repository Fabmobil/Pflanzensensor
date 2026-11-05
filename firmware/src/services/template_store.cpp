/**
 * @file template_store.cpp
 */
#include "services/template_store.h"

#include "core/mail/defaults.h"
#include "services/log.h"
#include "services/storage.h"

namespace ps {

using MailVorlage::Abschnitt;

namespace {

constexpr size_t kLineBuf = MailVorlage::ZEILE_MAX + 2;

const char* defaultFor(Abschnitt a) {
  using namespace MailVorlagenStandard;
  switch (a) {
  case Abschnitt::BootBetreff:
    return BOOT_BETREFF;
  case Abschnitt::BootRumpf:
    return BOOT_RUMPF;
  case Abschnitt::WarnBetreff:
    return WARNUNG_BETREFF;
  case Abschnitt::WarnRumpf:
    return WARNUNG_RUMPF;
  case Abschnitt::AliveBetreff:
    return ALIVE_BETREFF;
  case Abschnitt::AliveRumpf:
    return ALIVE_RUMPF;
  case Abschnitt::Stil:
    return STIL;
  default:
    return nullptr;
  }
}

/// Eine Zeile lesen (ohne \n, ohne \r). false am Dateiende ohne Daten.
bool readLine(hal::File& f, char* buf, size_t cap, size_t& len) {
  len = 0;
  bool any = false;
  for (;;) {
    uint8_t c;
    if (f.read(&c, 1) != 1)
      break;
    any = true;
    if (c == '\n')
      break;
    if (len + 1 < cap)
      buf[len++] = static_cast<char>(c);
  }
  while (len > 0 && buf[len - 1] == '\r')
    len--;
  buf[len] = '\0';
  if (len + 1 >= cap)
    len = MailVorlage::ganzeZeichen(buf, len), buf[len] = '\0';
  return any;
}

void writeLine(hal::File& f, const char* line) {
  if (MailVorlage::brauchtEntwertung(line))
    f.write("\\", 1);
  f.write(line, strlen(line));
  f.write("\n", 1);
}

/// Text zeilenweise (CRLF → LF) schreiben; singleLine: Umbrüche zu Leerzeichen
void writeText(hal::File& f, const char* text, bool singleLine) {
  char line[kLineBuf];
  size_t n = 0;
  for (const char* p = text;; p++) {
    const char c = *p;
    if (c == '\0' || (c == '\n' && !singleLine)) {
      while (n > 0 && line[n - 1] == '\r')
        n--;
      line[n] = '\0';
      writeLine(f, line);
      n = 0;
      if (c == '\0')
        break;
      continue;
    }
    if (c == '\r' && singleLine)
      continue;
    if (n + 1 < sizeof(line))
      line[n++] = (c == '\n') ? ' ' : c;
  }
}

} // namespace

bool TemplateStore::kindFromName(const char* name, TemplateKind& out) {
  if (strcmp(name, "boot") == 0)
    out = TemplateKind::Boot;
  else if (strcmp(name, "warning") == 0)
    out = TemplateKind::Warning;
  else if (strcmp(name, "alive") == 0)
    out = TemplateKind::Alive;
  else if (strcmp(name, "style") == 0)
    out = TemplateKind::Style;
  else
    return false;
  return true;
}

Abschnitt TemplateStore::subjectOf(TemplateKind k) {
  switch (k) {
  case TemplateKind::Boot:
    return Abschnitt::BootBetreff;
  case TemplateKind::Warning:
    return Abschnitt::WarnBetreff;
  case TemplateKind::Alive:
    return Abschnitt::AliveBetreff;
  default:
    return Abschnitt::Keiner;
  }
}

Abschnitt TemplateStore::bodyOf(TemplateKind k) {
  switch (k) {
  case TemplateKind::Boot:
    return Abschnitt::BootRumpf;
  case TemplateKind::Warning:
    return Abschnitt::WarnRumpf;
  case TemplateKind::Alive:
    return Abschnitt::AliveRumpf;
  default:
    return Abschnitt::Stil;
  }
}

TemplateKind TemplateStore::kindOf(Mail::Kind k) {
  switch (k) {
  case Mail::Kind::Boot:
    return TemplateKind::Boot;
  case Mail::Kind::Warning:
    return TemplateKind::Warning;
  default:
    return TemplateKind::Alive;
  }
}

bool TemplateStore::readFromFile(Abschnitt wanted, LineFn fn, void* ctx) {
  hal::FileGuard f(fs_, fs_.open(kPath, hal::OpenMode::Read));
  if (!f)
    return false;
  char line[kLineBuf];
  size_t n;
  if (!readLine(*f.get(), line, sizeof(line), n) || strcmp(line, MailVorlage::DATEI_KOPF) != 0) {
    LOGW(Mod::Mail, "Vorlagendatei hat eine fremde Kopfzeile - Standard wird benutzt");
    return false;
  }
  bool inSection = false;
  while (readLine(*f.get(), line, sizeof(line), n)) {
    const Abschnitt m = MailVorlage::erkenneMarke(line);
    if (m != Abschnitt::Keiner) {
      if (inSection)
        break;
      inSection = (m == wanted);
      continue;
    }
    if (!inSection)
      continue;
    if (fn && !fn(MailVorlage::entwerte(line), ctx))
      break;
  }
  return inSection;
}

void TemplateStore::read(Abschnitt a, LineFn fn, void* ctx) {
  if (readFromFile(a, fn, ctx))
    return;
  const char* src = defaultFor(a);
  if (!src)
    return;
  char line[kLineBuf];
  size_t at = 0;
  for (size_t i = 0;; i++) {
    const char c = static_cast<char>(ps_read_byte(src + i));
    if (c == '\0' || c == '\n') {
      line[at] = '\0';
      if (at > 0 || c == '\n')
        if (!fn(line, ctx))
          return;
      at = 0;
      if (c == '\0')
        return;
      continue;
    }
    if (at + 1 < sizeof(line))
      line[at++] = c;
  }
}

bool TemplateStore::isCustom(TemplateKind k) { return readFromFile(bodyOf(k), nullptr, nullptr); }

Err TemplateStore::validate(TemplateKind k, const char* subject, const char* body,
                            schema::FieldErrors& errs) {
  if (!body) {
    errs.add("body", Err::InvalidArgument);
    return Err::InvalidArgument;
  }
  if (k == TemplateKind::Style) {
    if (strlen(body) > MailVorlage::STIL_MAX)
      errs.add("body", Err::TooLong);
    if (strchr(body, '<') || strchr(body, '>'))
      errs.add("body", Err::InvalidArgument);
    return errs.empty() ? Err::None : Err::InvalidArgument;
  }
  if (!subject || !*subject)
    errs.add("subject", Err::InvalidArgument);
  else if (strlen(subject) > MailVorlage::BETREFF_MAX)
    errs.add("subject", Err::TooLong);
  if (strlen(body) > MailVorlage::RUMPF_MAX)
    errs.add("body", Err::TooLong);
  // Zeilenlänge
  size_t lineLen = 0;
  for (const char* p = body;; p++) {
    if (*p == '\n' || *p == '\0') {
      if (lineLen > MailVorlage::ZEILE_MAX) {
        errs.add("body", Err::TooLong);
        break;
      }
      lineLen = 0;
      if (!*p)
        break;
    } else if (*p != '\r') {
      lineLen++;
    }
  }
  return errs.empty() ? Err::None : Err::InvalidArgument;
}

bool TemplateStore::rewrite(const Replace* r, uint8_t n) {
  struct Ctx {
    TemplateStore* self;
    const Replace* r;
    uint8_t n;
  } ctx{this, r, n};
  return storage::writeAtomic(
      fs_, kPath,
      [](hal::File& out, void* c) -> bool {
        Ctx& x = *static_cast<Ctx*>(c);
        out.write(MailVorlage::DATEI_KOPF);
        out.write("\n", 1);
        for (uint8_t i = 0; i < x.n; i++) {
          if (!x.r[i].text)
            continue;
          out.write(MailVorlage::markeVon(x.r[i].section));
          out.write("\n", 1);
          writeText(out, x.r[i].text, x.r[i].singleLine);
        }
        // Übrige Abschnitte aus der alten Datei übernehmen (schon entwertet)
        hal::FileGuard old(x.self->fs_, x.self->fs_.open(kPath, hal::OpenMode::Read));
        if (!old)
          return true;
        char line[kLineBuf];
        size_t len;
        if (!readLine(*old.get(), line, sizeof(line), len) ||
            strcmp(line, MailVorlage::DATEI_KOPF) != 0)
          return true;
        bool take = false;
        while (readLine(*old.get(), line, sizeof(line), len)) {
          const Abschnitt m = MailVorlage::erkenneMarke(line);
          if (m != Abschnitt::Keiner) {
            take = m != Abschnitt::Unbekannt;
            for (uint8_t i = 0; i < x.n && take; i++)
              if (x.r[i].section == m)
                take = false;
            if (take) {
              out.write(line, len);
              out.write("\n", 1);
            }
            continue;
          }
          if (take) {
            out.write(line, len);
            out.write("\n", 1);
          }
        }
        return true;
      },
      &ctx);
}

Err TemplateStore::save(TemplateKind k, const char* subject, const char* body,
                        schema::FieldErrors& errs) {
  const Err v = validate(k, subject, body, errs);
  if (v != Err::None)
    return v;
  bool ok;
  if (k == TemplateKind::Style) {
    const Replace r[] = {{Abschnitt::Stil, body, false}};
    ok = rewrite(r, 1);
  } else {
    const Replace r[] = {{subjectOf(k), subject, true}, {bodyOf(k), body, false}};
    ok = rewrite(r, 2);
  }
  if (!ok)
    return Err::Io;
  LOGI(Mod::Mail, "Vorlage gespeichert");
  return Err::None;
}

Err TemplateStore::reset(TemplateKind k) {
  if (!fs_.exists(kPath))
    return Err::None;
  bool ok;
  if (k == TemplateKind::Style) {
    const Replace r[] = {{Abschnitt::Stil, nullptr, false}};
    ok = rewrite(r, 1);
  } else {
    const Replace r[] = {{subjectOf(k), nullptr, true}, {bodyOf(k), nullptr, false}};
    ok = rewrite(r, 2);
  }
  if (!ok)
    return Err::Io;
  LOGI(Mod::Mail, "Vorlage auf Standard zurückgesetzt");
  return Err::None;
}

namespace {
struct JsonLines {
  JsonWriter* w;
  bool first;
  bool single;
};
bool emitLine(const char* line, void* c) {
  JsonLines& j = *static_cast<JsonLines*>(c);
  if (!j.first) {
    if (j.single)
      return false;
    j.w->sink().write("\\n", 2);
  }
  jsonEscape(j.w->sink(), line, strlen(line));
  j.first = false;
  return true;
}
/// Abschnitt als JSON-String (Zeilen mit \n verbunden)
void sectionString(TemplateStore& s, JsonWriter& w, Abschnitt a, bool single) {
  w.raw("\"");
  JsonLines j{&w, true, single};
  s.read(a, emitLine, &j);
  w.sink().write("\"", 1);
}
} // namespace

void TemplateStore::writeJson(JsonWriter& w, TemplateKind k) {
  w.beginObject();
  static const char* const kNames[] = {"boot", "warning", "alive", "style"};
  w.kv(FS_("kind"), kNames[static_cast<uint8_t>(k)]);
  if (k != TemplateKind::Style) {
    w.key(FS_("subject"));
    sectionString(*this, w, subjectOf(k), true);
  }
  w.key(FS_("body"));
  sectionString(*this, w, bodyOf(k), false);
  w.kv(FS_("custom"),
       k == TemplateKind::Style ? readFromFile(Abschnitt::Stil, nullptr, nullptr) : isCustom(k));
  w.endObject();
}

void TemplateStore::writeCustomJson(JsonWriter& w) {
  w.beginObject();
  static const char* const kNames[] = {"boot", "warning", "alive", "style"};
  for (uint8_t i = 0; i < 4; i++) {
    const TemplateKind k = static_cast<TemplateKind>(i);
    const Abschnitt body = bodyOf(k);
    if (!readFromFile(body, nullptr, nullptr))
      continue;
    w.key(kNames[i]).beginObject();
    if (k != TemplateKind::Style) {
      w.key(FS_("subject"));
      sectionString(*this, w, subjectOf(k), true);
    }
    w.key(FS_("body"));
    sectionString(*this, w, body, false);
    w.endObject();
  }
  w.endObject();
}

} // namespace ps
