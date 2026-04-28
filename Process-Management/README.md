# Управление процессами

В данной директории расположены материалы и задания по лекции «Управление процессами»

 + [Конспект](./Управление%20процессами.md) лекции
 + [Текстовая версия](./Управление%20процессами,%20полный%20текст.md) лекции

## Самостоятельная работа
## Самостоятельная работа

В рамках самостоятельной работы слушателям предлагается сыграть в процессную головоломку: 

В виртуальной машине запущено «зависшее приложение». Оно создало большое дерево процессов с неочевидными именами. Большинство процессов фиктивные: они почти не нагружают систему и просто ожидают сигналов. Внутри дерева спрятаны несколько специальных процессов:

- процесс, который не завершается по обычному `SIGTERM`;
- остановленный процесс;
- зомби-процесс;
- процесс, который держит открытым удалённый файл;
- процессы, у которых есть секретная метка: в аргументах, окружении, имени процесса или открытых файлах.

Секретная метка — `LinuxPracticUsage`

Для решения головоломки необходимо предварительно настроить виртуальную машину.

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
[user@localhost: /tmp (venv)] $ pysnap clone Basename Proc_maze
[user@localhost: /tmp (venv)] $ pysnap connect Proc_maze
```

1. Выполните DHCP-настройку интерфейса eth0: 

```console
[root@Proc_maze ~]# dhcpcd eth0
[root@Proc_maze ~]#
```
2. С помощью утилиты `wget` скачайте файлы запуска головоломки

```console
[root@Proc_maze ~]# wget https://github.com/UsamG1t/Linux_Practic_Usage/blob/master/Process-Management/Attached_materials/proc_maze_check
[root@Proc_maze ~]# wget https://github.com/UsamG1t/Linux_Practic_Usage/blob/master/Process-Management/Attached_materials/proc_maze_stand
[root@Proc_maze ~]# wget https://github.com/UsamG1t/Linux_Practic_Usage/blob/master/Process-Management/Attached_materials/proc_maze_cleanup.sh
```

#### Управление головоломкой


**Ваша цель** — найти все специальные процессы и завершить/запустить их с минимальным количеством завершений всех остальных процессов. 

Пользоваться поисковиком для нахождения интересных способов исследования _приветствуется_. Писать shell-сценарии для автопроверки процессов — _очень приветствуется_. Ниже собраны группы команд, которые могут помочь в решении головоломки.

Перед выполнением головоломки необходимо включить [запись отчёта](https://github.com/UsamG1t/Nets_ASVK_Labs/blob/master/02_SystemGreetings/%D0%97%D0%BD%D0%B0%D0%BA%D0%BE%D0%BC%D1%81%D1%82%D0%B2%D0%BE%20%D1%81%20%D1%81%D0%B8%D1%81%D1%82%D0%B5%D0%BC%D0%BE%D0%B9.md#%D1%81%D0%B4%D0%B0%D1%87%D0%B0-%D1%81%D0%B0%D0%BC%D0%BE%D1%81%D1%82%D0%BE%D1%8F%D1%82%D0%B5%D0%BB%D1%8C%D0%BD%D1%8B%D1%85-%D1%80%D0%B0%D0%B1%D0%BE%D1%82). 

Для старта запустите программу `proc_maze_stand` в фоновом режиме. 

```console
[root@Proc_maze ~]# ./proc_maze_stand &
[root@Proc_maze ~]#
```

В процессе вы можете проверять количество оставшихся процессов с помощью `proc_maze_check`

```console
[root@Proc_maze ~]# ./proc_maze_check
[root@Proc_maze ~]#
```

Для досрочного завершения головоломки воспользуйтесь shell-сценарием очистки (Отчёты с неполным решением и досрочным завершением также будут положительно оцениваться):

```console
[root@Proc_maze ~]# ./proc_maze_cleanup.sh
[root@Proc_maze ~]#
```

Полученные отчёт report.01.proc_maze пришлите на почту преподавателя с указанием ФИО. Тема письма должна быть «[LinuxPractice3]<Фамилия> <Имя>»

#### Полезные команды

```bash
ps -eo pid,ppid,pgid,sid,tty,stat,comm,args -p <PID>
pstree -p

ps -eo pid,ppid,stat,comm,args | grep <слово>
cat /proc/<PID>/status

ls -l /proc/<PID>/fd
readlink /proc/<PID>/fd/* 2>/dev/null

kill -TERM <PID>
kill -STOP <PID>
kill -CONT <PID>
kill -KILL <PID>

tr '\0' ' ' < /proc/<PID>/cmdline; echo
tr '\0' '\n' < /proc/<PID>/environ; echo
```

Не забывайте про shell-конструкции, например: 

```console
for pid in `ls /proc`; do 
	ls -la /proc/$pid/fd 2> /dev/null && echo "MINE $pid";
done | grep -E "Linux|MINE"
```
