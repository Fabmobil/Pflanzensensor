#ifndef PREFERENCES_MANAGER_H
#define PREFERENCES_MANAGER_H

#include <Arduino.h>

class PreferencesManager {
public:
    PreferencesManager();

    void initNamespace(const String& namespaceName);

    String getString(const String& namespaceName, const String& key, const String& defaultValue);
    void setString(const String& namespaceName, const String& key, const String& value);
};

#endif  // PREFERENCES_MANAGER_H
