# AGENT.md — Aeria Sorround Upmix

> **Dokumen instruksi utama untuk AI Agent (Google Antigravity).**  
> Agent wajib membaca dokumen ini sebelum mengubah kode, membuat berkas, memilih API, atau menyatakan fitur telah selesai.

## 1. Identitas proyek

- **Nama produk:** Aeria Sorround Upmix
- **Nama filter OBS:** `Sorround Upmix`
- **Nama dock kontrol:** `<Nama Filter> — Sorround Control`
- **Nama dock monitor:** `Sorround Monitor`
- **Folder proyek:** `aeria-sorroundupmix`
- **Versi awal:** `v0.0.1`
- **Target:** OBS Studio 32.x pada Windows
- **Bahasa dokumentasi:** Bahasa Indonesia
- **Bahasa antarmuka:** Bahasa Indonesia untuk versi awal, kecuali istilah teknis yang lebih jelas bila dibiarkan dalam bahasa Inggris
- **Jenis proyek:** Plugin native OBS dengan pemrosesan audio C/C++ dan antarmuka dock berbasis web bila dapat diintegrasikan secara stabil dengan API resmi OBS

### Aturan nama dan ejaan

Nama produk resmi tetap **Aeria Sorround Upmix** sesuai keputusan pemilik proyek. Jangan mengganti nama produk menjadi “Surround” secara otomatis, walaupun “surround” adalah ejaan bahasa Inggris yang umum. Nama class, simbol internal, file, dan ID boleh memakai ejaan teknis yang konsisten, tetapi nama yang terlihat pengguna harus mengikuti nama produk di atas.

## 2. Peran dan prioritas agent

Agent bertugas mengembangkan plugin yang benar-benar dapat dibangun, dimuat OBS, memproses audio nyata, dan menyediakan antarmuka yang terhubung ke state plugin. Jangan berhenti pada mockup, desain statis, prototipe HTML yang tidak terhubung, atau kode contoh yang tidak bisa dikompilasi.

Urutan prioritas:

1. Kompatibilitas dengan OBS Studio 32.x dan SDK/API yang benar-benar tersedia.
2. Keamanan audio: tidak crash, tidak mengakses buffer di luar batas, tidak membuat callback audio menunggu UI, dan tidak menghasilkan level tak terkendali.
3. Pemrosesan multikanal yang benar untuk layout dan jumlah kanal aktual.
4. Sinkronisasi parameter antara properti filter, Sorround Control, dan mesin audio.
5. Pembuatan/penghapusan dock serta penanganan siklus hidup objek yang aman.
6. Sorround Monitor yang mengukur sinyal aktual, bukan angka atau animasi simulasi.
7. Tampilan profesional, responsif, konsisten, dan mudah digunakan.
8. Dokumentasi, pengujian, dan kemudahan pemeliharaan.

Jangan mengklaim fitur selesai sebelum implementasi dan pengujian yang sesuai benar-benar dilakukan.

## 3. Aturan kerja wajib

Sebelum coding:

1. Periksa seluruh isi repository dan struktur proyek yang sudah ada. Jangan menimpa pekerjaan yang valid tanpa alasan.
2. Identifikasi versi OBS 32.x, arsitektur target (umumnya Windows x64), compiler, CMake, Qt, OBS development files, dan dependency yang benar-benar tersedia di lingkungan pengembangan.
3. Periksa header, contoh plugin, dokumentasi API, serta CMake package dari SDK OBS yang dipakai. Untuk setiap API penting, pastikan deklarasi dan tanda tangannya benar.
4. Buat `PLAN.md` atau rencana kerja singkat yang membagi pekerjaan menjadi tahap kecil dan menyebutkan cara verifikasi setiap tahap.
5. Bangun kerangka minimal yang dapat dikompilasi dan dimuat OBS sebelum menambah fitur besar.
6. Setiap tahap harus diakhiri dengan build dan pengujian yang relevan. Jangan menumpuk seluruh implementasi menjadi satu perubahan besar.

Saat bekerja:

