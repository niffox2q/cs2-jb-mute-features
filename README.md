### Модуль Mute Features для Jailbreak Core  

Добавляет систему управления мутом заключённых в режиме Jailbreak с возможностью автоматического снятия мута для определённых игроков.

Модуль работает следующим образом:
- при выдаче мута заключённому проверяются исключения
- игрок автоматически размучивается, если у него есть доступ
- поддерживаются 3 типа исключений:
  - право администратора (`@admin/...`)
  - список SteamID из конфигурации
  - VIP-иммунитет через VIP систему

Иммунитет VIP настраивается через фичу `jb_mute_immunity`. ВАЖНО, в меню оно не отображается, работает пассивно.

Все SteamID и права берутся из конфигурационного файла `addons/configs/Jailbreak/mute_features.ini`.

### Установка:
Распаковать в `game/csgo/addons/`

### Требования:
- [Jailbreak Core](https://discord.gg/WkTwuKe8zy)
- [Utils](https://github.com/Pisex/cs2-menus)
- [Admin System](https://github.com/Pisex/cs2-admin_system)
- [VIP System](https://github.com/Pisex/cs2-vip/tree/main)
