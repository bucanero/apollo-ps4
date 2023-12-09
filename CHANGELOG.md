# Changelog

All notable changes to the `apollo-ps4` project will be documented in this file. This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]()

## [v1.4.2](https://github.com/bucanero/apollo-ps4/releases/tag/v1.4.2) - 2023-12-10

### Added

* Auto-detect `X`/`O` button settings
* Network HTTP proxy settings support
* New cheat codes
  - Grand Theft Auto V
  - Metal Gear Solid 5: The Phantom Pain
* Custom decryption support
  - Metal Gear Solid 5: The Phantom Pain
* Custom checksum support
  - Grand Theft Auto V
  - Metal Gear Solid 5: The Phantom Pain
  - Shantae and the Pirate's Curse
  - Shantae: Risky's Revenge

### Misc

* Include original keystones for 116 games
* Updated audio library to `libs3m`
* Updated [`apollo-lib`](https://github.com/bucanero/apollo-lib) Patch Engine to v0.6.0
  - Add host callbacks (username, wlan mac, psid, account id)
  - Add `murmu3_32`, `jhash` hash functions
  - Add Patapon 3 PSP decryption
  - Add MGS5 decryption (PS3/PS4)
  - Add Monster Hunter 2G/3rd PSP decryption
  - Add Castlevania:LoS checksum
  - Add Rockstar checksum
  - Fix SaveWizard Code Type C
  - Fix `right()` on little-endian platforms

## [v1.4.0](https://github.com/bucanero/apollo-ps4/releases/tag/v1.4.0) - 2023-04-16

### Added

* Network Tools
  * URL downloader tool (download http/https/ftp/ftps links)
  * Simple local Web Server (full access to console drives)
  * Disable Web Browser history
* Hex Editor for save-data files
* On-screen Keyboard (for text input)
* Activate offline accounts with user-defined account IDs (on-screen keyboard)
* Improved internal Web Server (Online DB support)
* User-defined Online DB URL (`Settings`)
* Improved DLC rebuild (read content details from `.pkg` file)
* Explicit firmware check when importing encrypted saves

### Misc

* Updated Apollo patch engine v0.4.1
  * Skip search if the pattern was not found
  * Improve code types 9, B, D
  * Add value subtraction support (BSD)

## [v1.3.1](https://github.com/bucanero/apollo-ps4/releases/tag/v1.3.1) - 2023-01-08

### Added

* Show PS4 IP address when running Apollo's Web Server

### Fixed

* Copy `SAVEDATA_LIST_PARAM` value when resigning `param.sfo`
  * This fixes an issue when importing save-games from Horizon Zero Dawn, Gran Turismo Sport, Patapon, LocoRoco Remastered, and others.

### Misc

* Download application data updates from `apollo-patches` repository

## [v1.3.0](https://github.com/bucanero/apollo-ps4/releases/tag/v1.3.0) - 2022-12-14

### Added

* New Save Wizard codes
* Add save-game sorting options
* Add database rebuild tools
  * Rebuild `app.db`
  * Rebuild DLC database `addcont.db`
  * Fix missing "Delete" option in XMB
* Add database backup/restore
* Download Online DB save-games to HDD
* Load external saves from HDD (`/data/fakeusb/`)

### Changes

* Updated networking code to `libcurl`+`polarssl` (TLS 1.2)
* Improved Pad control handling

## [v1.2.2](https://github.com/bucanero/apollo-ps4/releases/tag/v1.2.2) - 2022-11-05

### Changes

* New Save Wizard codes
* Custom decryption support
  * Diablo 3
* Changed background music
* Removed unused screen settings

### Patch Engine

* Updated Apollo patch engine v0.3.0
* Improve patch error handling
* Save Wizard / Game Genie
  * Improve SW code types 9, A
  * Add SW code types 3, 7, B, C, D
* BSD scripts
  * New commands: `copy`, `endian_swap`, `msgbox`
  * New custom hash: `force_crc32`, `mgspw_checksum`
  * Support initial value for `add/wadd/dwadd/wsub`
  * Fix `md5_xor`, `sha1_xor64` custom hashing
  * Fix little-endian support for decrypters/hashes

## [v1.2.0](https://github.com/bucanero/apollo-ps4/releases/tag/v1.2.0) - 2022-05-05

### Added

* Local Web server (Download saves as .Zip)
* RAR, Zip, 7-Zip archive extraction (place files on `/data/`)
* 50+ Save Wizard cheat-code files (collected by @OfficialAhmed)
* Custom decryption support
  * Borderlands 3
  * Grand Theft Auto 5 (AES)
  * Resident Evil Revelations 2 (Blowfish)
* Custom checksum support
  * Alien: Isolation
  * Resident Evil Revelations 2 (SHA1)

### Fixed

* Fixed issue when cheat patch couldn't be applied

## [v1.1.0](https://github.com/bucanero/apollo-ps4/releases/tag/v1.1.0) - 2022-04-17

### Added

* New firmware support: 5.07, 7.51
* Improved save-game listing (game name, saves count)
* Save-game selection (`Touchpad` to select)
  * Copy/Resign selected saves (Bulk Management)
* Detect if a save couldn't be unmounted

### Fixed

* Fixed Trophy-set listing bug
* Avoid issue when removing mount patches on exit

## [v1.0.1](https://github.com/bucanero/apollo-ps4/releases/tag/v1.0.1) - 2022-02-02

### Fixed

* Fixed Bulk resign and copy from USB

## [v1.0.0](https://github.com/bucanero/apollo-ps4/releases/tag/v1.0.0) - 2022-01-30

### Added

* Encrypted save-games support
* Offline Activation with user-defined account IDs (`owners.xml`)
* Export ownership data to `.xml`

### Fixed

* Fixed inner file handling when save has sub-folders
* Fixed SaveWizard write codes

## [v0.8.0](https://github.com/bucanero/apollo-ps4/releases/tag/v0.8.0) - 2022-01-15

### Added

* Full-HD 1920x1080 UI rendering
* Enabled animations and screen transitions
* Import/export Keystone files

### Fixed

* Fixed file attributes when creating .Zips
* Fixed Online DB links

### Misc

* Project updated to Open Orbis SDK v0.5.2

## [v0.7.0](https://github.com/bucanero/apollo-ps4/releases/tag/v0.7.0) - 2022-01-05

### Added

* 9.00 firmware support
* New Save Wizard codes
* Improved graphics library (SDL2 lib by cpasjuste)

### Fixed

* Fixed save-mount patches for 9.00 (thanks to GRModSave_Username)

## [v0.6.0](https://github.com/bucanero/apollo-ps4/releases/tag/v0.6.0) - 2022-01-01

### Added

* Automatic Save-mounting patches (GoldHEN or ps4debug required)
  * Supported FWs 5.05, 6.72, 7.02, 7.5x
* Offline Account activation
* Show Parental security passcode
* Save Keystone fingerprint dump

### Fixed

* Fixed font rendering issue

## [v0.5.0](https://github.com/bucanero/apollo-ps4/releases/tag/v0.5.0) - 2021-12-22

First public beta release.