- Utamakan API resmi OBS dan dependency yang sudah tersedia.
- Jangan mengarang nama fungsi OBS, signature callback, target CMake, path SDK, atau mekanisme JavaScript bridge.
- Jika API yang dibutuhkan tidak ditemukan, cari alternatif yang sah dan dokumentasikan keputusan tersebut.
- Jangan mengubah file build atau dependency menjadi konfigurasi khusus satu komputer tanpa mendokumentasikannya.
- Jangan menghapus error build dengan menonaktifkan fitur penting, menekan semua warning, atau mengganti implementasi dengan mock.
- Pisahkan perubahan audio, UI, build system, dan dokumentasi agar mudah ditinjau.
- Hindari operasi jaringan saat runtime. UI harus bisa berjalan dari aset lokal kecuali ada alasan yang disetujui.
- Jangan mengirim audio, log pribadi, atau data pengguna ke layanan eksternal.
- Bila kebutuhan pengguna bertentangan dengan batasan API OBS, jelaskan kendalanya dan ajukan alternatif yang paling dekat sebelum melakukan perubahan arsitektur besar.

## 4. Definisi produk

Plugin terdiri dari tiga komponen yang saling berhubungan.

### 4.1 Filter audio `Sorround Upmix`

Filter diterapkan pada sebuah source audio OBS. Fungsi utamanya:

- Memproses input mono atau stereo menjadi distribusi audio multikanal untuk output surround 5.1 atau 7.1, sejauh didukung oleh pipeline audio dan konfigurasi output OBS.
- Tetap menyediakan kontrol kanal bila input sudah multikanal. Filter tidak boleh menganggap semua input pasti stereo.
- Menyediakan gain terpisah untuk kanal output yang didukung.
- Memiliki tombol untuk membuka dock kontrol khusus bagi instance filter tersebut.
- Menyimpan parameter melalui mekanisme settings OBS agar tetap ada setelah OBS dimulai ulang.
- Menampilkan atau menjelaskan status layout/kanal input dan output bila informasi itu tersedia.

**Penting:** filter tidak dapat memaksa seluruh jalur audio OBS, perangkat output, driver, dan sistem operasi menjadi 5.1/7.1 hanya dengan mengubah satu source. Agent harus membedakan:
- layout kanal yang diterima filter;
- layout yang diproses filter;
- layout bus/mix OBS;
- layout yang dikirim ke perangkat/output.
Jika pipeline OBS yang digunakan tidak menyediakan output 5.1/7.1 pada konfigurasi pengguna, jelaskan batasannya dan jangan menampilkan status 7.1 seolah-olah sudah benar-benar keluar.

### 4.2 Dock `Sorround Control`

Setiap instance filter aktif dapat membuka **dock kontrol miliknya sendiri**. Dock ini tidak boleh secara diam-diam mengontrol semua filter.

Dock menampilkan:

- Nama filter dan identitas instance/source yang relevan.
- Kontrol panning dua dimensi berbentuk area melingkar dengan posisi speaker.
- Titik/handle yang dapat digeser untuk mengubah arah atau distribusi suara.
- Visualisasi level kanal.
- Slider gain setiap kanal yang didukung.
- Tombol untuk mengaktifkan/menonaktifkan visualisasi level.
- Tombol/aksi yang sesuai untuk menutup atau melepaskan hubungan dock dari instance.
- Credit kecil pada bagian bawah: `Aeria Sorround Upmix` dan tautan `https://linktr.ee/aeriacreative`.

Semua perubahan dari dock harus tercermin pada properti filter, dan perubahan dari properti filter harus tercermin di dock. Gunakan satu sumber state parameter, bukan dua implementasi terpisah.

### 4.3 Dock `Sorround Monitor`

Dock ini memantau **mix/output OBS yang benar-benar dipilih untuk dipantau**, bukan hanya source yang sedang dipasangi filter.

Fitur visual:

- Peta speaker berbentuk lingkaran atau tata letak surround.
- Indikator level kanal individual.
- Indikasi kanal yang mencapai puncak atau mendekati clipping.
- Pilihan atau keterangan jelas tentang titik pengukuran: misalnya mix/output yang memang bisa diakses melalui API OBS.
- Tombol/indikator status pengukuran.
- Credit kecil yang dapat diklik di bagian bawah.

Dock ini tidak memiliki kontrol panning atau slider gain per kanal. Ia hanya untuk pemantauan.

**Batasan penting:** jangan menyebutnya sebagai pengukuran output perangkat fisik atau sinyal yang didengar pengguna jika implementasi hanya dapat membaca mix internal OBS. Nama sumber pengukuran harus jelas di UI. Jika API yang tersedia hanya memberi akses ke mix tertentu atau kanal tertentu, tampilkan batasan itu.

## 5. Arsitektur yang diharapkan

