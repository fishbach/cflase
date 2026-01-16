## Kompilierung und Installation:

### 1. **Voraussetzungen installieren:**
```bash
sudo apt-get install build-essential linux-headers-$(uname -r)
```

### 6. **Modul dauerhaft installieren:**
```bash
sudo make install
```

## Debug-Level anpassen:

Beim Laden kannst du den Debug-Level setzen:
```bash
sudo modprobe easylase.ko debug=0xff  # Alle Debug-Meldungen
sudo modprobe easylase.ko debug=0x01  # Nur Fehler
```

## Device-Node:

Nach dem Laden sollte das Device automatisch unter `/dev/easylase0` erscheinen (für das erste Gerät).
