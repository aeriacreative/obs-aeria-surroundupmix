# Aeria Sorround Upmix

**Plugin audio OBS Studio untuk pemrosesan upmix surround, kontrol gain kanal, dan pemantauan distribusi audio.**

- **Versi:** `v0.0.1` — versi awal pengembangan
- **Folder proyek:** `aeria-sorroundupmix`
- **Target:** OBS Studio 32.x
- **Sistem operasi target:** Windows
- **Status:** Dalam pengembangan

> **Catatan:** Dokumen ini menjelaskan tujuan dan rancangan proyek. Fitur yang belum dibangun atau belum diuji tidak boleh dianggap sudah tersedia. Perbarui bagian status setelah implementasi dan pengujian benar-benar dilakukan.

## 1. Gambaran umum

Aeria Sorround Upmix dirancang sebagai plugin OBS yang membantu pengguna mengatur distribusi audio multikanal dengan antarmuka yang terinspirasi perangkat lunak audio profesional dan plugin VST.

Plugin direncanakan memiliki tiga komponen utama:

1. **Filter Sorround Upmix** — pemrosesan audio pada source OBS, termasuk upmix mono/stereo dan kontrol gain kanal.
2. **Sorround Control** — dock khusus untuk mengendalikan satu instance filter, lengkap dengan peta speaker, kontrol arah suara, slider gain, dan meter.
3. **Sorround Monitor** — dock pemantauan mix/output OBS yang tersedia melalui API, untuk melihat kanal mana yang benar-benar memiliki sinyal.

Antarmuka dirancang dengan teknologi web lokal jika integrasi native yang dibutuhkan dapat disediakan secara andal pada OBS 32.x. Pemrosesan audio tetap dilakukan oleh kode native plugin, bukan oleh animasi UI.

## 2. Tujuan proyek

- Mempermudah distribusi audio mono/stereo ke kanal surround.
- Memberikan kontrol gain individual untuk kanal yang didukung.
- Menyediakan kontrol panning yang mudah dipahami melalui tata letak speaker melingkar.
- Memungkinkan beberapa filter digunakan sekaligus, dengan dock kontrol terpisah untuk setiap instance.
- Memantau sinyal aktual pada titik mix/output OBS yang memang dapat diakses.
- Menyajikan antarmuka yang konsisten, rapi, dan terasa seperti alat audio profesional.
- Memisahkan pengolahan audio dari UI agar lebih mudah diuji dan dikembangkan.

## 3. Rancangan fitur

### 3.1 Filter Sorround Upmix

Filter direncanakan muncul pada daftar filter audio OBS dengan nama **Sorround Upmix**.

Fitur yang direncanakan:

- Memproses input mono atau stereo menjadi distribusi multikanal.
- Menyediakan mode target 5.1 dan 7.1 jika didukung oleh jalur audio OBS yang digunakan.
- Menangani input multikanal yang sudah memiliki kanal surround, dengan perilaku yang jelas dan tidak menggandakan kanal secara membabi buta.
- Menyediakan gain per kanal dengan rentang `-30 dB` sampai `+30 dB`.
- Menggunakan nilai default `0 dB` untuk setiap slider gain.
- Menyediakan tombol untuk membuka dock Sorround Control milik instance filter yang sedang diedit.
- Menyimpan parameter melalui settings OBS.

**Catatan kompatibilitas audio:** kemampuan memproses kanal pada sebuah filter tidak otomatis berarti seluruh output OBS atau perangkat audio Windows akan mengeluarkan 5.1/7.1. Hasil akhir bergantung pada layout mix/output, konfigurasi OBS, perangkat, driver, dan API yang tersedia. Plugin harus menampilkan status yang sesuai dengan kondisi nyata.

### 3.2 Kanal surround

Susunan kanal yang umum digunakan sebagai acuan UI:

| Label | Arti |
|---|---|
| L | Front Left — depan kiri |
| R | Front Right — depan kanan |
| C | Center — tengah depan |
| LFE | Low-Frequency Effects — kanal efek frekuensi rendah |
| SL | Side Left — surround samping kiri |
| SR | Side Right — surround samping kanan |
| RL | Rear/Back Left — belakang kiri |
| RR | Rear/Back Right — belakang kanan |

Pada konfigurasi 5.1, umumnya digunakan L, R, C, LFE, SL, dan SR. Pada 7.1 ditambahkan RL dan RR. Urutan internal buffer harus mengikuti layout yang didefinisikan oleh API OBS, bukan urutan tabel ini secara otomatis.

### 3.3 Gain setiap kanal

Setiap kanal yang didukung akan memiliki slider gain:

- Minimum: `-30 dB`
- Default: `0 dB`
- Maksimum: `+30 dB`

Nilai slider ditampilkan dalam dB agar mudah dibaca. Implementasi harus mengonversi nilai dB ke gain linear dengan rumus yang benar dan menangani risiko clipping ketika gain dinaikkan.

### 3.4 Sorround Control

