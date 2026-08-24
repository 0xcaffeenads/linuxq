# cartesi-busybox-lite

Cartesi Machine (RISC-V) için aşırı basit, BusyBox tabanlı bir root filesystem
üreten build sistemi + GitHub Actions ile build/deploy workflow'u.

## Mimarinin mantığı

Cartesi bir "her şeyi kendin derle" sistemi değildir: temel Linux kernel'i
Cartesi tarafından (RISC-V64 için) sağlanır. Sizin ürettiğiniz şey **rootfs**
(BusyBox + kendi init scriptiniz + uygulamanız) olur. Bu rootfs bir imaja
paketlenir, hash'i (Merkle root) hesaplanır ve o hash zincirdeki DApp
kontratına yazılır. Node çalışırken input'ları bu machine image'ı içinde
deterministik olarak işler.

Bu repo şunu yapar:
1. `docker/Dockerfile` → statik BusyBox derler, minimal bir rootfs dizini
   kurar (`/bin`, `/sbin`, `/etc`, custom `/init`).
2. `scripts/build-image.sh` → rootfs'i ext2 image'a çevirir, Cartesi'nin
   `cartesi-machine` aracıyla (resmi kernel + bu rootfs) machine hash'ini
   hesaplar.
3. `.github/workflows/build-and-deploy.yml` → `workflow_dispatch` ile
   tetiklenir, network/RPC/kontrat adresi gibi **hassas olmayan** parametreleri
   input olarak alır; private key'i ise **GitHub Secrets**'tan okur.

## Neden private key workflow_dispatch input'u OLMAMALI

- Actions UI'daki input'lar ve run logları, repoya "read" erişimi olan
  herkese görünür (private repo olsa bile organizasyon içindeki herkese).
  Secrets ise loglara maskelenerek yazılır ve yetkisiz kullanıcılara
  gösterilmez.
- Bir input değişkenini yanlışlıkla `echo` etmek/log'a düşürmek çok kolaydır;
  secret'lar GitHub tarafından otomatik redakte edilir (`***`).
- Bu yüzden bu repoda `PRIVATE_KEY` **sadece** `secrets.DEPLOYER_PRIVATE_KEY`
  olarak, workflow dosyasında env değişkenine set edilerek kullanılıyor.

Kurulum: repo → Settings → Secrets and variables → Actions → New repository
secret → `DEPLOYER_PRIVATE_KEY`.

## Kullanım

1. Bu repoyu GitHub'a push'la.
2. `DEPLOYER_PRIVATE_KEY` secret'ını ekle.
3. Actions sekmesinden `Build & Deploy Cartesi Machine` workflow'unu seç,
   "Run workflow" de, network/RPC/kontrat adresini gir.
4. Workflow BusyBox rootfs'i derler, machine hash'ini hesaplar, çıktı
   olarak `.car` / ext2 image'ı artifact olarak bırakır ve istersen
   `--deploy` adımında `cast send` ile hash'i kontrata yazar.

## DApp: basit "echo" örneği

`dapp/app.c` — dış bağımlılığı olmayan (libcurl vs. yok), ham POSIX socket'lerle
Cartesi Rollup HTTP API'sine konuşan minimal bir C programı. `init` scripti
bunu `/bin/app` olarak PID 1'den hemen sonra çalıştırır.

Mantığı:
- `advance_state` (zincirden gelen input) → payload'u aynen bir **notice**
  olarak geri yollar (zincirde doğrulanabilir çıktı).
- `inspect_state` (zincir dışı sorgu) → payload'u aynen bir **report**
  olarak geri yollar.

Bu, Cartesi ekosisteminde referans "echo dapp" örneğinin C ile, BusyBox'a
uygun ağırlıkta yazılmış hali. Kendi iş mantığınızı eklemek için
`app.c` içindeki `advance_state` bloğunu değiştirmeniz yeterli — payload'u
decode edip istediğiniz hesaplamayı yapıp sonucu tekrar hex'e çevirerek
notice/voucher olarak gönderebilirsiniz.

Test etmek için (yerelde, Cartesi'nin "host mode" ortamıyla):
```bash
# cartesi-machine kurulu değilse: https://docs.cartesi.io kurulum adımları
export ROLLUP_HTTP_SERVER_URL=http://127.0.0.1:5004
# rollup-http-server'ı ayrı bir terminalde başlatıp sonra:
./out/rootfs/bin/app
```

## Yerelde build (opsiyonel)

```bash
docker build -t busybox-lite-rootfs -f docker/Dockerfile .
docker run --rm -v $(pwd)/out:/out busybox-lite-rootfs
```

Bu repo, ağ erişimi gerektiren adımları (BusyBox kaynak indirme, Cartesi SDK
image'ı çekme) CI ortamında çalıştırmak üzere tasarlandı; bu chat ortamının
kendisinde internet erişimi olmadığından burada derleme test edilmedi —
dosyalar standart, resmi Cartesi/BusyBox akışlarını takip ediyor.
