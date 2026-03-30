# Работа в командной строке

В данной директории расположены материалы и задания по лекции «Работа в командной строке»

 + [Конспект](./Работа%20в%20командной%20строке.html) лекции
 + [Текстовая версия](./Работа%20в%20командной%20строке,%20полный%20текст.html) лекции

## Самостоятельная работа

В качестве самостоятельной работы слушателям предлагается написать Shell-сценарий, который будет объединять в себе сразу несколько системных утилит.

1. Сценарий должен будет принимать аргументы командной строки. Это параметры, описываемые при вызове программы из командной строки. В самой программе обращение к параметрам осуществляется с помощью конструкции `$<N>`.
   
   Пример использования аргументов командной строки:

```console
[user@localhost] $ cat test.sh
echo $3 $2 $1  
[user@localhost] $ chmod +x test.sh
[user@localhost] $ ./test.sh First_arg 42 "Аргументы могут быть и текстовыми (из одного слова или множества слов в кавычках) и числовыми"
Аргументы могут быть и текстовыми (из одного слова или множества слов в кавычках) и числовыми 42 First_arg  
[user@localhost] $
```

2. Работа программы должна определяться значением первого параметра командной строки с помощью конструкции `case`
   
   Пример использования конструкции `case`:

```
case <выражение> in
	<шаблон 1>) <команды> ;;
	...
	<шаблон N>) <команды> ;;
	*) <команды> ;;
esac
```

`languages.sh`
```shell
echo -n "Enter the name of a country: "
read COUNTRY

echo -n "The official language of $COUNTRY is "

case $COUNTRY in

  Lithuania)
    echo -e "Lithuanian\n"
    ;;

  Romania | Moldova)
    echo -e "Romanian\n"
    ;;

  Italy | "San Marino" | Switzerland | "Vatican City")
    echo -e "Italian\n"
    ;;

  *)
    echo -e "unknown\n"
    ;;
esac
```

```console
[user@localhost] $ ./languages.sh
Enter the name of a country: Romania
The official language of Romania is Romanian

[user@localhost] $  ./languages.sh
Enter the name of a country: Russia
The official language of Russia is unknown

[user@localhost] $ 
```

### Задание

Напишите сценарий `multitask.sh`, который будет работать по следующим правилам:

 + При вызове `./multitask.sh repeat <N> <arg>`, необходимо N раз вывести на экран параметр `arg`

```console
[user@localhost] $ ./multitask.sh repeat 3 "Hello, Friends!"
Hello Friends
Hello Friends
Hello Friends
[user@localhost] $ 
```

 
 + При вызове `./multitask.sh today`, необходимо вывести календарь, оставив в нём заголовок и единственную строку с днями текущей недели:

```console
[user@localhost] $ ./multitask.sh today
     Март 2026        
Пн Вт Ср Чт Пт Сб Вс  
30 31
[user@localhost] $ 
```

	 + Первую часть можно вывести с помощью `head`
	 + Для второй части можно взять значение `date`, с помощью `cut -d' ' -f2` получить текущий день, а затем с помощью grep найти и вывести нужную неделю

 + При вызове `./multitask.sh create <Name>`, необходимо создать файл `/tmp/<Name>`, и заполнить его вводом с экрана 

```console
[user@localhost] $ ./multitask.sh create hello
Just a small text  
For checking  
  
Yep
[user@localhost] $ ls /tmp/hello
/tmp/hello
[user@localhost] $ cat /tmp/hello
Just a small text  
For checking  
  
Yep 
[user@localhost] $ 
```

 + При вызове `./multitask.sh check <N> <M>`, необходимо вывести результат сравнения двух чисел: `«<N> greater than <M>»`, `«<N> less than <M>»` или `«<N> and <M> are equal»` 

```console
[user@localhost] $ ./test.sh check 2 3
2 less than 3  
[user@localhost] $ ./test.sh check 3 2
3 greater than 2  
[user@localhost] $ ./test.sh check 4 4
4 and 4 are equal  
[user@localhost] $ 
```

Ваш сценарий должен корректно проходить все показанные примеры. Выполненные программы в виде текстовых файлов пришлите на почту преподавателя с указанием ФИО. Тема письма должна быть «\[LinuxPractice2\]<Фамилия> <Имя>»