Setiap instance filter memiliki dock kontrol sendiri. Contoh nama:

- `Music — Sorround Control`
- `Mic — Sorround Control`
- `Desktop Audio — Sorround Control`

Komponen yang direncanakan:

- Header dengan nama dock dan identitas filter/source.
- Peta speaker melingkar.
- Handle yang dapat digeser untuk menentukan arah/distribusi suara.
- Visualisasi level kanal.
- Slider gain kanal.
- Tombol untuk mengaktifkan atau menonaktifkan pembaruan meter.
- Status koneksi dan status filter.
- Credit dan tautan di bagian bawah.

Perubahan dari dock harus tersinkronisasi dengan properti filter OBS. Sebaliknya, perubahan dari properti filter harus tercermin pada dock yang terbuka.

Dock untuk satu filter tidak boleh mengubah filter lain. Membuka dock berulang kali untuk instance yang sama seharusnya memfokuskan dock yang sudah ada, bukan membuat banyak dock duplikat.

### 3.5 Sorround Monitor

Sorround Monitor ditujukan untuk memantau mix/output OBS secara keseluruhan pada titik pengukuran yang tersedia, bukan hanya source yang memiliki filter Sorround Upmix.

Komponen yang direncanakan:

- Peta speaker melingkar.
- Meter kanal individual yang benar-benar tersedia pada titik pengukuran.
- Informasi level, misalnya peak dalam dBFS.
- Indikator peak/clipping jika sudah diimplementasikan.
- Keterangan titik pengukuran dan layout kanal.
- Status ketika data audio tidak tersedia.
- Credit dan tautan di bagian bawah.

Jika titik pengukuran hanya menyediakan stereo, monitor harus menampilkan dua kanal depan atau menjelaskan keterbatasannya. Monitor tidak boleh mengarang level kanal surround untuk membuat visual terlihat penuh.

### 3.6 Gaya antarmuka

Arah desain:

- Tampilan gelap yang terinspirasi plugin audio profesional.
- Peta speaker menjadi fokus visual utama.
- Warna kanal konsisten di seluruh dock.
- Meter mudah dibaca tanpa mengganggu kontrol.
- Tipografi, jarak, dan ukuran kontrol konsisten.
- Tetap dapat digunakan ketika dock diperkecil.
- Animasi seperlunya, tanpa mengganggu pembacaan level audio.
- Semua kontrol aktif benar-benar terhubung ke state plugin.

### 3.7 Credit

Pada bagian bawah dock, tampilkan credit kecil:

**Aeria Sorround Upmix · linktr.ee/aeriacreative**

Tautan mengarah ke `https://linktr.ee/aeriacreative`.

## 4. Batasan teknis yang perlu dipahami

### 4.1 Upmix bukan output surround otomatis

Filter dapat mengolah distribusi kanal, tetapi output 5.1/7.1 harus didukung oleh pipeline audio OBS serta konfigurasi output yang digunakan. Plugin harus membedakan kanal input, hasil pemrosesan filter, mix internal, dan output perangkat.

### 4.2 Kanal LFE perlu perlakuan khusus

Kanal LFE bukan sekadar kanal depan yang dipindahkan. Jika plugin membangkitkan sinyal LFE, metode filter frekuensi rendah dan karakteristiknya harus ditentukan serta didokumentasikan. Jangan mengirim salinan full-range ke LFE tanpa alasan teknis.

### 4.3 Meter harus berdasarkan data nyata

Meter di Sorround Control harus mengukur sinyal kanal yang benar-benar diproses oleh instance filter. Sorround Monitor harus mengukur titik mix/output yang benar-benar dapat diakses. Animasi demo atau data contoh tidak boleh ditampilkan seolah-olah merupakan pengukuran nyata.

### 4.4 Dock web membutuhkan integrasi native

HTML/CSS/JavaScript dapat digunakan untuk tampilan, tetapi komunikasi dengan pemrosesan audio native memerlukan bridge yang aman dan sesuai dengan dependency OBS yang tersedia. Keberadaan Browser Source atau Browser Docks di OBS tidak otomatis memberi JavaScript akses ke state internal plugin.

Implementasi harus memverifikasi API dock OBS 32.x, ketersediaan Qt WebEngine atau alternatifnya, dan metode komunikasi native sebelum menetapkan arsitektur final.

## 5. Status pengembangan

Status implementasi dev0.0.1:

| Komponen | Status |
|---|---|
| Kerangka plugin dan build OBS 32.x | Selesai dibangun (MSVC 2022, CMake 4.4.3) |
| Filter Sorround Upmix | Selesai diimplementasikan |
| Gain individual kanal (-30 dB s/d +30 dB) | Selesai diimplementasikan |
| Mesin upmix 5.1/7.1 | Selesai diimplementasikan |
| Penyimpanan parameter settings OBS | Selesai diimplementasikan |
| Sorround Control per-instance | Selesai diimplementasikan |
| Kontrol panning melingkar | Selesai diimplementasikan |
| Meter kanal pada filter | Selesai diimplementasikan |
| Sorround Monitor mix/output | Selesai diimplementasikan |
| Packaging Windows (.zip di dist) | Selesai diimplementasikan |
| Instalasi ke OBS Studio Portable | Terpasang di portable OBS 32.2.2 |