Pisahkan kode menjadi modul yang memiliki tanggung jawab jelas. Nama file di bawah merupakan usulan awal; boleh diubah jika struktur repository yang ada mengharuskan pilihan lain, tetapi tanggung jawab harus tetap dipisahkan.

```text
aeria-sorroundupmix/
├── AGENT.md
├── README.md
├── CMakeLists.txt
├── cmake/
├── data/
│   ├── locale/
│   ├── ui/
│   │   ├── index.html
│   │   ├── styles.css
│   │   ├── app.js
│   │   └── assets/
│   └── README.md
├── src/
│   ├── plugin-main.cpp
│   ├── plugin-main.hpp
│   ├── audio/
│   │   ├── surround-filter.cpp
│   │   ├── surround-filter.hpp
│   │   ├── upmix-engine.cpp
│   │   ├── upmix-engine.hpp
│   │   ├── channel-layout.hpp
│   │   ├── level-meter.cpp
│   │   └── level-meter.hpp
│   ├── frontend/
│   │   ├── dock-manager.cpp
│   │   ├── dock-manager.hpp
│   │   ├── control-dock.cpp
│   │   ├── control-dock.hpp
│   │   ├── monitor-dock.cpp
│   │   └── monitor-dock.hpp
│   ├── state/
│   │   ├── filter-state.cpp
│   │   ├── filter-state.hpp
│   │   ├── state-bridge.cpp
│   │   └── state-bridge.hpp
│   └── util/
├── tests/
│   ├── audio/
│   ├── state/
│   └── README.md
└── .gitignore
```

Jangan membuat seluruh logika dalam satu file besar. UI HTML/CSS/JavaScript harus terpisah dari pemrosesan audio native.

### 5.1 Lapisan plugin OBS

Tanggung jawab:

- Registrasi plugin.
- Registrasi `obs_source_info` untuk filter audio.
- Inisialisasi dan penghentian plugin.
- Registrasi menu/aksi frontend jika dibutuhkan.
- Pendaftaran serta pelepasan callback dengan benar.
- Penanganan shutdown tanpa callback yang masih menunjuk ke objek yang sudah dihapus.

Gunakan pola API dan contoh plugin yang sesuai dengan OBS 32.x. Jangan menyalin API dari versi OBS lama tanpa memeriksa apakah masih tersedia.

### 5.2 Mesin audio

Tanggung jawab:

- Memeriksa layout dan jumlah kanal input.
- Menentukan strategi pemetaan/upmix.
- Menerapkan gain per kanal.
- Menjaga format sampel, jumlah frame, timestamp, dan layout sesuai kontrak API yang digunakan.
- Menangani buffer tanpa alokasi memori yang tidak perlu pada callback audio.
- Tidak mengakses objek UI dari thread audio.
- Menyediakan pengukuran level dengan biaya komputasi yang wajar.

Pisahkan algoritma DSP dari callback OBS agar dapat diuji menggunakan buffer audio sintetis di unit test.

### 5.3 State dan sinkronisasi

Satu instance filter mempunyai state tersendiri. Minimal state logis:

- ID internal instance yang stabil selama masa hidup instance.
- Nama filter yang ditampilkan.
- Identitas source pemilik, jika tersedia.
- Layout input aktual yang diketahui.
- Mode/target layout yang dipilih.
- Gain kanal.
- Posisi panning.
- Status visualisasi.
- Status koneksi dock.
- Versi skema settings untuk migrasi di masa depan.

Jangan menggunakan nama filter saja sebagai satu-satunya kunci. Nama dapat sama dan dapat berubah. Gunakan identitas instance yang dikelola plugin serta mekanisme validasi umur objek.

Semua update dari UI harus divalidasi sebelum diterapkan. Hindari race condition antara thread audio, thread frontend, dan JavaScript. Gunakan snapshot parameter, atomic, mutex yang tidak memblokir callback audio, atau mekanisme aman lain yang sesuai dengan desain dan telah diverifikasi.

### 5.4 Antarmuka web dan dock OBS

Pengguna menginginkan antarmuka bergaya plugin VST profesional. Prioritaskan HTML/CSS/JavaScript lokal untuk visualnya, tetapi integrasi ke dock OBS harus memakai mekanisme yang benar-benar didukung.

Sebelum memilih implementasi:

