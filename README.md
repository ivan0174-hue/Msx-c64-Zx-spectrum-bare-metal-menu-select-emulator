# rpi-retro — menu di avvio bare metal per Raspberry Pi 3

Tre sistemi bare metal (niente Linux) su una sola scheda SD, scelti da un menu all'accensione:

| Voce | Kernel | Origine |
|---|---|---|
| Launcher (menu) | `kernel8-32.img` / `kernel7.img` | questo progetto (`launcher/`) |
| MSX1 / MSX2 / MSX2+ | `/msx/kernel-msx.img` | questo progetto (`msx/`, fMSX via fmsx-libretro) |
| Commodore 64/128/VIC-20/Plus4/PET | `/bmc64/…`, `/kernel8-32.img.c128` … | BMC64 (da scaricare) |
| ZX Spectrum | `/zxmini/kernel7.img` | ZXMini (da scaricare) |

## Installazione veloce
Scarica [`release/rpi3-retro-sd.zip`](release/rpi3-retro-sd.zip) (già compilato), scompattalo nella
radice di una SD FAT32 e segui `LEGGIMI.txt` (BMC64 e ZXMini vanno scaricati a parte; i BIOS MSX
originali vanno in `/msx/bios`).

> Provato su un Raspberry Pi 3 reale e in QEMU (emulazione Raspberry Pi 2).

## Come funziona
* **launcher/** — app Circle: legge `launcher.cfg`, mostra il menu, carica in RAM il kernel scelto,
  ferma il controller USB e usa il *chain boot* di Circle (MMU e cache spente, copia a 0x8000, salto).
  Spegnendo e riaccendendo si torna al menu.
* **msx/** — app Circle + circle-stdlib (newlib) che ospita il core **fmsx-libretro** con un mini
  frontend libretro scritto apposta: video RGB565 scalato 2x in un framebuffer 640x480 (con bande
  laterali per gli schermi 16:9), audio HDMI o jack a 48 kHz, tastiera USB (codici HID → RETROK),
  joypad USB, menu di configurazione (modello, RAM, VRAM, velocità Z80, PAL/NTSC, BIOS, mapper),
  file browser per ROM/DSK/CAS e menu di pausa (F12).
* Modifiche al core fMSX (in `msx/fmsx`): moltiplicatore velocità CPU (`BM_CPUMul`, scala
  `IPeriod` in `LoopZ80`), caricamento di 2 cartucce + 2 floppy + cassetta dal menu, RAM 1/2 MB,
  joypad attivo anche in modalità tastiera, cambio floppy a caldo con salvataggio.
* **common/** — video, input (plug and play USB), menu/file browser, file di configurazione.

## Compilare
```
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib texinfo git make curl
./build.sh          # scarica e compila circle-stdlib (-r 3), poi launcher e MSX
                    # risultato in out/sd
```
Per provarlo in QEMU (`qemu-system-arm -M raspi2b`) serve una circle-stdlib configurata con
`-r 2 --qemu` e i core secondari parcheggiati (QEMU li avvia tutti a 0x8000).

## Licenze
Launcher/frontend: GPLv3. fMSX © Marat Fayzullin (uso non commerciale, vedi `msx/fmsx/LICENSE`).
Circle / circle-stdlib: GPLv3. C-BIOS: BSD.
