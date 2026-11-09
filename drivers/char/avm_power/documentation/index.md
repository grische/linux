# AVM-Power-Treiber

## Senke für diverse Temperatur/Powermanagment-Informationen
 * CPU-Last
 * Temperatur
 * Engergieverbrauch peripherer Geräte (USB, SLICS, ISDN, WLAN, LTE, DVBC, Ethernet)
 * Speicherbedarf

 Das Sammeln dieser Information erfolgt u.a. über folgende im Kernel/Treiber zu realisierende Hooks:

 PowerManagmentRessourceInfo()

## Bereitstellung der Powermanagement-Infos per AVM-Event
 * Liefert die Informationen für den Energiemonitor der Webseite.
 * Liefert die Konfiguration der Ethernet-Ports

## Konfiguration von (komplexen) treiberübergreifenden Betriebs-Modi (Powermode-Skript)
 * DSL-Modus (ADSL, VDSL, LTE, ATA) (versendet avm-dsl-event)
 * Bitfile-Modus (POTS/TE)
 * Telefon-Profil  (versendet avm-telefonie-event)
 * VOIP-Profil
 * Multicast-Profil
 * DECT-Trace-Profil (Achtung! Öffnet Socketport 51111/51112)
 * Speedstep/Governor
 * Konfiguration der Ethernetports (10/100/1000 MBit)
 * Setzt die Powermanagment-QoS (Wakeup-Latency) beim x86
 * Dying GASP via Wyatt-Earp(obsolet)

 Das Registrieren entsprechend notwendiger Hooks erfolgt über:

 PowerManagmentRegister()