1. Verifikasi apakah OBS 32 dan dependency instalasi target menyediakan API dock frontend yang dibutuhkan.
2. Verifikasi apakah `QWebEngineView` tersedia pada toolchain/dependency yang digunakan. Jangan menganggap Qt WebEngine otomatis tersedia hanya karena OBS menggunakan Qt.
3. Verifikasi cara aman menjembatani UI web dengan kode native. Gunakan bridge lokal yang terkontrol, misalnya `QWebChannel` hanya bila dependency dan integrasinya benar-benar tersedia, atau mekanisme resmi lain yang tervalidasi.
4. Jangan menganggap dock Browser OBS otomatis memiliki akses JavaScript ke state internal plugin.
5. Jangan mengekspos server HTTP/WebSocket tanpa autentikasi atau perlindungan hanya demi mempermudah UI.
6. Jika antarmuka web tertanam tidak bisa didistribusikan secara andal, dokumentasikan hambatannya dan usulkan fallback Qt native yang mempertahankan visual profesional. Jangan diam-diam mengganti persyaratan tanpa menjelaskan alasannya.

Aset UI harus lokal dan disertakan dalam paket plugin. UI harus memiliki status kosong, status memuat, status koneksi terputus, dan status error yang dapat dipahami.

## 6. Spesifikasi DSP dan pemetaan kanal

### 6.1 Kanal yang dituju

Layout 5.1 umumnya memiliki enam kanal:
- L — Front Left
- R — Front Right
- C — Center
- LFE — Low-Frequency Effects
- SL — Surround Left
- SR — Surround Right

Layout 7.1 umumnya memiliki delapan kanal:
- L — Front Left
- R — Front Right
- C — Center
- LFE — Low-Frequency Effects
- SL — Side Left
- SR — Side Right
- RL — Rear/Back Left
- RR — Rear/Back Right

Penamaan dan urutan kanal internal **tidak boleh diasumsikan**. Verifikasi konstanta speaker/layout OBS dan urutan buffer dari header serta dokumentasi versi target. UI boleh memakai label yang mudah dimengerti, tetapi pemetaan internal harus mengikuti urutan yang benar.

### 6.2 Pilihan mode

Rancang pilihan mode yang eksplisit, misalnya:

- **Otomatis:** mengikuti layout yang dapat diproses dengan aman.
- **Upmix 5.1:** gunakan strategi distribusi 5.1 jika pipeline mengizinkan.
- **Upmix 7.1:** gunakan strategi distribusi 7.1 jika pipeline mengizinkan.
- **Kontrol kanal saja:** untuk input multikanal, menerapkan gain tanpa memaksakan algoritma upmix tambahan jika pengguna memilih perilaku ini.

Mode yang benar-benar didukung harus ditentukan setelah memeriksa API OBS dan pipeline audio. Jangan menawarkan pilihan yang tidak dapat diterapkan.

### 6.3 Perilaku upmix

Upmix tidak sama dengan menyalin sampel stereo mentah ke semua kanal. Implementasikan matriks/rule pemetaan yang terdokumentasi dan menghindari kenaikan loudness yang tidak disengaja.

Persyaratan:

- Mono dapat diposisikan atau didistribusikan ke kanal yang sesuai.
- Stereo mempertahankan citra depan kiri/kanan.
- Kanal center, surround, dan rear memperoleh sinyal berdasarkan algoritma yang dijelaskan.
- Kanal LFE tidak boleh sekadar menerima salinan full-range tanpa alasan. Gunakan low-pass/filter yang sesuai jika memang dirancang menghasilkan sinyal LFE; dokumentasikan cutoff, kemiringan filter, dan perilaku.
- Input 5.1/7.1 yang sudah ada tidak boleh di-upmix ulang secara membabi buta sehingga menciptakan duplikasi kanal atau lonjakan level.
- Kanal yang tidak tersedia tidak boleh dibaca.
- Tangani input diam, nilai non-finite bila mungkin, dan buffer kosong sesuai kontrak API.
- Hindari clipping dengan desain gain staging yang tepat. Jangan menganggap slider +30 dB aman untuk semua sinyal.
- Jangan melakukan normalisasi otomatis yang mengubah dinamika secara tidak terdokumentasi.

Buat dokumentasi singkat tentang matriks/rule pemetaan yang digunakan. Bila memakai koefisien, jelaskan arti dan alasan pemilihannya. Jangan menyatakan bahwa hasilnya setara dengan mastering surround profesional atau hasil encoding film.

### 6.4 Gain per kanal

Rentang UI yang diminta:

- Minimum: `-30 dB`
- Maksimum: `+30 dB`
- Nilai default: `0 dB`

Sediakan slider dan angka numerik yang mudah dibaca. Perubahan harus memengaruhi kanal yang sesuai. Konversi dB ke gain linear harus benar:

