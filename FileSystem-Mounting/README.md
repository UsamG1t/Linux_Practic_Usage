# Структура файловой системы

В данной директории расположены материалы и задания по лекции «Структура файловой системы»

 + [Конспект](./Структура%20файловой%20системы.md) лекции
 + [Текстовая версия](./Структура%20файловой%20системы,%20полный%20текст.md) лекции

## Самостоятельная работа

В рамках самостоятельной работы слушателям предлагается самостоятельно исследовать вопрос подключения сетевой файловой системы. Для этого необходимо создать две ВМ, объединить в сеть и настроить соответствующие сервисы.

#### Подготовка стенда. 

Работаем с [ВМ для лабораторных работ](https://github.com/UsamG1t/Nets_ASVK_Labs/blob/master/01_FirstStart/%D0%9D%D0%B0%D1%81%D1%82%D1%80%D0%BE%D0%B9%D0%BA%D0%B0%20%D1%81%D0%B8%D1%81%D1%82%D0%B5%D0%BC%D1%8B%20%D0%B4%D0%BB%D1%8F%20%D0%B2%D1%8B%D0%BF%D0%BE%D0%BB%D0%BD%D0%B5%D0%BD%D0%B8%D1%8F%20%D0%BB%D0%B0%D0%B1%D0%BE%D1%80%D0%B0%D1%82%D0%BE%D1%80%D0%BD%D1%8B%D1%85.md) по сетевым технологиям
 + Так просто проще настраивать всё
	 + С точностью до правильных интерфейсов можно повторить с любым образом
 + Есть удобные способы управления и взаимодействия с ВМ
	 + [Shell-сценарии](https://github.com/FrBrGeorge/vbsnap) (Linux only)
	 + Python-пакет [PySnap](https://github.com/usamg1t/neuropysnap) (Кроссплатформенный!)

```console
[user@localhost: ~] $  cd /tmp
[user@localhost: /tmp] $  python3 -m venv venv
[user@localhost: /tmp] $  source venv/bin/activate
[user@localhost: /tmp (venv)] $  pip install ~/Downloads/pysnap-0.1.1-py3-none-any.whl

Processing /home/papillon_rouge/Downloads/pysnap-0.1.1-py3-none-any.whl  
<…>
Successfully installed prompt_toolkit-3.0.52 pysnap-0.1.1 pyte-0.8.2 wcwidth-0.6.0

[user@localhost: /tmp (venv)] $ 
```

```console
[user@localhost: /tmp (venv)] $ pysnap import /path/to/your/image.ova BaseName
[user@localhost: /tmp (venv)] $ pysnap protosettings BaseName
[user@localhost: /tmp (venv)] $ pysnap clone Basename PC1 intnet
[user@localhost: /tmp (venv)] $ pysnap clone BaseName PC2 intnet
```

```console
[user@localhost: /tmp (venv)] $ pysnap connect PC1
```

```console
[user@localhost: /tmp (venv)] $ pysnap connect PC2
```

После подключения и ввода логина и пароля **сразу** необходимо будет включить [запись отчёта](https://github.com/UsamG1t/Nets_ASVK_Labs/blob/master/02_SystemGreetings/%D0%97%D0%BD%D0%B0%D0%BA%D0%BE%D0%BC%D1%81%D1%82%D0%B2%D0%BE%20%D1%81%20%D1%81%D0%B8%D1%81%D1%82%D0%B5%D0%BC%D0%BE%D0%B9.md#%D1%81%D0%B4%D0%B0%D1%87%D0%B0-%D1%81%D0%B0%D0%BC%D0%BE%D1%81%D1%82%D0%BE%D1%8F%D1%82%D0%B5%D0%BB%D1%8C%D0%BD%D1%8B%D1%85-%D1%80%D0%B0%D0%B1%D0%BE%D1%82) на обеих ВМ. 

На каждой из ВМ необходимо задать IP-адрес и включить интерфейс:

`@PC1`
```console
[root@PC1 ~]# ip link set eth1 up
[root@PC1 ~]# ip addr add dev eth1 10.0.10.1/24
[root@PC1 ~]#
```


`@PC2`
```console
[root@PC2 ~]# ip link set eth1 up
[root@PC2 ~]# ip addr add dev eth1 10.0.10.2/24
[root@PC2 ~]#
```

Кроме настройки локального соединения необходимо скачать специальные утилиты для настройки сетевой файловой системы. Для этого необходимо подключить ВМ к глобальной сети и загрузить установочные пакеты.

1. Выполните на обеих ВМ DHCP-настройку интерфейса eth0: 

`@PC1`
```console
[root@PC1 ~]# dhcpcd eth0
[root@PC1 ~]#
```

`@PC2`
```console
[root@PC2 ~]# dhcpcd eth0
[root@PC2 ~]#
```

2. Для установки пакетов необходимо описать пути к репозиториям пакетов в сети. Данные о репозиториях пишутся в `/etc/apt/sources.list.d/alt.list`. В образах ВМ уже описаны некоторые пути, но для нашей задачи они неактуальны, их можно игнорировать.

`@PC1:/etc/apt/sources.list.d/alt.list`
```console
rpm [alt] http://ftp.altlinux.org/pub/distributions/ALTLinux/Sisyphus x86_64 debuginfo
rpm [alt] http://ftp.altlinux.org/pub/distributions/ALTLinux/Sisyphus x86_64 classic
rpm [alt] http://ftp.altlinux.org/pub/distributions/ALTLinux/Sisyphus noarch classic
```

`@PC2:/etc/apt/sources.list.d/alt.list`
```console
rpm [alt] http://ftp.altlinux.org/pub/distributions/ALTLinux/Sisyphus x86_64 debuginfo
rpm [alt] http://ftp.altlinux.org/pub/distributions/ALTLinux/Sisyphus x86_64 classic
rpm [alt] http://ftp.altlinux.org/pub/distributions/ALTLinux/Sisyphus noarch classic
```

После описания пути к репозиториям для установки необходимых утилит выполните следующие команды:

`@PC1`
```console
[root@PC1 ~]# apt-get update
[root@PC1 ~]# apt-get install nfs-server
[root@PC1 ~]#
```

`@PC2`
```console
[root@PC2 ~]# apt-get update
[root@PC2 ~]# apt-get install nfs-clients
[root@PC2 ~]#
```

#### Настройка NFS-сервера

Для настройки NFS-сервера, с которого будет раздаваться NFS, необходимо:

1. Создать экспортируемый каталог. Для простоты настройки задать ему права 0777.

```console
[root@PC1 ~]# mkdir -p /srv/nfs/share
[root@PC1 ~]# chmod 0777 /srv/nfs/share
[root@PC1 ~]#
```

2. Настроить `/etc/exports` для описания настроек NFS.


```console
[root@PC1 ~]# echo "/srv/nfs/share 10.0.10.0/24(rw,sync,no_subtree_check)" >> /etc/exports
[root@PC1 ~]#
```

3. Запустить службы NFS

```console
[root@PC1 ~]# systemctl enable --now rpcbind 
[root@PC1 ~]# systemctl enable --now nfs-server
[root@PC1 ~]# exportfs -rav
[root@PC1 ~]#
```

#### Настройка NFS-клиента

Для настройки NFS-клиента и подключения NFS необходимо:

1. Создать точку монтирования

```console
[root@PC2 ~]# mkdir -p /mnt/nfs-share
```

2. Смонтировать NFS-каталог

```console
[root@PC2 ~]# mount -t nfs 10.0.10.1:/srv/nfs/share /mnt/nfs-share
```

#### Проверка монтирования и автомонтирование

Для проверки монтирования запустите:

```console
[root@PC2 ~]# findmnt /mnt/nfs-share
[root@PC2 ~]# df -h /mnt/nfs-share
```
Затем добавьте в директорию файл и убедитесь, что он сохранился на другой ВМ:

```console
# ls -la /mnt/nfs-share
# cat /mnt/nfs-share/server.txt
[root@PC2 ~]# echo "file from client" > /mnt/nfs-share/client.txt
[root@PC2 ~]# ls -la /mnt/nfs-share
```

```console
[root@PC1 ~]# ls -la /srv/nfs/share
[root@PC1 ~]# cat /srv/nfs/share/client.txt
[root@PC1 ~]# echo "file from server" > /srv/nfs/share/server.txt
[root@PC1 ~]# ls -la /srv/nfs/share
```


```console
[root@PC2 ~]# ls -la /mnt/nfs-share
[root@PC2 ~]# cat /mnt/nfs-share/server.txt
```

Для автомонтирования файловой системы аналогично /proc или /tmp добавьте описание NFS в `/etc/fstab`:

```console
[root@PC2 ~]# echo "10.0.10.1:/srv/nfs/share /mnt/nfs-share nfs defaults,_netdev,vers=4 0 0" >> /etc/fstab
[root@PC2 ~]#
```

Проверьте запись:

```console
[root@PC2 ~]# umount /mnt/nfs-share
[root@PC2 ~]# mount -a
[root@PC2 ~]# findmnt /mnt/nfs-share
```

Завершите отчёты с помощью `Ctrl+D`. Полученные отчёты `report.01.pc1` и `report.01.pc2` пришлите на почту преподавателя с указанием ФИО. Тема письма должна быть «\[LinuxPractice4\]<Фамилия> <Имя>»