## 6. Lingkungan pengembangan

Target pengembangan yang telah diverifikasi:

- Windows x64.
- OBS Studio 32.2.2 Portable (`OBS-Studio-32.2.2-Windows-x64`).
- Compiler: MSVC 19.44 (Visual Studio 2022 Build Tools).
- CMake: 4.4.3.
- OBS SDK / Dependencies: OBS Studio 31.1.1 + Qt 6.8 (obs-deps-qt6-2025-07-11-x64).
- Branch: `dev0.0.1`.

## 7. Cara membangun dan menginstal

Jalankan script otomatis dari direktori proyek:

```powershell
powershell -ExecutionPolicy Bypass -File .\build_and_install.ps1
```

Script akan otomatis:
1. Menjalankan `cmake --preset windows-x64`
2. Mengompilasi konfigurasi `RelWithDebInfo`
3. Memasang DLL dan locale ke OBS Studio Portable (`OBS-Studio-32.2.2-Windows-x64`)
4. Membuat arsip distribusi ZIP di folder `dist/` (`aeria-surroundupmix-v0.0.1-windows-x64.zip`)

## 8. Instalasi dan pemeriksaan awal

Langkah instalasi final bergantung pada struktur paket hasil build. Setelah paket tersedia:

1. Tutup OBS sebelum mengganti file plugin.
2. Salin plugin dan aset pendukung ke lokasi plugin yang sesuai dengan format paket OBS yang dipilih.
3. Pastikan seluruh dependency runtime tersedia.
4. Buka OBS Studio 32.x.
5. Periksa log OBS jika plugin tidak muncul atau gagal dimuat.
6. Tambahkan filter **Sorround Upmix** ke source audio yang sesuai.
7. Uji terlebih dahulu pada level audio rendah menggunakan sinyal uji.
8. Verifikasi kanal output dan pengukuran meter sebelum digunakan pada produksi langsung.

Jangan memasang build eksperimental langsung ke lingkungan produksi tanpa pengujian.

## 9. Rencana pengujian

Pengujian harus mencakup:

- Input mono, stereo, dan multikanal yang didukung.
- Sinyal kiri saja dan kanan saja untuk memeriksa pemetaan kanal.
- Gain `-30 dB`, `0 dB`, dan `+30 dB`.
- Penyimpanan serta pemulihan parameter.
- Banyak instance filter sekaligus.
- Pembukaan dock berulang kali.
- Sinkronisasi antara properti filter dan Sorround Control.
- Penghapusan source/filter ketika dock terbuka.
- Restart OBS.
- Output stereo versus multikanal pada titik pengukuran yang tersedia.
- Silence, sinyal tinggi, peak, dan clipping.
- Kinerja audio saat meter aktif maupun nonaktif.
- Penutupan OBS tanpa crash atau referensi objek yang tertinggal.

## 10. Struktur proyek

Struktur akhir dapat berbeda sesuai template plugin, tetapi pemisahan tanggung jawab yang disarankan:

```text
aeria-sorroundupmix/
├── AGENT.md
├── README.md
├── CMakeLists.txt
├── cmake/
├── data/
│   ├── locale/
│   └── ui/
├── src/
│   ├── audio/
│   ├── frontend/
│   ├── state/
│   └── util/
└── tests/
    ├── audio/
    └── state/
```

- `src/audio/`: filter OBS, mesin upmix, layout kanal, dan meter.
- `src/frontend/`: pembuatan dan pengelolaan dock.
- `src/state/`: parameter filter dan komunikasi antara UI dengan kode native.
- `data/ui/`: aset antarmuka web lokal.
- `tests/`: pengujian DSP, parameter, dan state.

## 11. Prinsip pengembangan

- Pemrosesan audio harus terpisah dari antarmuka.
- Callback audio tidak boleh menunggu UI atau melakukan operasi berat yang tidak diperlukan.
- UI tidak boleh mengakses buffer audio secara langsung.
- Semua parameter dari UI harus divalidasi.
- Semua kanal dan layout harus mengikuti definisi API OBS yang digunakan.
- Setiap dock kontrol hanya mengendalikan satu instance filter.
- Sorround Monitor harus menyebutkan titik pengukuran secara jelas.
- Jangan mengarang hasil pengujian atau menampilkan data simulasi sebagai data nyata.
- Dokumentasikan batasan API, dependency, dan keputusan desain.
- Perbarui README saat status fitur berubah.

## 12. Kredit

**Aeria Sorround Upmix**  
`https://linktr.ee/aeriacreative`

Dikembangkan sebagai proyek plugin audio OBS untuk Windows.