`gain_linear = 10^(gain_db / 20)`

Nilai parameter harus dibatasi dan divalidasi. Jangan memakai penambahan linear pada sampel untuk mewakili dB. Tentukan apakah nilai disimpan dalam dB atau gain linear; untuk settings dan UI, menyimpan dB biasanya lebih mudah dipahami.

### 6.5 Kontrol panning melingkar

Kontrol utama berupa bidang melingkar dengan:
- speaker ditampilkan pada posisi relatif yang konsisten;
- titik sumber/handle yang dapat digeser;
- pusat sebagai posisi tengah;
- batas gerak yang jelas;
- indikator posisi saat ini;
- dukungan mouse dan keyboard dasar bila memungkinkan.

Gerakan handle harus menghasilkan perubahan distribusi kanal yang nyata, bukan hanya menggerakkan gambar. Definisikan koordinat, normalisasi jarak, dan aturan distribusi ke speaker. Pertimbangkan equal-power panning atau pendekatan lain yang sesuai dan dokumentasikan. Hindari perubahan level mendadak ketika handle bergerak.

Untuk v0.0.1, kontrol panning dapat dirancang sebagai **kontrol arah/distribusi suara yang sederhana dan deterministik**. Jangan menjanjikan simulasi akustik ruangan, jarak, HRTF, atau posisi 3D realistis kecuali benar-benar diimplementasikan dan diuji.

### 6.6 Meter level

Tampilkan level kanal berdasarkan sinyal aktual. Tentukan apakah meter menggunakan peak, RMS, atau keduanya, lalu beri label yang sesuai.

- Konversi amplitudo ke dBFS dengan penanganan nilai nol yang aman.
- Tetapkan floor tampilan, misalnya sekitar `-60 dBFS`, tanpa mengubah data audio.
- Tampilkan puncak/indikator clipping secara jujur.
- Refresh UI pada frekuensi yang wajar; jangan memperbarui DOM setiap sampel audio.
- Jalur pengukuran harus non-blocking bagi callback audio.
- Saat preview meter dimatikan, hentikan update UI yang tidak diperlukan, bukan mematikan pemrosesan audio utama.

## 7. Persyaratan filter OBS

Filter harus memiliki:

- Nama yang muncul di daftar filter OBS: `Sorround Upmix`.
- Deskripsi yang menjelaskan bahwa filter memproses distribusi/gain kanal audio.
- Properti mode/layout yang memang didukung.
- Slider gain per kanal dengan rentang `-30 dB` hingga `+30 dB`, default `0 dB`.
- Tombol `Buka Sorround Control`.
- Indikator nama/ID instance atau informasi yang membantu pengguna menghubungkan filter dengan dock yang tepat.
- Penyimpanan dan pemulihan parameter melalui settings OBS.
- Validasi saat settings lama atau tidak lengkap dibaca.

Tombol buka dock harus membuka atau memfokuskan dock milik instance filter yang sama, bukan membuat dock duplikat setiap kali diklik. Jika dock belum ada, buat. Jika sudah ada, fokuskan. Jika instance filter dihapus, putuskan koneksi dan bersihkan resource yang terkait dengan aman.

Jika API properti OBS tidak mendukung tombol dengan perilaku yang diinginkan, teliti mekanisme UI alternatif yang sah. Jangan membuat tombol dekoratif yang tidak bekerja.

## 8. Persyaratan Sorround Control

### 8.1 Tata letak UI

Prioritas visual:

1. Header dengan nama `Sorround Control`, nama filter, dan nama source.
2. Area panning speaker melingkar sebagai elemen utama.
3. Visualisasi level kanal yang berada di bawah/lapisan visual kontrol utama tanpa menutupi handle.
4. Slider gain kanal yang tersusun rapi.
5. Tombol meter aktif/nonaktif dan status koneksi.
6. Footer credit kecil.

Gunakan gaya antarmuka audio profesional:
- latar gelap netral;
- kontras teks yang baik;
- aksen warna terbatas untuk kanal/level;
- tipografi konsisten;
- jarak antarelemen teratur;
- slider dan knob dengan status hover/focus;
- desain yang tetap terbaca saat dock diperkecil;
- tidak berlebihan memakai glow, animasi, atau gradient;
- animasi hanya untuk membantu pemahaman dan tidak mengganggu pembacaan meter.

Warna setiap kanal harus konsisten antara diagram, slider, dan monitor. Jangan mengandalkan warna saja; label kanal harus tetap tersedia.

