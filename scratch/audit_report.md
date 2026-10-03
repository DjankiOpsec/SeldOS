# Аудит сетевой подсистемы и DNS SeldOS

## 1. Первопричина ошибки (Root Cause)
На скриншоте: ошибка `Diagnostic: DNS Resolution Failed (code -3)` при попытке открыть `http://info.cern.ch/hypertext/WWW/TheProject.html` в VirtualBox.
- **Лог запуска ядра в VirtualBox** (`/tmp/vbox_seldos.log`):
  `[-] e1000: No supported Intel network controller detected on PCI bus.`
  `[-] Net: No compatible NIC detected. Network offline.`
- **Конфигурация VM VirtualBox** (`vboxmanage showvminfo SeldOS`):
  Адаптер по умолчанию: `Type: Am79C973` (AMD PCnet-FAST III).
- Ядро SeldOS поддерживало исключительно контроллеры семейства `Intel e1000`. Драйвер сетевой карты не инициализировался, интерфейс оставался в статусе `link_up = 0`.
- Браузер и резолвер не проверяли статус линка перед отправкой UDP DNS-запроса, зависали на 4-секундный таймаут и возвращали общую ошибку таймаута DNS (`code -3`).

## 2. Проведенные исправления
1. **Реализован нативный драйвер AMD PCnet-FAST III / PCnet-PCI II (`kernel/drivers/pcnet.c`, `kernel/include/pcnet.h`)**:
   - Автодетект PCI Vendor `0x1022`, Device `0x2000` (дефолтный сетевой адаптер VirtualBox).
   - Чтение аппаратного MAC-адреса из APROM (порты `0x00`-`0x05`).
   - 32-битный Software Style (`BCR20 = 0x0102`), дескрипторные кольца TX/RX с флагами владения `0x8000` (OWN) и demand polling (`CSR0 TDMD`).
2. **Унификация сетевого стека (`kernel/net/net.c`)**:
   - Абстракция драйверов (`nic_is_active`, `nic_send_packet`, `nic_poll_packet`).
   - Поддержка как Intel e1000 (QEMU/VBox), так и AMD PCnet (VirtualBox по умолчанию).
   - Мгновенная проверка `net_is_online()` в `net_dns_resolve()` без 4-секундного зависания.
   - Параллельная отправка запросов на первичный DNS (`10.0.2.3:53`) и шлюз (`10.0.2.2:53`).
   - Парсинг CNAME, цепочек ответов и дополнительных записей (Additional records, `arcount`) в DNS-пакетах.
3. **Диагностика и обработка ошибок в юзерспейсе (`tor/http.c`, `tor/main.c`, `sh/main.c`)**:
   - Добавлен код ошибки `-15` (`Network Interface Offline / No Active NIC Carrier`).
   - Команда `ifconfig` в шелле явно сообщает статус `DOWN` и подсказку по настройке адаптера VM при отсутствии несущей.

## 3. Доказательства верификации (Verification Evidence)
- **QEMU с AMD PCnet (`-net nic,model=pcnet`)**:
  - Обнаружение `pcnet: Found AMD PCnet NIC (Device: 0x2000, I/O: 0xC000)`.
  - DNS-резолв: `Resolved: info.cern.ch -> 188.184.67.127`.
  - Успешный `ping 10.0.2.2`.
  - Успешная загрузка и рендеринг страницы CERN в Tor Browser: `scratch/test_cern_rendered.png`.
- **QEMU с Intel e1000 (`-net nic,model=e1000`)**:
  - Полный регрессионный сьют `tests/verify_real_internet.py` — **7/7 тестов пройдены успешно**.
- **Тест без сети (`-net none`)**:
  - Мгновенная диагностика `flags=DOWN` и `code -15` без таймаутов.
