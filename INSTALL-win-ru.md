# Установка на Windows (WSL)

Скачать Arch WSL, распаковать в любую папку, запустить Arch.exe

## Разбираемся с Arch и systemd
```bash
wsl
cd ~

# На вопрос про fakeroot ответить "не ставить"
pacman -Syu base-devel vim git openssh cmake ninja fcgi fmt nlohmann-json nodejs-lts-gallium npm \
	postgresql-libs uriparser libev docker docker-compose

# Нам нужна чуть более старая версия protobuf
pacman -U https://archive.archlinux.org/packages/p/protobuf/protobuf-3.20.1-2-x86_64.pkg.tar.zst

# Раскомментировать строку 88 про пользователей из группы wheel без пароля
EDITOR=vim visudo

useradd -G wheel -m user
su -l user
git clone https://aur.archlinux.org/daemonize.git
cd daemonize
makepkg -sri
exit
curl -OL https://github.com/arkane-systems/genie/releases/download/v2.4/genie-systemd-2.4-1-x86_64.pkg.tar.zst
pacman -U genie-systemd-2.4-1-x86_64.pkg.tar.zst

# Следующие две команды будут давать миллиард ошибок и предупреждений
# Оно зависнет на 2 минуты и вывалится с ошибкой, поэтому можешь спустя секунд 10 кильнуть ctrl+c 
genie -s

# А вот во второй раз худо-бедно сработает
genie -s
systemctl set-default multi-user.target
```

Надо применить фикс [отсюда](https://github.com/arkane-systems/genie/wiki/Systemd-units-known-to-be-problematic-under-WSL#systemd-sysusersservice), там вставить строчки в
файл, который откроется:

```bash
EDITOR=vim systemctl edit systemd-sysusers.service

systemctl mask getty@tty1.service
exit
genie -u
exit
wsl --shutdown
wsl

# Теперь должно срабоать с первого раза и не кидать никакие варнинги
genie -s
systemctl enable docker
systemctl start docker
```

## SSH ключ для GitHub
```bash
# Подставить свой email, на все вопросы нажать enter
ssh-keygen -t ed25519 -C "email@email.com"
printf "Host github.com\n\tIdentityFile \"~/.ssh/id_ed25519\"" > .ssh/config

# Строчку, которая находится в этом файле надо добавить на GitHub как новый SSH ключ
cat .ssh/id_ed25519.pub
```


## Собрать сам проект
```bash  
mkdir kege && cd kege
# Можно сделать хак: хранить исходники на диске винды. Так Sublime будет нормально отображать файлы
# в левой панели. Но если этого не делать, то фронтенд будет собираться чуть быстрее.
ln -s /mnt/c/kege src
git clone git@github.com:DanShaders/kege-by-danshaders.git src
mkdir debug && cd debug
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ../src/api
ninja
cd ../src/config
docker-compose build
cd ../ui
JOBS=$(nproc) npm install
npm run build-diff-proto
npm run gen-proto
npm run build
```

## Как это запускать
```bash
# Части c докером нужно запускать из-под genie, т. е. команда для получения линуксовской консоли
# будет следующей:
wsl -- genie -s
# Чтобы не заморачиваться, лучше всегда работать из-под genie.
```

### Database & nginx
```bash
# В папке kege/src/config
docker-compose up

# Если нужно почистить базу
docker-compose down
docker-compose up

# Если поменял таблицы (kege/src/config/postgresql/db.sql)
docker-compose down
docker-compose build
docker-compose up
```

### C++ WebAPI
Я использую следующую команду в папке kege/debug
```bash
clear && ninja && KEGE_ROOT=../src/config ./KEGE
```
После того, как что-то меняшь в файлах на C++ надо в этом терминале ctrl+c и перезапустить
предыдущую команду. Она сама перекомпилирует всё, что поменялось и перезапустит.
Бэкенд нужно перезапускать всякий раз, когда перезапускалась БД, иначе будут предупреждения в
логах и какое-то время после перезапуска она будет отдавать 500 Internal Server Error (это
фиксится в теории и даже есть тикет в GH). Первый раз её также надо запускать после БД.

### UI
В папке kege/src/ui оставляешь запущённой следующее
```bash
npm run watch
```

Она почти всегда сама мониторит, когда меняются файлы и пересобирает всё.
Есть исключение: файлы из папки src/proto (которые описывают API сервера). Если менять их, то
нужно остановить предыдущую команду, запустить
```bash
npm run gen-proto
# и снова
npm run watch
```

Да, я и Ваня держим 3 окна терминала открытыми, когда этим всем занимаемся. Можно на винду установить Windows Terminal - современное приложение для них. Красивее и удобнее) И в нём можно переопределить команду на запуск wsl на wsl -- genie -s.