### 8.2 Sinkronisasi

- Perubahan slider di dock memperbarui parameter native.
- Perubahan slider di properti filter OBS memperbarui dock yang terbuka.
- Perubahan handle panning memperbarui state DSP dan semua tampilan parameter terkait.
- Perubahan settings yang masuk dari OBS harus divalidasi.
- UI tidak boleh mengirim update berulang tanpa batas akibat sinkronisasi dua arah.
- Gunakan ID parameter dan skema pesan yang jelas, dengan versi jika diperlukan.
- Saat koneksi putus, tampilkan status yang jelas dan cegah perubahan yang tampak berhasil padahal tidak diterapkan.

### 8.3 Banyak instance

Setiap instance filter harus memiliki dock yang terpisah. Contoh:
- `Music — Sorround Control`
- `Mic — Sorround Control`
- `Desktop Audio — Sorround Control`

Nama tampilan harus mengandung identitas pembeda jika ada beberapa filter dengan nama yang sama. Hindari nama dock yang ambigu. Saat instance dihapus, dock harus ditutup atau masuk status terputus lalu dibersihkan sesuai kebijakan siklus hidup yang dipilih.

## 9. Persyaratan Sorround Monitor

Monitor harus mengukur sinyal mix/output yang benar-benar tersedia melalui API OBS. Sebelum coding, tentukan sumber pengukuran yang tepat dengan memeriksa API publik OBS 32.x.

Agent wajib menjawab pertanyaan berikut dalam dokumentasi teknis:

1. Apakah pengukuran dilakukan pada mix utama, output tertentu, atau titik lain?
2. Apakah pengukuran tersedia per kanal?
3. Apakah layout 5.1/7.1 benar-benar tersedia pada titik tersebut?
4. Jika OBS menghasilkan mix stereo, apakah UI menampilkan hanya dua kanal depan atau memberi tahu bahwa titik pengukuran memang stereo?
5. Apakah pengukuran terjadi sebelum atau sesudah filter tertentu?
6. Apa keterbatasan yang tidak dapat diketahui plugin?

Jangan menampilkan delapan kanal aktif jika data sebenarnya hanya stereo. Jangan membuat data palsu untuk melengkapi visual.

### 9.1 Tampilan monitor

- Peta speaker melingkar yang sejalan dengan diagram di Sorround Control.
- Meter individual untuk kanal yang tersedia.
- Nilai peak dBFS atau nilai meter lain yang didefinisikan.
- Indikator clipping/peak hold bila diimplementasikan.
- Status sumber pengukuran dan jumlah/layout kanal.
- Pesan yang informatif saat sumber pengukuran tidak tersedia.
- Tombol meter aktif/nonaktif bila relevan.
- Footer credit kecil dan tautan `https://linktr.ee/aeriacreative`.

Monitor tidak boleh menyediakan slider gain/panning. Perubahan tampilan monitor tidak boleh memodifikasi audio.

## 10. Credit dan tautan

Pada bagian paling bawah setiap dock, tampilkan teks kecil:

`Aeria Sorround Upmix · linktr.ee/aeriacreative`

Tautan harus menuju `https://linktr.ee/aeriacreative`, dapat diklik, dan dibuka menggunakan mekanisme yang aman. Jangan memuat halaman tautan tersebut di dalam UI plugin hanya untuk membuat credit dapat diklik. Credit tidak boleh mendominasi ruang UI.

## 11. Pengaturan, persistensi, dan kompatibilitas

- Simpan semua parameter yang perlu dipertahankan melalui settings OBS.
- Tetapkan nilai default yang konsisten dengan UI dan mesin audio.
- Gunakan skema settings yang dapat diberi versi agar migrasi berikutnya dimungkinkan.
- Tangani field yang tidak ada, nilai di luar rentang, serta versi settings lama.
- Jangan menyimpan pointer, handle UI, atau data runtime sementara ke settings.
- Jangan menganggap nama source selalu unik atau permanen.
- Pastikan dock dan callback dibersihkan ketika source/filter dihapus, scene berubah, plugin dinonaktifkan, atau OBS ditutup.
- Pastikan aset UI ditemukan menggunakan path runtime plugin yang benar, bukan working directory saat ini.
- Dokumentasikan batas kompatibilitas Windows, arsitektur, versi OBS, dan dependency.

## 12. Build dan distribusi Windows

Gunakan CMake atau sistem build yang paling cocok dengan template plugin OBS yang dipilih. Konfigurasi final harus diverifikasi terhadap SDK OBS 32.x yang nyata.

Build yang diharapkan:
- konfigurasi Debug untuk pengembangan;
- konfigurasi Release untuk distribusi;
- arsitektur x64 bila target OBS x64;
- output plugin, manifest, dan aset UI berada pada struktur paket yang dapat dipasang;
- tidak mengandalkan path absolut milik komputer pengembang;
- dependency runtime yang diperlukan didokumentasikan.

Sediakan instruksi build yang menyebutkan:
- versi Visual Studio/Build Tools yang diuji;
- versi CMake yang diuji;
- cara memperoleh atau menentukan OBS development package;
- perintah konfigurasi dan build;
- lokasi hasil build;
- cara memasang untuk uji lokal;
- cara mengumpulkan log jika plugin gagal dimuat.

Jangan mengarang perintah final sebelum menguji toolchain. Perintah dalam README harus sesuai dengan konfigurasi repository saat ini.

## 13. Rencana implementasi bertahap

### Tahap 0 — Audit dan rencana
- Periksa repository, toolchain, SDK, dan dependency.
- Tetapkan strategi dock/web UI yang layak.
- Tetapkan cara pengukuran mix untuk Sorround Monitor.
- Catat risiko teknis dan hasil pemeriksaan.
- Kriteria selesai: rencana dan keputusan arsitektur terdokumentasi.

### Tahap 1 — Kerangka plugin
- Buat struktur CMake dan registrasi plugin.
- Registrasikan filter audio sederhana.
- Pastikan plugin dapat dibangun dan dimuat di OBS 32.x.
- Tambahkan log diagnostik yang ringkas.
- Kriteria selesai: filter muncul di UI OBS dan dapat ditambahkan/dihapus tanpa crash.

### Tahap 2 — Jalur audio dan meter dasar
- Periksa format/layout input.
- Implementasikan pemrosesan pass-through yang aman terlebih dahulu.
- Tambahkan pengukuran kanal aktual.
- Uji mono, stereo, dan multikanal bila tersedia.
- Kriteria selesai: audio tetap terdengar benar dan meter tidak menampilkan data palsu.

### Tahap 3 — Gain kanal
- Tambahkan parameter gain dB per kanal yang benar-benar didukung.
- Implementasikan konversi dB ke linear.
- Simpan/pulihkan settings.
- Kriteria selesai: setiap slider mengubah kanal yang sesuai dan default adalah 0 dB.

### Tahap 4 — Upmix
- Implementasikan strategi upmix yang terdokumentasi.
- Tangani mono/stereo dan input multikanal.
- Tambahkan pembatasan/validasi untuk mencegah kesalahan buffer dan lonjakan tak terkendali.
- Kriteria selesai: uji sinyal per kanal membuktikan pemetaan sesuai desain.

### Tahap 5 — Dock dan bridge
- Implementasikan dock melalui API yang sudah diverifikasi.
- Hubungkan UI dengan state native.
- Pastikan satu dock per instance filter.
- Kriteria selesai: perubahan UI memengaruhi DSP dan sebaliknya; penutupan/penghapusan instance tidak meninggalkan referensi rusak.

### Tahap 6 — Sorround Control
- Bangun tampilan profesional berbasis aset lokal.
- Tambahkan diagram speaker, handle panning, slider kanal, meter, dan footer.
- Kriteria selesai: tampilan responsif dan semua kontrol yang tampak aktif benar-benar bekerja.

### Tahap 7 — Sorround Monitor
- Implementasikan capture/pengukuran mix yang didukung API.
- Tampilkan layout dan kanal yang benar-benar tersedia.
- Tambahkan indikator peak/clipping yang didefinisikan.
- Kriteria selesai: monitor menunjukkan hasil pengukuran nyata dan menjelaskan keterbatasan titik pengukuran.

### Tahap 8 — Stabilitas dan packaging
- Uji banyak instance, reload scene, rename source, hapus filter, tutup dock, restart OBS, dan shutdown.
- Periksa log, memory/lifetime, sinkronisasi, serta kinerja.
- Buat paket instalasi manual dan dokumentasi.
- Kriteria selesai: checklist uji lulus dan README mencerminkan keadaan implementasi sebenarnya.

Agent boleh mengubah urutan hanya dengan alasan teknis yang jelas dan terdokumentasi.

## 14. Strategi pengujian

### 14.1 Build dan pemuatan
- Build Debug dan Release.
- Plugin dapat ditemukan OBS.
- Plugin gagal dimuat dengan pesan yang dapat ditelusuri bila dependency hilang.
- Tidak ada path aset yang bergantung pada folder kerja.

### 14.2 Uji audio
Gunakan sinyal uji yang mudah diidentifikasi:
- mono dengan level konstan;
- stereo kiri saja;
- stereo kanan saja;
- sinyal berbeda pada setiap kanal multikanal;
- silence;
- sinyal mendekati 0 dBFS;
- beberapa frekuensi untuk menguji jalur LFE bila dibuat.

Pastikan:
- tidak ada kanal yang tertukar;
- kanal yang tidak ditargetkan tidak berubah secara tidak sengaja;
- gain 0 dB tidak mengubah amplitudo selain akibat algoritma upmix;
- gain negatif/positif sesuai perhitungan;
- perubahan mode tidak menyebabkan lonjakan atau buffer invalid;
- audio tidak berhenti ketika UI dock dibuka/ditutup.

### 14.3 Uji UI/state
- Buka dock berulang kali: tidak membuat duplikat untuk instance yang sama.
- Dua instance: kontrol tidak tertukar.
- Ubah parameter dari properti filter: dock ikut berubah.
- Ubah parameter dari dock: properti filter ikut berubah.
- Tutup dock lalu buka kembali: state sesuai filter.
- Hapus source/filter saat dock terbuka: tidak crash.
- Ubah nama source/filter: dock tetap mengidentifikasi instance dengan benar.
- Matikan meter: audio tetap diproses.

### 14.4 Uji Sorround Monitor
- Input/output stereo: monitor tidak mengarang kanal surround.
- Output multikanal yang didukung: kanal yang benar tampil.
- Sinyal hening: meter menunjukkan silence/floor yang benar.
- Sinyal tinggi: indikator peak/clipping bekerja sesuai definisi.
- Perubahan scene/source tidak meninggalkan callback yang menunjuk objek lama.

### 14.5 Kinerja dan keamanan
- Tidak ada alokasi memori besar atau operasi file/network pada callback audio.
- Tidak ada operasi UI dari thread audio.
- Tidak ada loop tak terbatas pada JavaScript bridge.
- Nilai dari UI divalidasi sebelum masuk DSP.
- Semua callback, timer UI, dan koneksi dibersihkan dengan aman.

## 15. Definisi selesai untuk v0.0.1

Fitur hanya boleh ditandai selesai jika:
- Plugin dibangun untuk target OBS 32.x Windows yang terdokumentasi.
- Filter `Sorround Upmix` muncul dan bisa ditambahkan.
- Jalur audio nyata berjalan dan tidak sekadar mock.
- Mode dan pemetaan kanal yang ditawarkan sesuai kemampuan aktual implementasi.
- Gain kanal bekerja dalam rentang yang ditentukan.
- Settings tersimpan dan pulih.
- Dock per-instance tidak tertukar atau berduplikasi.
- Sorround Control tersambung ke state native.
- Sorround Monitor menampilkan pengukuran aktual dengan label titik pengukuran.
- UI menangani error dan koneksi terputus.
- Credit/tautan muncul di bagian bawah kedua dock.
- Instruksi build, instalasi, batasan, dan pengujian diperbarui.
- Tidak ada klaim pengujian yang belum benar-benar dijalankan.

Jika ada fitur yang belum selesai, tandai sebagai **Belum diimplementasikan**, **Eksperimental**, atau **Dibatasi oleh API/pipeline**. Jangan menyamarkan fitur belum jadi sebagai fitur aktif.

## 16. Format laporan setiap tahap

Pada akhir setiap tahap, berikan ringkasan dalam format:

1. **Perubahan:** file/fitur yang ditambahkan atau diubah.
2. **Alasan teknis:** mengapa implementasi dipilih.
3. **Build:** perintah yang dijalankan dan hasil sebenarnya.
4. **Pengujian:** skenario yang dijalankan dan hasilnya.
5. **Kendala:** error, API yang tidak tersedia, atau keterbatasan yang ditemukan.
6. **Langkah berikutnya:** pekerjaan paling kecil dan masuk akal berikutnya.

Jangan menyatakan “berhasil” jika build atau pengujian belum dijalankan. Jika lingkungan agent tidak memiliki OBS/SDK untuk uji runtime, katakan dengan eksplisit bahwa hasil tersebut baru diverifikasi pada level build atau inspeksi kode.
